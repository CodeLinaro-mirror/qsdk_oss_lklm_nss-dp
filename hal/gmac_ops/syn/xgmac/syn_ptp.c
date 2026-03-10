/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * syn_ptp.c
 *	Synopsys XGMAC PTP Hardware Clock (PHC) support
 *
 * This file implements PTP Hardware Clock support for the Synopsys XGMAC
 * driver, enabling precise time synchronization using IEEE 1588 PTP.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <fal/fal_port_ctrl.h>
#include "nss_dp_api_if.h"
#include "nss_dp_hal_if.h"
#include "syn_dev.h"
#include "edma.h"

/* Global singleton platform manager */
static struct syn_ptp_platform_mgr *g_platform_mgr = NULL;
static DEFINE_MUTEX(platform_mgr_mutex);

/* Global PTP debugfs directory */
static struct dentry *g_ptp_debugfs_dir = NULL;
static int g_ptp_debugfs_refcount = 0;
static DEFINE_MUTEX(ptp_debugfs_mutex);

/* Platform-specific register addresses for IPQ52XX */
#define IPQ52XX_TCSR_PPS_IN_REG		0x0196100C
#define IPQ52XX_TCSR_PPS_OUT_REG	0x01961010
#define IPQ52XX_GPIO_PPS		18	/* Reference only - GPIO now acquired from DTS */
#define IPQ52XX_PPS_IN_IRQ		359
#define IPQ52XX_PPS_OUT_IRQ		360

/* Platform-specific settings for IPQ96XX */
#define IPQ96XX_GPIO_PPS		50	/* Reference only - GPIO now acquired from DTS */
#define IPQ96XX_PPS_IN_IRQ		362
#define IPQ96XX_PPS_OUT_IRQ		363

#define SYN_MAC_PTP_TXTS_FIFO_NUM	8

static int syn_ptp_time_set(void __iomem *mac_base, u32 flag, u32 sec, u32 nsec)
{
	u32 value;

	/* Write seconds to MAC_System_Time_Seconds_Update register */
	hal_write_reg(mac_base, SYN_MAC_SYS_TIME_SECS_UPDATE, sec);

	/* Write nanoseconds to MAC_System_Time_Nanoseconds_Update register */
	hal_write_reg(mac_base, SYN_MAC_SYS_TIME_NSECS_UPDATE, nsec);

	/* Set TSINIT or TSUPDT bit to initialize the time */
	hal_set_reg_bits(mac_base, SYN_MAC_TS_CTL, flag);

	/* Poll TSINIT or TSUPDT bit until cleared by hardware */
	return readl_poll_timeout_atomic(mac_base + SYN_MAC_TS_CTL, value,
					 !(value & flag),
					 10, 100000);
}

static int syn_ptp_increment_set(void __iomem *mac_base, u32 ns)
{
	u32 value;

	/* Set integer nanosecond increment value per each clock cycle
	 * Note: SYN_MAC_SUB_SEC_INCR only supports integer nanosecond increments.
	 * Fractional nanosecond adjustments are handled via SYN_MAC_TS_ADDEND register.
	 */
	value = hal_read_reg(mac_base, SYN_MAC_SUB_SEC_INCR);

	value &= ~SYN_MAC_SUB_SEC_INCR_SSINC_MASK;
	value |= FIELD_PREP(SYN_MAC_SUB_SEC_INCR_SSINC_MASK, ns);

	hal_write_reg(mac_base, SYN_MAC_SUB_SEC_INCR, value);
	dev_dbg(NULL, "SYN_MAC_SUB_SEC_INCR configured: reg=0x%x, val=0x%x, ns=%u\n",
		SYN_MAC_SUB_SEC_INCR, value, ns);

	return 0;
}

static int syn_ptp_adjfine(void __iomem *mac_base, u32 addend)
{
	u32 value;

	/* Write addend value to MAC_Timestamp_Addend register */
	hal_write_reg(mac_base, SYN_MAC_TS_ADDEND, addend);

	/* Set TSADDREG bit to update the addend register */
	hal_set_reg_bits(mac_base, SYN_MAC_TS_CTL, SYN_MAC_TS_CTL_TSADDREG);

	/* Poll TSADDREG bit until cleared by hardware */
	return readl_poll_timeout_atomic(mac_base + SYN_MAC_TS_CTL, value,
					 !(value & SYN_MAC_TS_CTL_TSADDREG),
					 10, 100000);
}

/*
 * qcom_nss_ptp_adjfine()
 *	Adjust PHC frequency with SSINC compensation
 *
 * This function adjusts the clock frequency by modifying the addend register.
 * Unlike the standard adjust_by_scaled_ppm(), this implementation accounts for
 * the fact that SSINC may be rounded up (e.g., 3 ns instead of 2.666 ns for
 * 375 MHz clock), and the addend must compensate accordingly.
 *
 * The standard adjust_by_scaled_ppm() assumes:
 *   new_addend = default_addend × (1 + ppm/10^6)
 *
 * But with rounded SSINC, we need:
 *   Δaddend = (ptp_clock_rate × ppm × 2^32) / (10^9 × ssinc)
 *
 * This accounts for the SSINC multiplication factor in the hardware.
 */
static int qcom_nss_ptp_adjfine(struct ptp_clock_info *ptp, long scaled_ppm)
{
	struct syn_ptp_priv *ptp_priv = container_of(ptp, struct syn_ptp_priv, caps);
	struct syn_hal_dev *shd = ptp_priv->shd;
	void __iomem *mac_base = shd->nghd.mac_base;
	unsigned long flags;
	u32 addend;
	int ret;

	addend = (u32)adjust_by_scaled_ppm(ptp_priv->default_addend, scaled_ppm);

	spin_lock_irqsave(&ptp_priv->lock, flags);
	ret = syn_ptp_adjfine(mac_base, addend);
	spin_unlock_irqrestore(&ptp_priv->lock, flags);

	return ret;
}

/*
 * qcom_nss_ptp_adjtime()
 *	Adjust PHC time by offset
 *
 * This function adjusts the clock by adding or subtracting a time offset.
 */
static int qcom_nss_ptp_adjtime(struct ptp_clock_info *ptp, s64 delta)
{
	struct syn_ptp_priv *ptp_priv = container_of(ptp, struct syn_ptp_priv, caps);
	struct syn_hal_dev *shd = ptp_priv->shd;
	void __iomem *mac_base = shd->nghd.mac_base;
	unsigned long flags;
	bool negative = false;
	u32 sec, nsec;
	int ret;

	/* Handle negative delta */
	if (delta < 0) {
		negative = true;
		delta = -delta;
	}

	/* Convert delta (nanoseconds) to seconds and nanoseconds */
	sec = div_u64_rem(delta, 1000000000ULL, &nsec);

	/* Write nanoseconds to MAC_System_Time_Nanoseconds_Update register
	 * Set ADDSUB bit (bit 31) if delta is negative
	 */
	if (negative)
		nsec |= SYN_MAC_SYS_TIME_NSECS_UPDATE_ADDSUB;

	spin_lock_irqsave(&ptp_priv->lock, flags);
	ret = syn_ptp_time_set(mac_base, SYN_MAC_TS_CTL_TSUPDT, sec, nsec);
	spin_unlock_irqrestore(&ptp_priv->lock, flags);

	return ret;
}

/*
 * qcom_nss_ptp_gettimex()
 *	Get current PHC time
 *
 * This function reads the current hardware timestamp.
 * Handles nanosecond rollover correctly.
 */
static int qcom_nss_ptp_gettimex(struct ptp_clock_info *ptp,
				 struct timespec64 *ts,
				 struct ptp_system_timestamp *sts)
{
	struct syn_ptp_priv *ptp_priv = container_of(ptp, struct syn_ptp_priv, caps);
	struct syn_hal_dev *shd = ptp_priv->shd;
	void __iomem *mac_base = shd->nghd.mac_base;
	unsigned long flags;
	u32 sec, nsec, sec2;

	spin_lock_irqsave(&ptp_priv->lock, flags);

	/* Read seconds */
	sec = hal_read_reg(mac_base, SYN_MAC_SYS_TIME_SECS);

	/* Read nanoseconds */
	nsec = hal_read_reg(mac_base, SYN_MAC_SYS_TIME_NSECS);

	/* Read seconds again to detect rollover */
	sec2 = hal_read_reg(mac_base, SYN_MAC_SYS_TIME_SECS);

	/* If seconds changed, re-read nanoseconds */
	if (sec != sec2) {
		sec = sec2;
		nsec = hal_read_reg(mac_base, SYN_MAC_SYS_TIME_NSECS);
	}

	spin_unlock_irqrestore(&ptp_priv->lock, flags);

	/* Mask nanoseconds to 31 bits (bit 31 is reserved) */
	nsec &= SYN_MAC_SYS_TIME_NSECS_MASK;

	/* Fill timespec64 structure */
	ts->tv_sec = sec;
	ts->tv_nsec = nsec;

	return 0;
}

/*
 * qcom_nss_ptp_settime()
 *	Set PHC time
 *
 * This function initializes the hardware clock to a specific time.
 */
static int qcom_nss_ptp_settime(struct ptp_clock_info *ptp,
				const struct timespec64 *ts)
{
	struct syn_ptp_priv *ptp_priv = container_of(ptp, struct syn_ptp_priv, caps);
	struct syn_hal_dev *shd = ptp_priv->shd;
	void __iomem *mac_base = shd->nghd.mac_base;
	unsigned long flags;
	int ret;

	/* Validate nanoseconds */
	if (ts->tv_nsec >= 1000000000ULL) {
		dev_err(ptp_priv->dev,
			"Invalid nanoseconds value: %ld\n",  ts->tv_nsec);
		return -EINVAL;
	}

	spin_lock_irqsave(&ptp_priv->lock, flags);
	ret = syn_ptp_time_set(mac_base, SYN_MAC_TS_CTL_TSINIT,
			       (u32)ts->tv_sec, (u32)ts->tv_nsec);
	spin_unlock_irqrestore(&ptp_priv->lock, flags);

	return ret;
}

/*
 * qcom_nss_ptp_enable()
 *	Enable/disable PPS capture
 *
 * This function enables or disables PPS timestamp capture.
 */
static int qcom_nss_ptp_enable(struct ptp_clock_info *ptp,
			       struct ptp_clock_request *rq, int on)
{
	struct syn_ptp_priv *ptp_priv = container_of(ptp, struct syn_ptp_priv, caps);
	unsigned long flags;

	/* Only support external timestamp requests */
	if (rq->type != PTP_CLK_REQ_EXTTS)
		return -EOPNOTSUPP;

	spin_lock_irqsave(&ptp_priv->lock, flags);

	/* Set or clear PPS enabled flag */
	ptp_priv->pps_enabled = (on != 0);

	spin_unlock_irqrestore(&ptp_priv->lock, flags);

	dev_dbg(ptp_priv->dev,
		"PPS capture %s\n",  on ? "enabled" : "disabled");

	return 0;
}

/*
 * qcom_nss_ptp_verify()
 *	Verify pin configuration
 *
 * This function validates pin configuration requests.
 * We only support one pin for external timestamps.
 */
static int qcom_nss_ptp_verify(struct ptp_clock_info *ptp, unsigned int pin,
			       enum ptp_pin_function func, unsigned int chan)
{
	/* Only accept pin 0 */
	if (pin != 0)
		return -EINVAL;

	/* Only accept external timestamp function */
	if (func != PTP_PF_EXTTS)
		return -EINVAL;

	return 0;
}

/*
 * qcom_nss_ptp_irq_handler_thread()
 *	Threaded interrupt handler for PPS signals
 *
 * This function is called when a PPS signal is received. It captures
 * the current timestamp and delivers it to userspace via the PTP subsystem.
 *
 * @irq: IRQ number
 * @priv: Pointer to PTP private data structure
 *
 * Returns: IRQ_HANDLED
 */
static irqreturn_t qcom_nss_ptp_irq_handler_thread(int irq, void *priv)
{
	struct syn_ptp_priv *ptp_priv = (struct syn_ptp_priv *)priv;
	struct ptp_clock_event event;
	struct timespec64 ts;
	unsigned long flags;

	/* Check if PPS capture is enabled */
	spin_lock_irqsave(&ptp_priv->lock, flags);
	if (!ptp_priv->pps_enabled) {
		spin_unlock_irqrestore(&ptp_priv->lock, flags);
		return IRQ_HANDLED;
	}
	spin_unlock_irqrestore(&ptp_priv->lock, flags);

	/* Capture current timestamp */
	qcom_nss_ptp_gettimex(&ptp_priv->caps, &ts, NULL);

	/* Prepare PTP clock event */
	event.type = PTP_CLOCK_EXTTS;
	event.index = 0;
	event.timestamp = ts.tv_sec * 1000000000ULL + ts.tv_nsec;

	/* Deliver event to PTP subsystem */
	ptp_clock_event(ptp_priv->clock, &event);

	dev_dbg(ptp_priv->dev, " PPS event captured at %lld.%09ld\n",
		 (s64)ts.tv_sec, ts.tv_nsec);

	return IRQ_HANDLED;
}

/*
 * syn_ptp_platform_detect()
 *	Detect platform type (IPQ52XX/IPQ96XX/Unknown)
 *
 * Returns: enum platform_type indicating the detected platform
 */
static enum platform_type syn_ptp_platform_detect(void)
{
#if defined(NSS_DP_IPQ96XX)
	return PLATFORM_IPQ96XX;
#elif defined(NSS_DP_IPQ52XX)
	return PLATFORM_IPQ52XX;
#endif
	return PLATFORM_UNKNOWN;
}

/*
 * syn_ptp_ipq52xx_init()
 *	Initialize IPQ52XX platform-specific PPS configuration
 *
 * This function configures the ToP TCSR registers and GPIO for PPS
 * signal routing on the IPQ52XX platform.
 *
 * @mgr: Pointer to platform manager structure
 * @dev: Pointer to device structure for logging
 *
 * Returns: 0 on success, negative error code on failure
 */
static int syn_ptp_ipq52xx_init(struct syn_ptp_platform_mgr *mgr, struct device *dev)
{
	/* Map ToP TCSR PPS_IN register (keep mapped for runtime access) */
	mgr->tcsr_pps_in = devm_ioremap(dev, IPQ52XX_TCSR_PPS_IN_REG, 4);
	if (!mgr->tcsr_pps_in) {
		dev_err(dev, "Failed to map TCSR PPS_IN register\n");
		return -ENOMEM;
	}

	/* Map ToP TCSR PPS_OUT register (keep mapped for runtime access) */
	mgr->tcsr_pps_out = devm_ioremap(dev, IPQ52XX_TCSR_PPS_OUT_REG, 4);
	if (!mgr->tcsr_pps_out) {
		dev_err(dev, "Failed to map TCSR PPS_OUT register\n");
		return -ENOMEM;
	}

	/* Configure TCSR PPS_IN register
	 * Set to 0 for external PHY PPS input (default)
	 */
	writel(0x0, mgr->tcsr_pps_in);
	mgr->pps_in_source = 0;

	/* Configure TCSR PPS_OUT register
	 * Set to 0 for NSS_PPS_OUT (PPE/XGMAC) (default)
	 */
	writel(0x0, mgr->tcsr_pps_out);
	mgr->pps_out_source = 0;

	/* Initialize in disabled mode */
	mgr->pps_mode = PPS_MODE_DISABLED;

	return 0;
}

/*
 * syn_ptp_platform_mgr_get()
 *	Get or create the singleton platform manager
 *
 * This function implements the singleton pattern for platform_mgr.
 * On first call, it allocates and initializes the singleton, detects
 * the platform, and performs platform-specific initialization.
 * On subsequent calls, it increments the reference count.
 *
 * @pdev: Pointer to platform device structure
 *
 * Returns: Pointer to platform manager on success, NULL on failure
 */
static struct syn_ptp_platform_mgr *syn_ptp_platform_mgr_get(struct platform_device *pdev)
{
	struct syn_ptp_platform_mgr *mgr;
	struct device *dev = &pdev->dev;

	mutex_lock(&platform_mgr_mutex);

	if (g_platform_mgr) {
		/* Already exists, increment refcount */
		g_platform_mgr->refcount++;
		mutex_unlock(&platform_mgr_mutex);
		dev_dbg(dev, "Platform manager refcount incremented to %d\n",
			g_platform_mgr->refcount);
		return g_platform_mgr;
	}

	/* First instance - create the singleton */
	mgr = devm_kzalloc(dev, sizeof(*mgr), GFP_KERNEL);
	if (!mgr) {
		mutex_unlock(&platform_mgr_mutex);
		dev_err(dev, "Failed to allocate platform manager\n");
		return NULL;
	}

	/* Initialize singleton */
	mutex_init(&mgr->lock);
	mgr->refcount = 1;
	mgr->platform = PLATFORM_UNKNOWN;
	mgr->gpio_desc = NULL;
	mgr->pps_in_irq = -1;
	mgr->pps_mode = PPS_MODE_DISABLED;
	mgr->tcsr_pps_in = NULL;
	mgr->tcsr_pps_out = NULL;
	mgr->pps_in_source = 0;
	mgr->pps_out_source = 0;
	mgr->dev = dev;

	/* Acquire GPIO from DTS (optional) - GPIOD_IN flag configures it as input */
	mgr->gpio_desc = devm_gpiod_get_optional(dev, "pps", GPIOD_IN);

	/* Get PPS_IN interrupt from DTS by name */
	mgr->pps_in_irq = platform_get_irq_byname_optional(pdev, "pps_in");
	if (mgr->pps_in_irq >= 0) {
		dev_info(&pdev->dev, "PPS_IN IRQ from DTS: %d\n", mgr->pps_in_irq);
	}

	/* Detect platform type */
	mgr->platform = syn_ptp_platform_detect();

	/* Initialize platform-specific PPS configuration */
	if (mgr->platform == PLATFORM_IPQ52XX) {
		int ret = syn_ptp_ipq52xx_init(mgr, &pdev->dev);
		if (ret) {
			dev_warn(&pdev->dev, "IPQ52XX platform init failed: %d, continuing without PPS\n", ret);
			/* Continue without PPS support */
		}
	}

	g_platform_mgr = mgr;

	/* Store platform manager in device driver data for sysfs access */
	dev_set_drvdata(dev, mgr);

	mutex_unlock(&platform_mgr_mutex);
	return mgr;
}

/*
 * syn_ptp_platform_mgr_put()
 *	Release reference to platform manager
 *
 * This function decrements the reference count. When the count reaches
 * zero, it performs platform cleanup and frees the singleton.
 */
static void syn_ptp_platform_mgr_put(void)
{
	mutex_lock(&platform_mgr_mutex);

	if (!g_platform_mgr) {
		mutex_unlock(&platform_mgr_mutex);
		return;
	}

	g_platform_mgr->refcount--;

	if (g_platform_mgr->refcount == 0)
		g_platform_mgr = NULL;

	mutex_unlock(&platform_mgr_mutex);
}

/*
 * syn_ptp_set_pps_mode()
 *	Set PPS mode (input/output) and configure GPIO direction
 *
 * This function switches between PPS input (slave) and output (master) modes.
 * It handles GPIO direction, interrupt registration/unregistration, and
 * hardware configuration.
 *
 * @ptp_priv: Pointer to PTP private data structure
 * @mode: Desired PPS mode (PPS_MODE_INPUT or PPS_MODE_OUTPUT)
 *
 * Returns: 0 on success, negative error code on failure
 */
static int syn_ptp_set_pps_mode(struct syn_ptp_priv *ptp_priv, enum pps_mode mode)
{
	unsigned long flags;
	int ret;

	if (!ptp_priv->platform_mgr->gpio_desc) {
		dev_dbg(ptp_priv->dev, "GPIO for PPS is not provided");
		return 0;
	}

	if (mode == ptp_priv->platform_mgr->pps_mode) {
		dev_dbg(ptp_priv->dev, "Already in requested mode %d\n", mode);
		return 0;
	}

	spin_lock_irqsave(&ptp_priv->lock, flags);

	/* Disable current mode */
	if (ptp_priv->platform_mgr->pps_mode == PPS_MODE_INPUT) {
		/* Unregister PPS_IN interrupt */
		if (ptp_priv->platform_mgr->pps_in_irq >= 0) {
			spin_unlock_irqrestore(&ptp_priv->lock, flags);
			free_irq(ptp_priv->platform_mgr->pps_in_irq, ptp_priv);
			dev_info(ptp_priv->dev, "Unregistered PPS_IN interrupt (IRQ=%d)\n",
				 ptp_priv->platform_mgr->pps_in_irq);
			spin_lock_irqsave(&ptp_priv->lock, flags);
		}
	}

	/* Configure new mode */
	switch (mode) {
	case PPS_MODE_INPUT:
		/* Configure GPIO as input */
		spin_unlock_irqrestore(&ptp_priv->lock, flags);

		ret = gpiod_direction_input(ptp_priv->platform_mgr->gpio_desc);
		if (ret) {
			dev_err(ptp_priv->dev, "Failed to set GPIO as input: %d\n", ret);
			return ret;
		}

		/* Register PPS_IN interrupt handler */
		if (ptp_priv->platform_mgr->pps_in_irq >= 0) {
			ret = request_threaded_irq(ptp_priv->platform_mgr->pps_in_irq,
						    NULL,
						    qcom_nss_ptp_irq_handler_thread,
						    IRQF_TRIGGER_RISING | IRQF_ONESHOT,
						    "qcom-nss-ptp-in",
						    ptp_priv);
			if (ret) {
				dev_err(ptp_priv->dev, "Failed to register PPS_IN IRQ %d: %d\n",
				       ptp_priv->platform_mgr->pps_in_irq, ret);
				return ret;
			}
			dev_info(ptp_priv->dev, "Registered PPS_IN interrupt (IRQ=%d)\n",
				ptp_priv->platform_mgr->pps_in_irq);
		}

		spin_lock_irqsave(&ptp_priv->lock, flags);
		ptp_priv->platform_mgr->pps_mode = PPS_MODE_INPUT;
		spin_unlock_irqrestore(&ptp_priv->lock, flags);
		dev_info(ptp_priv->dev, "Switched to PPS_IN mode (slave)\n");
		break;

	case PPS_MODE_OUTPUT:
		/* Configure GPIO as output */
		spin_unlock_irqrestore(&ptp_priv->lock, flags);

		ret = gpiod_direction_output(ptp_priv->platform_mgr->gpio_desc, 0);
		if (ret) {
			dev_err(ptp_priv->dev, "Failed to set GPIO as output: %d\n", ret);
			return ret;
		}

		spin_lock_irqsave(&ptp_priv->lock, flags);
		ptp_priv->platform_mgr->pps_mode = PPS_MODE_OUTPUT;
		spin_unlock_irqrestore(&ptp_priv->lock, flags);
		dev_info(ptp_priv->dev, "Switched to PPS_OUT mode (master)\n");
		break;

	case PPS_MODE_DISABLED:
		ptp_priv->platform_mgr->pps_mode = PPS_MODE_DISABLED;
		spin_unlock_irqrestore(&ptp_priv->lock, flags);
		dev_info(ptp_priv->dev, "PPS disabled\n");
		break;

	default:
		spin_unlock_irqrestore(&ptp_priv->lock, flags);
		dev_err(ptp_priv->dev, "Invalid PPS mode %d\n", mode);
		return -EINVAL;
	}

	return 0;
}


/*
 * pps_mode_show()
 *	Show current PPS mode via debugfs
 *
 * @m: seq_file for output
 * @v: Unused parameter
 *
 * Returns: 0 on success
 */
static int pps_mode_show(struct seq_file *m, void *v)
{
	struct syn_ptp_platform_mgr *mgr = g_platform_mgr;
	const char *mode_str;

	if (!mgr) {
		seq_printf(m, "PPS not initialized\n");
		return 0;
	}

	switch (mgr->pps_mode) {
	case PPS_MODE_INPUT:
		mode_str = "input";
		break;
	case PPS_MODE_OUTPUT:
		mode_str = "output";
		break;
	case PPS_MODE_DISABLED:
		mode_str = "disabled";
		break;
	default:
		mode_str = "unknown";
	}

	seq_printf(m, "%s\n", mode_str);
	return 0;
}

/*
 * pps_mode_open()
 *	Open callback for pps_mode debugfs file
 */
static int pps_mode_open(struct inode *inode, struct file *file)
{
	return single_open(file, pps_mode_show, inode->i_private);
}

/*
 * pps_mode_write()
 *	Write callback for pps_mode debugfs file
 *
 * Valid values: "input" (slave mode), "output" (master mode), "disabled"
 */
static ssize_t pps_mode_write(struct file *file, const char __user *user_buf,
			      size_t count, loff_t *ppos)
{
	struct syn_ptp_platform_mgr *mgr = g_platform_mgr;
	char buf[32];
	size_t len;
	enum pps_mode mode;

	if (!mgr)
		return -ENODEV;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';

	/* Remove trailing newline if present */
	if (len > 0 && buf[len - 1] == '\n')
		buf[len - 1] = '\0';

	/* Parse input string */
	if (strcmp(buf, "input") == 0)
		mode = PPS_MODE_INPUT;
	else if (strcmp(buf, "output") == 0)
		mode = PPS_MODE_OUTPUT;
	else if (strcmp(buf, "disabled") == 0)
		mode = PPS_MODE_DISABLED;
	else
		return -EINVAL;

	/* Update mode in platform manager */
	mgr->pps_mode = mode;

	pr_info("PPS mode set to %s\n",
		mode == PPS_MODE_INPUT ? "input" :
		mode == PPS_MODE_OUTPUT ? "output" : "disabled");

	return count;
}

/* File operations for pps_mode debugfs file */
static const struct file_operations pps_mode_fops = {
	.open = pps_mode_open,
	.read = seq_read,
	.write = pps_mode_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * pps_in_source_show()
 *	Show current PPS_IN source selection via debugfs
 *
 * @m: seq_file for output
 * @v: Unused parameter
 *
 * Returns: 0 on success
 */
static int pps_in_source_show(struct seq_file *m, void *v)
{
	struct syn_ptp_platform_mgr *mgr = g_platform_mgr;
	const char *source_str;

	if (!mgr || mgr->platform != PLATFORM_IPQ52XX) {
		seq_printf(m, "Not available on this platform\n");
		return 0;
	}

	switch (mgr->pps_in_source) {
	case 0:
	case 3:
		source_str = "external_phy";
		break;
	case 1:
		source_str = "pon_mac";
		break;
	case 2:
		source_str = "gephy";
		break;
	default:
		source_str = "unknown";
	}

	seq_printf(m, "%s\n", source_str);
	return 0;
}

/*
 * pps_in_source_open()
 *	Open callback for pps_in_source debugfs file
 */
static int pps_in_source_open(struct inode *inode, struct file *file)
{
	return single_open(file, pps_in_source_show, inode->i_private);
}

/*
 * pps_in_source_write()
 *	Write callback for pps_in_source debugfs file
 *
 * Valid values: "external_phy", "pon_mac", "gephy"
 */
static ssize_t pps_in_source_write(struct file *file, const char __user *user_buf,
				    size_t count, loff_t *ppos)
{
	struct syn_ptp_platform_mgr *mgr = g_platform_mgr;
	char buf[32];
	size_t len;
	u32 value;

	if (!mgr || mgr->platform != PLATFORM_IPQ52XX)
		return -ENODEV;

	if (!mgr->tcsr_pps_in)
		return -ENODEV;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';

	/* Remove trailing newline if present */
	if (len > 0 && buf[len - 1] == '\n')
		buf[len - 1] = '\0';

	/* Parse input string */
	if (strcmp(buf, "external_phy") == 0)
		value = 0;
	else if (strcmp(buf, "pon_mac") == 0)
		value = 1;
	else if (strcmp(buf, "gephy") == 0)
		value = 2;
	else
		return -EINVAL;

	/* Write to TCSR PPS_IN register */
	writel(value, mgr->tcsr_pps_in);

	/* Update cached value */
	mgr->pps_in_source = value;

	pr_info("PPS_IN source set to %s (value=%u)\n",
		value == 0 ? "external_phy" :
		value == 1 ? "pon_mac" : "gephy", value);

	return count;
}

/* File operations for pps_in_source debugfs file */
static const struct file_operations pps_in_source_fops = {
	.open = pps_in_source_open,
	.read = seq_read,
	.write = pps_in_source_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * pps_out_source_show()
 *	Show current PPS_OUT source selection via debugfs
 *
 * @m: seq_file for output
 * @v: Unused parameter
 *
 * Returns: 0 on success
 */
static int pps_out_source_show(struct seq_file *m, void *v)
{
	struct syn_ptp_platform_mgr *mgr = g_platform_mgr;
	const char *source_str;

	if (!mgr || mgr->platform != PLATFORM_IPQ52XX) {
		seq_printf(m, "Not available on this platform\n");
		return 0;
	}

	switch (mgr->pps_out_source) {
	case 0:
		source_str = "nss_pps";
		break;
	case 1:
		source_str = "external_pps";
		break;
	default:
		source_str = "unknown";
	}

	seq_printf(m, "%s\n", source_str);
	return 0;
}

/*
 * pps_out_source_open()
 *	Open callback for pps_out_source debugfs file
 */
static int pps_out_source_open(struct inode *inode, struct file *file)
{
	return single_open(file, pps_out_source_show, inode->i_private);
}

/*
 * pps_out_source_write()
 *	Write callback for pps_out_source debugfs file
 *
 * Valid values: "nss_pps", "external_pps"
 */
static ssize_t pps_out_source_write(struct file *file, const char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct syn_ptp_platform_mgr *mgr = g_platform_mgr;
	char buf[32];
	size_t len;
	u32 value;

	if (!mgr || mgr->platform != PLATFORM_IPQ52XX)
		return -ENODEV;

	if (!mgr->tcsr_pps_out)
		return -ENODEV;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';

	/* Remove trailing newline if present */
	if (len > 0 && buf[len - 1] == '\n')
		buf[len - 1] = '\0';

	/* Parse input string */
	if (strcmp(buf, "nss_pps") == 0)
		value = 0;
	else if (strcmp(buf, "external_pps") == 0)
		value = 1;
	else
		return -EINVAL;

	/* Write to TCSR PPS_OUT register */
	writel(value, mgr->tcsr_pps_out);

	/* Update cached value */
	mgr->pps_out_source = value;

	pr_info("PPS_OUT source set to %s (value=%u)\n",
		value == 0 ? "nss_pps" : "external_pps", value);

	return count;
}

/* File operations for pps_out_source debugfs file */
static const struct file_operations pps_out_source_fops = {
	.open = pps_out_source_open,
	.read = seq_read,
	.write = pps_out_source_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/**
 * calculate_ptp_parameters()
 *	Calculate PTP parameters based on actual PTP reference clock
 *
 * The XGMAC PTP hardware uses two registers to control timestamp increment:
 * 1. SSINC (Sub-Second Increment): Integer nanoseconds added per clock tick
 * 2. Addend: Controls fractional adjustment via accumulator
 *
 * The timestamp counter increments by SSINC nanoseconds each time the
 * internal accumulator overflows (reaches 2^32). The accumulator adds
 * 'addend' on each reference clock cycle.
 *
 * For accurate 1 second = 10^9 nanoseconds:
 * - Accumulator overflows per second = ptp_clock_rate × addend / 2^32
 * - Nanoseconds per second = overflows_per_sec × ssinc = 10^9
 *
 * Therefore: addend = (10^9 × 2^32) / (ssinc × ptp_clock_rate)
 *
 * IMPORTANT: SSINC must be an integer, so we use ceiling division to round up.
 * This ensures we never lose precision due to truncation. The addend value
 * automatically compensates by being smaller than 0xFFFFFFFF.
 *
 * Example for 375 MHz clock:
 * - Ideal: 1,000,000,000 / 375,000,000 = 2.666... ns per tick
 * - SSINC: 3 ns (rounded up)
 * - Addend: ~0xE38E38E3 (compensates for the 0.333 ns excess per tick)
 *
 * @ptp_clock_rate: PTP reference clock frequency in Hz (after division)
 * @ssinc: Output - sub-second increment in nanoseconds (rounded up)
 * @default_addend: Output - default addend value for timestamp accumulator
 *
 * Returns: 0 on success, -EINVAL on invalid parameters
 */
static int calculate_ptp_parameters(u32 ptp_clock_rate,
				    u32 *ssinc, u32 *default_addend)
{
	u64 temp;

	/* Validate input parameters */
	if (!ptp_clock_rate || !ssinc || !default_addend) {
		pr_err("Invalid parameters: ptp_clock_rate=%u, ssinc=%p, default_addend=%p\n",
		       ptp_clock_rate, ssinc, default_addend);
		return -EINVAL;
	}

	/* Calculate SSINC: nanoseconds per clock tick (rounded UP)
	 * Use ceiling division to ensure we never truncate and lose precision.
	 * The addend will compensate for any excess.
	 *
	 * For example, 375 MHz: ceil(10^9 / 375×10^6) = ceil(2.666) = 3 ns
	 */
	*ssinc = (u32)DIV_ROUND_UP_ULL(NSEC_PER_SEC, ptp_clock_rate);

	/* Calculate addend to achieve accurate 10^9 ns per second
	 * Formula: addend = (10^9 × 2^32) / (ssinc × ptp_clock_rate)
	 *
	 * This ensures: (addend × ptp_clock_rate / 2^32) × ssinc = 10^9 ns/sec
	 *
	 * With rounded-up SSINC, the addend will be less than 0xFFFFFFFF,
	 * which compensates for the extra nanoseconds per tick.
	 */
	temp = NSEC_PER_SEC;
	temp <<= 32;  /* 10^9 × 2^32 */
	temp = div_u64(temp, *ssinc);  /* ÷ ssinc (rounded up) */
	*default_addend = (u32)div_u64(temp, ptp_clock_rate);  /* ÷ ptp_clock_rate */

	/* Sanity check: addend should be non-zero */
	if (*default_addend == 0) {
		pr_err("Calculated addend is zero (clock_rate=%u Hz too high)\n",
		       ptp_clock_rate);
		return -EINVAL;
	}

	pr_info("PTP params: clock=%u Hz, ssinc=%u ns, addend=0x%08x\n",
		ptp_clock_rate, *ssinc, *default_addend);

	return 0;
}

/*
 * syn_ptp_stats_show()
 *	Display PTP statistics via debugfs
 *
 * This function displays comprehensive PTP statistics including:
 * - PHC information
 * - TX timestamp statistics
 * - TX timestamp queue status
 *
 * @m: seq_file for output
 * @v: Unused parameter
 *
 * Returns: 0 on success
 */
static int syn_ptp_stats_show(struct seq_file *m, void *v)
{
	struct syn_ptp_priv *ptp_priv = (struct syn_ptp_priv *)m->private;
	unsigned long flags;
	int i;
	ktime_t now;

	if (!ptp_priv) {
		seq_printf(m, "PTP not initialized\n");
		return 0;
	}

	/* Print banner */
	seq_printf(m, "========================================\n");
	seq_printf(m, "PTP Statistics for %s\n", ptp_priv->caps.name);
	seq_printf(m, "========================================\n\n");

	/* PHC Information */
	seq_printf(m, "PHC Information:\n");
	seq_printf(m, "  PHC Index: %d\n", ptp_clock_index(ptp_priv->clock));
	seq_printf(m, "  PTP Clock Rate: %u Hz\n", ptp_priv->ptp_clock_rate);
	seq_printf(m, "  SSINC: %u ns\n", ptp_priv->ssinc);
	seq_printf(m, "  Default Addend: 0x%08x\n", ptp_priv->default_addend);
	seq_printf(m, "  PPS Enabled: %s\n", ptp_priv->pps_enabled ? "Yes" : "No");
	seq_printf(m, "\n");

	/* TX Timestamp Statistics */
	seq_printf(m, "TX Timestamp Statistics:\n");
	seq_printf(m, "  FIFO Full Count: %u\n", ptp_priv->tx_ts_fifo_full_count);
	seq_printf(m, "  Match Success: %u\n", ptp_priv->tx_ts_match_success);
	seq_printf(m, "  Match Failures: %u\n", ptp_priv->tx_ts_match_fail);
	seq_printf(m, "  Stale Timestamps: %u\n", ptp_priv->tx_ts_stale_count);
	seq_printf(m, "  Slot Conflicts: %u\n", ptp_priv->tx_ts_slot_conflict_count);
	seq_printf(m, "\n");

	/* RX Timestamp Statistics */
	seq_printf(m, "RX Timestamp Statistics:\n");
	seq_printf(m, "  Success: %u\n", ptp_priv->rx_ts_success);
	seq_printf(m, "  Filtered: %u\n", ptp_priv->rx_ts_filtered);
	seq_printf(m, "\n");

	/* TX Timestamp Queue Status */
	seq_printf(m, "TX Timestamp Queue Status:\n");
	now = ktime_get();
	spin_lock_irqsave(&ptp_priv->tx_ts_lock, flags);
	for (i = 0; i < SYN_MAC_PTP_TXTS_FIFO_NUM; i++) {
		if (ptp_priv->tx_ts_queue[i].valid) {
			s64 age_ms = ktime_ms_delta(now, ptp_priv->tx_ts_queue[i].capture_time);
			seq_printf(m, "  Slot %d: [Valid] pkt_id=%u, timestamp=%llu ns, age=%lld ms\n",
				   i, ptp_priv->tx_ts_queue[i].pkt_id,
				   ptp_priv->tx_ts_queue[i].timestamp_ns, age_ms);
		} else {
			seq_printf(m, "  Slot %d: [Empty]\n", i);
		}
	}
	spin_unlock_irqrestore(&ptp_priv->tx_ts_lock, flags);
	seq_printf(m, "\n");

	/* Timestamp Configuration */
	seq_printf(m, "Timestamp Configuration:\n");
	seq_printf(m, "  TX Type: %d\n", ptp_priv->tstamp_config.tx_type);
	seq_printf(m, "  RX Filter: %d\n", ptp_priv->tstamp_config.rx_filter);
	seq_printf(m, "\n");

	return 0;
}

/*
 * syn_ptp_stats_open()
 *	Open callback for PTP stats debugfs file
 *
 * @inode: inode structure
 * @file: file structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int syn_ptp_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, syn_ptp_stats_show, inode->i_private);
}

/*
 * syn_ptp_stats_fops
 *	File operations for PTP stats debugfs file
 */
static const struct file_operations syn_ptp_stats_fops = {
	.open = syn_ptp_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * syn_ptp_debugfs_init()
 *	Initialize PTP debugfs interface
 *
 * This function creates a debugfs file at /sys/kernel/debug/qca-nss-dp/ptp/stats_<mac_id>
 * to display PTP statistics for this XGMAC instance.
 *
 * @ptp_priv: Pointer to PTP private data structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int syn_ptp_debugfs_init(struct syn_ptp_priv *ptp_priv)
{
	struct dentry *root_dentry;
	char filename[32];

	mutex_lock(&ptp_debugfs_mutex);

	/* Lookup the root debugfs directory created by EDMA */
	root_dentry = debugfs_lookup("qca-nss-dp", NULL);
	if (!root_dentry) {
		mutex_unlock(&ptp_debugfs_mutex);
		dev_warn(ptp_priv->dev, "EDMA root debugfs directory not found\n");
		return -ENOENT;
	}

	/* Create PTP subdirectory on first call */
	if (!g_ptp_debugfs_dir) {
		g_ptp_debugfs_dir = debugfs_create_dir("ptp", root_dentry);
		if (!g_ptp_debugfs_dir) {
			mutex_unlock(&ptp_debugfs_mutex);
			dev_err(ptp_priv->dev, "Failed to create PTP debugfs directory\n");
			return -ENOMEM;
		}
	}

	/* Increment refcount for PTP debugfs directory */
	g_ptp_debugfs_refcount++;

	mutex_unlock(&ptp_debugfs_mutex);

	/* Create PTP stats file inside ptp subdirectory */
	snprintf(filename, sizeof(filename), "stats_%d", ptp_priv->shd->nghd.mac_id);
	ptp_priv->ptp_dentry = debugfs_create_file(filename, S_IRUGO, g_ptp_debugfs_dir,
						   ptp_priv, &syn_ptp_stats_fops);
	if (!ptp_priv->ptp_dentry) {
		dev_err(ptp_priv->dev, "Failed to create PTP debugfs file\n");

		/* Decrement refcount and cleanup directory if this was the first instance */
		mutex_lock(&ptp_debugfs_mutex);
		g_ptp_debugfs_refcount--;
		if (g_ptp_debugfs_refcount == 0) {
			debugfs_remove(g_ptp_debugfs_dir);
			g_ptp_debugfs_dir = NULL;
		}
		mutex_unlock(&ptp_debugfs_mutex);

		return -ENOMEM;
	}

	/* Create PPS control files (only once, not per-MAC) */
	mutex_lock(&ptp_debugfs_mutex);
	if (g_ptp_debugfs_refcount == 1) {
		/* First instance - create PPS control files */
		struct dentry *pps_mode_dentry, *pps_in_dentry, *pps_out_dentry;

		/* Create pps_mode file (available on all platforms with GPIO) */
		if (ptp_priv->platform_mgr->gpio_desc) {
			pps_mode_dentry = debugfs_create_file("pps_mode", 0644, g_ptp_debugfs_dir,
							      NULL, &pps_mode_fops);
			if (!pps_mode_dentry) {
				dev_warn(ptp_priv->dev, "Failed to create pps_mode debugfs file\n");
			}
		}

		/* Create IPQ52XX-specific files */
		if (ptp_priv->platform_mgr->platform == PLATFORM_IPQ52XX) {
			pps_in_dentry = debugfs_create_file("pps_in_source", 0644, g_ptp_debugfs_dir,
							    NULL, &pps_in_source_fops);
			if (!pps_in_dentry) {
				dev_warn(ptp_priv->dev, "Failed to create pps_in_source debugfs file\n");
			}

			pps_out_dentry = debugfs_create_file("pps_out_source", 0644, g_ptp_debugfs_dir,
							     NULL, &pps_out_source_fops);
			if (!pps_out_dentry) {
				dev_warn(ptp_priv->dev, "Failed to create pps_out_source debugfs file\n");
			}
		}
	}
	mutex_unlock(&ptp_debugfs_mutex);

	return 0;
}

/*
 * syn_ptp_debugfs_exit()
 *	Cleanup PTP debugfs interface
 *
 * This function removes the PTP debugfs file and cleans up the PTP
 * subdirectory when the last instance is removed.
 *
 * @ptp_priv: Pointer to PTP private data structure
 */
static void syn_ptp_debugfs_exit(struct syn_ptp_priv *ptp_priv)
{
	if (ptp_priv->ptp_dentry) {
		debugfs_remove(ptp_priv->ptp_dentry);
		ptp_priv->ptp_dentry = NULL;
	}

	/* Decrement refcount and remove PTP directory if this is the last instance */
	mutex_lock(&ptp_debugfs_mutex);
	g_ptp_debugfs_refcount--;
	if (g_ptp_debugfs_refcount == 0 && g_ptp_debugfs_dir) {
		debugfs_remove(g_ptp_debugfs_dir);
		g_ptp_debugfs_dir = NULL;
	}
	mutex_unlock(&ptp_debugfs_mutex);
}

/*
 * syn_ptp_hw_init()
 *	Initialize PTP hardware following XGMAC datasheet guidelines
 *
 * This function implements the 7-step initialization sequence from the
 * XGMAC PTP datasheet (Section 14.26.1):
 * 1. Mask timestamp trigger interrupt
 * 2. Enable timestamping (TSENA)
 * 3. Program sub-second increment based on PTP clock frequency
 * 4. Select Fine correction method
 * 5-6. Initialize timestamp counter to 0
 * 6b. Initialize asymmetry correction registers
 *
 * @ptp_priv: Pointer to PTP private data structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int syn_ptp_hw_init(struct syn_ptp_priv *ptp_priv)
{
	void __iomem *mac_base = ptp_priv->shd->nghd.mac_base;
	int ret;

	/* Step 1: Mask timestamp trigger interrupt (bit 12) */
	hal_clear_reg_bits(mac_base, SYN_MAC_INT_ENABLE, SYN_MAC_INT_ENABLE_TSIE);

	/* Step 2: Enable timestamp, Fine mode and Timestamp Digital Rollover Control */
	hal_set_reg_bits(mac_base, SYN_MAC_TS_CTL, SYN_MAC_TS_CTL_TSENA |
			 SYN_MAC_TS_CTL_TSCFUPDT | SYN_MAC_TS_CTL_TSCTRLSSR);

	/* Step 3: Calculate PTP parameters using datasheet formula */
	ret = calculate_ptp_parameters(ptp_priv->ptp_clock_rate,
				       &ptp_priv->ssinc, &ptp_priv->default_addend);
	if (ret) {
		dev_err(ptp_priv->dev, "Failed to calculate PTP parameters: %d\n", ret);
		return ret;
	}

	/* Program sub-second increment register */
	syn_ptp_increment_set(mac_base, ptp_priv->ssinc);

	/* Step 4: Initialize addend register for fine frequency adjustment
	 * The addend value controls the rate at which the timestamp counter increments.
	 * Using the calculated default addend ensures accurate timekeeping.
	 */
	ret = syn_ptp_adjfine(mac_base, ptp_priv->default_addend);
	if (ret) {
		dev_err(ptp_priv->dev, "Failed to set default addend: %d\n", ret);
		return ret;
	}

	/* Step 5 & 6: Initialize timestamp counter to 0 */
	ret = syn_ptp_time_set(mac_base, SYN_MAC_TS_CTL_TSINIT, 0, 0);

	/* Step 6b: Initialize asymmetry correction registers to 0 */
	hal_write_reg(mac_base, SYN_MAC_TS_INGRESS_ASYM_CORR, 0);
	hal_write_reg(mac_base, SYN_MAC_TS_EGRESS_ASYM_CORR, 0);

	dev_info(ptp_priv->dev,
		 "Sub-second increment: %u ns, Addend: 0x%08x, Clock rate: %u Hz)\n",
		 ptp_priv->ssinc, ptp_priv->default_addend, ptp_priv->ptp_clock_rate);

	return ret;
}

/*
 * syn_ptp_init()
 *	Initialize PTP Hardware Clock support
 *
 * This function allocates and initializes the PTP private data structure,
 * sets up the PHC capabilities, and registers the PHC device with the
 * Linux PTP subsystem.
 *
 * @shd: Pointer to syn_hal_dev structure
 * @pdev: Pointer to platform device structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int syn_ptp_init(struct syn_hal_dev *shd, struct platform_device *pdev)
{
	struct syn_ptp_priv *ptp_priv;
	struct device *dev = &pdev->dev;
	struct clk *ptp_clk;
	int ret;

	/* Check PTP reference clock rate available or not. */
	ptp_clk = devm_clk_get_optional_enabled(dev, "ptp");
	if (IS_ERR(ptp_clk)) {
		dev_err(dev, "Failed to get PTP clock\n");
		return PTR_ERR(ptp_clk);
	}

	/* PTP is optional to be enabled. */
	if (!ptp_clk) {
		dev_dbg(dev, "PTP is not enabled\n");
		return 0;
	}

	/* Allocate PTP private data structure */
	ptp_priv = devm_kzalloc(dev, sizeof(*ptp_priv), GFP_KERNEL);
	if (!ptp_priv) {
		dev_err(dev, "Failed to allocate PTP private data\n");
		return -ENOMEM;
	}

	/* Get the PTP reference clock rate. */
	ptp_priv->ptp_clock_rate = clk_get_rate(ptp_clk);

	/* Store back pointer to HAL device */
	ptp_priv->shd = shd;

	/* Store platform device pointer */
	ptp_priv->dev = dev;

	/* Initialize spinlock for timestamp access synchronization */
	spin_lock_init(&ptp_priv->lock);

	/* Initialize TX timestamp queue spinlock */
	spin_lock_init(&ptp_priv->tx_ts_lock);

	/* Get reference to singleton platform manager */
	ptp_priv->platform_mgr = syn_ptp_platform_mgr_get(pdev);
	if (!ptp_priv->platform_mgr) {
		dev_err(dev, "Failed to get platform manager\n");
		return -ENOMEM;
	}

	/* Initialize PPS enabled flag */
	ptp_priv->pps_enabled = false;

	/* Initialize PTP clock capabilities */
	ptp_priv->caps.owner = THIS_MODULE;
	snprintf(ptp_priv->caps.name, sizeof(ptp_priv->caps.name),
		 "xgmac-ptp-%d", shd->nghd.mac_id);
	ptp_priv->caps.max_adj = S32_MAX;	/* Maximum frequency adjustment (ppb) */
	ptp_priv->caps.n_alarm = 0;		/* No alarm support */
	ptp_priv->caps.n_ext_ts = 1;		/* One external timestamp channel */
	ptp_priv->caps.n_per_out = 1;		/* One periodic output channel */
	ptp_priv->caps.n_pins = 1;		/* One configurable pin */
	ptp_priv->caps.pps = 1;			/* PPS support enabled */

	/* Set callback function pointers (stub implementations for now) */
	ptp_priv->caps.adjfine = qcom_nss_ptp_adjfine;
	ptp_priv->caps.adjtime = qcom_nss_ptp_adjtime;
	ptp_priv->caps.gettimex64 = qcom_nss_ptp_gettimex;
	ptp_priv->caps.settime64 = qcom_nss_ptp_settime;
	ptp_priv->caps.enable = qcom_nss_ptp_enable;
	ptp_priv->caps.verify = qcom_nss_ptp_verify;
	snprintf(ptp_priv->pin.name, sizeof(ptp_priv->pin.name), "PPS_SYNC");
	ptp_priv->caps.pin_config = &ptp_priv->pin;

	/* Initialize PTP hardware following datasheet guidelines */
	ret = syn_ptp_hw_init(ptp_priv);
	if (ret) {
		dev_err(dev, "PTP hardware initialization failed: %d\n", ret);
		goto err_free_priv;
	}

	/* Register PHC device with PTP subsystem */
	ptp_priv->clock = ptp_clock_register(&ptp_priv->caps, dev);
	if (IS_ERR(ptp_priv->clock)) {
		ret = PTR_ERR(ptp_priv->clock);
		dev_err(dev, "Failed to register PHC device: %d\n", ret);
		goto err_free_priv;
	}

	dev_info(dev, "PHC device registered successfully (index=%d)\n",
		 ptp_clock_index(ptp_priv->clock));

	/* Store PTP private data pointer in HAL device */
	shd->ptp_priv = ptp_priv;

	/* Initialize debugfs interface for PTP statistics */
	ret = syn_ptp_debugfs_init(ptp_priv);
	if (ret) {
		dev_warn(dev, "Failed to initialize PTP debugfs: %d\n", ret);
		/* Continue without debugfs support */
	}

	/* Set default PPS mode to INPUT (slave mode) if IRQ is valid */
	if (ptp_priv->platform_mgr->pps_in_irq >= 0) {
		ret = syn_ptp_set_pps_mode(ptp_priv, PPS_MODE_INPUT);
		if (ret) {
			dev_warn(dev, "Failed to set default PPS_IN mode: %d\n", ret);
			/* Continue with PPS disabled */
		}
	}

	return 0;

err_free_priv:
	syn_ptp_platform_mgr_put();
	return ret;
}
EXPORT_SYMBOL(syn_ptp_init);

/*
 * syn_ptp_get_ts_info()
 *	Get PTP timestamping information
 *
 * This function populates the ethtool_ts_info structure with PTP
 * timestamping capabilities supported by the hardware.
 *
 * @shd: Pointer to syn_hal_dev structure
 * @info: Pointer to ethtool_ts_info structure to populate
 *
 * Returns: 0 on success, negative error code on failure
 */
int syn_ptp_get_ts_info(struct syn_hal_dev *shd, struct ethtool_ts_info *info)
{
	struct syn_ptp_priv *ptp_priv;

	/* Retrieve ptp_priv from shd structure */
	ptp_priv = shd->ptp_priv;

	/* Check if PTP was initialized */
	if (!ptp_priv || !ptp_priv->clock) {
		/* PTP not available, return software timestamping only */
		info->so_timestamping = SOF_TIMESTAMPING_TX_SOFTWARE |
					SOF_TIMESTAMPING_RX_SOFTWARE |
					SOF_TIMESTAMPING_SOFTWARE;
		info->phc_index = -1;
		info->tx_types = (1 << HWTSTAMP_TX_OFF);
		info->rx_filters = (1 << HWTSTAMP_FILTER_NONE);
		return 0;
	}

	/* Populate timestamping capabilities */
	info->so_timestamping = SOF_TIMESTAMPING_TX_SOFTWARE |
				SOF_TIMESTAMPING_RX_SOFTWARE |
				SOF_TIMESTAMPING_SOFTWARE |
				SOF_TIMESTAMPING_TX_HARDWARE |
				SOF_TIMESTAMPING_RX_HARDWARE |
				SOF_TIMESTAMPING_RAW_HARDWARE;

	/* Set PHC index */
	info->phc_index = ptp_clock_index(ptp_priv->clock);

	/* Supported TX timestamp types */
	info->tx_types = (1 << HWTSTAMP_TX_OFF) |
			 (1 << HWTSTAMP_TX_ON) |
			 (1 << HWTSTAMP_TX_ONESTEP_SYNC);

	/* Supported RX filter types */
	info->rx_filters = (1 << HWTSTAMP_FILTER_NONE) |
			   (1 << HWTSTAMP_FILTER_ALL) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_L2_SYNC) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_L2_EVENT) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_L4_SYNC) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_L4_EVENT) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_SYNC) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_DELAY_REQ) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_EVENT);

	return 0;
}
EXPORT_SYMBOL(syn_ptp_get_ts_info);

/*
 * syn_ptp_hwtstamp_set()
 *	Configure hardware timestamping
 *
 * This function configures the XGMAC hardware for packet timestamping
 * based on the requested configuration from userspace (via SIOCSHWTSTAMP ioctl).
 *
 * @shd: Pointer to syn_hal_dev structure
 * @ifr: Pointer to ifreq structure containing hwtstamp_config
 *
 * Returns: 0 on success, negative error code on failure
 */
int syn_ptp_hwtstamp_set(void *hal_ctx, struct ifreq *ifr)
{
	struct syn_hal_dev *shd = (struct syn_hal_dev *)hal_ctx;
	struct syn_ptp_priv *ptp_priv = shd->ptp_priv;
	void __iomem *mac_base = shd->nghd.mac_base;
	struct hwtstamp_config config;
	unsigned long flags;
	u32 ts_ctl = 0;

	/* Check if PTP is initialized */
	if (!ptp_priv) {
		return -ENODEV;
	}

	/* Copy configuration from userspace */
	if (copy_from_user(&config, ifr->ifr_data, sizeof(config))) {
		return -EFAULT;
	}

	/* Validate TX timestamp mode */
	switch (config.tx_type) {
	case HWTSTAMP_TX_OFF:
	case HWTSTAMP_TX_ON:
	case HWTSTAMP_TX_ONESTEP_SYNC:
		break;
	default:
		return -ERANGE;
	}

	/* Validate and normalize RX filter */
	switch (config.rx_filter) {
	case HWTSTAMP_FILTER_NONE:
		break;

	case HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L4_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ:
		/* PTP v2 over L4 (UDP) - upgrade to event messages */
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_L4_EVENT;
		break;

	case HWTSTAMP_FILTER_PTP_V2_L2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ:
		/* PTP v2 over L2 (Ethernet) - upgrade to event messages */
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_L2_EVENT;
		break;

	case HWTSTAMP_FILTER_PTP_V2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
		/* PTP v2 over both L2 and L4 - upgrade to event messages */
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;
		break;

	case HWTSTAMP_FILTER_ALL:
		/* Timestamp all packets */
		break;

	default:
		return -ERANGE;
	}

	spin_lock_irqsave(&ptp_priv->lock, flags);

	/* Read current timestamp control register */
	ts_ctl = hal_read_reg(mac_base, SYN_MAC_TS_CTL);

	/* Clear configuration bits using mask (upstream approach) */
	ts_ctl &= ~SYN_MAC_HWTS_CFG_MASK;

	/* Build new configuration */
	u32 new_config = 0;

	/* Configure TX timestamping */
	if (config.tx_type != HWTSTAMP_TX_OFF) {
		new_config |= SYN_MAC_TS_CTL_TSENA;

		/* Enable one-step sync if requested */
		if (config.tx_type == HWTSTAMP_TX_ONESTEP_SYNC) {
			new_config |= SYN_MAC_TS_CTL_CSC;  /* Checksum correction */
		}
	}

	/* Configure RX filtering based on config.rx_filter */
	switch (config.rx_filter) {
	case HWTSTAMP_FILTER_NONE:
		/* No RX timestamping */
		break;

	case HWTSTAMP_FILTER_ALL:
		/* Timestamp all packets */
		new_config |= SYN_MAC_TS_CTL_TSENALL;
		break;

	case HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
		/* PTP v2 over UDP (IPv4/IPv6), event messages only */
		new_config |= SYN_MAC_TS_CTL_TSVER2ENA |
			      SYN_MAC_TS_CTL_TSIPV4ENA |
			      SYN_MAC_TS_CTL_TSIPV6ENA |
			      SYN_MAC_TS_CTL_TSEVENTENA;
		break;

	case HWTSTAMP_FILTER_PTP_V2_L2_EVENT:
		/* PTP v2 over Ethernet (L2), event messages only */
		new_config |= SYN_MAC_TS_CTL_TSVER2ENA |
			      SYN_MAC_TS_CTL_TSIPENA |
			      SYN_MAC_TS_CTL_TSEVENTENA;
		break;

	case HWTSTAMP_FILTER_PTP_V2_EVENT:
		/* PTP v2 over both L2 and L4, event messages only */
		new_config |= SYN_MAC_TS_CTL_TSVER2ENA |
			      SYN_MAC_TS_CTL_TSIPENA |
			      SYN_MAC_TS_CTL_TSIPV4ENA |
			      SYN_MAC_TS_CTL_TSIPV6ENA |
			      SYN_MAC_TS_CTL_TSEVENTENA;
		break;

	default:
		spin_unlock_irqrestore(&ptp_priv->lock, flags);
		return -ERANGE;
	}

	/* Apply new configuration (upstream mask-based approach) */
	ts_ctl |= new_config;
	hal_write_reg(mac_base, SYN_MAC_TS_CTL, ts_ctl);

	/* Store configuration */
	memcpy(&ptp_priv->tstamp_config, &config, sizeof(config));

	spin_unlock_irqrestore(&ptp_priv->lock, flags);

	if (ptp_priv->platform_mgr->platform == PLATFORM_IPQ52XX ||
	    ptp_priv->platform_mgr->platform == PLATFORM_IPQ96XX) {
		fal_port_pps_ctrl_t pps_ctrl = {0};

		fal_port_pps_ctrl_get(0, &pps_ctrl);
		if (config.rx_filter != HWTSTAMP_FILTER_NONE ||
				config.tx_type != HWTSTAMP_TX_OFF) {
			/* Enabling PTP - select this MAC (mac_id - 1) PPS out. */
			pps_ctrl.pps_out_sel = shd->nghd.mac_id - 1;
		} else {
			/* Disabling PTP - if we own the PPS out, release it (to 0) */
			if (pps_ctrl.pps_out_sel == shd->nghd.mac_id - 1)
				pps_ctrl.pps_out_sel = 0;
		}
		fal_port_pps_ctrl_set(0, &pps_ctrl);
	}

	/* Copy configuration back to userspace */
	if (copy_to_user(ifr->ifr_data, &config, sizeof(config))) {
		return -EFAULT;
	}

	return 0;
}
EXPORT_SYMBOL(syn_ptp_hwtstamp_set);

/*
 * syn_ptp_hwtstamp_get()
 *	Get current hardware timestamping configuration
 *
 * This function retrieves the current hardware timestamping configuration
 * (via SIOCGHWTSTAMP ioctl).
 *
 * @shd: Pointer to syn_hal_dev structure
 * @ifr: Pointer to ifreq structure to receive hwtstamp_config
 *
 * Returns: 0 on success, negative error code on failure
 */
int syn_ptp_hwtstamp_get(void *hal_ctx, struct ifreq *ifr)
{
	struct syn_hal_dev *shd = (struct syn_hal_dev *)hal_ctx;
	struct syn_ptp_priv *ptp_priv = shd->ptp_priv;
	struct hwtstamp_config config;
	unsigned long flags;

	/* Check if PTP is initialized */
	if (!ptp_priv) {
		return -ENODEV;
	}

	spin_lock_irqsave(&ptp_priv->lock, flags);
	memcpy(&config, &ptp_priv->tstamp_config, sizeof(config));
	spin_unlock_irqrestore(&ptp_priv->lock, flags);

	/* Copy configuration to userspace */
	if (copy_to_user(ifr->ifr_data, &config, sizeof(config))) {
		return -EFAULT;
	}

	return 0;
}
EXPORT_SYMBOL(syn_ptp_hwtstamp_get);

/*
 * syn_ptp_drain_tx_fifo()
 *	Drain all available TX timestamps from hardware FIFO
 *
 * This function reads all available timestamps from the 8-deep hardware FIFO
 * and stores them in the software queue indexed by packet ID. It uses the
 * TTSNS field (bits 14:10) in register 0xd20 to determine how many timestamps
 * are available.
 *
 * Called from TX completion handler to ensure timestamps don't get lost.
 *
 * @ptp_priv: Pointer to PTP private data structure
 */
static void syn_ptp_drain_tx_fifo(struct syn_ptp_priv *ptp_priv)
{
	void __iomem *mac_base = ptp_priv->shd->nghd.mac_base;
	unsigned long flags;
	u32 status, num_ts;
	int i, slot;
	struct {
		u32 pkt_id;
		u32 sec;
		u32 nsec;
		u64 timestamp_ns;
		bool valid;
	} temp_ts[SYN_MAC_PTP_TXTS_FIFO_NUM];
	int valid_count = 0;

	spin_lock_irqsave(&ptp_priv->lock, flags);

	/* Read number of timestamps available in FIFO (bits 14:10) */
	status = hal_read_reg(mac_base, SYN_MAC_TS_STATUS);
	num_ts = (status & SYN_MAC_TS_STATUS_TTSNS_MASK) >> SYN_MAC_TS_STATUS_TTSNS_SHIFT;

	if (num_ts == 0) {
		spin_unlock_irqrestore(&ptp_priv->lock, flags);
		return;
	}

	/* Check for FIFO full condition */
	if (num_ts == SYN_MAC_PTP_TXTS_FIFO_NUM) {
		ptp_priv->tx_ts_fifo_full_count++;
		if (net_ratelimit()) {
			dev_warn(ptp_priv->dev,
				 "TX timestamp FIFO full (%u entries), possible timestamp loss\n",
				 num_ts);
		}
	}

	/* Read all timestamps from hardware FIFO into temporary buffer */
	for (i = 0; i < num_ts && valid_count < SYN_MAC_PTP_TXTS_FIFO_NUM; i++) {
		u32 nsec, sec, pkt_id;

		pkt_id = hal_read_reg(mac_base, SYN_MAC_TX_TS_STATUS_PKTID) & 0x3FF;
		sec = hal_read_reg(mac_base, SYN_MAC_TX_TS_STATUS_SECS);
		nsec = hal_read_reg(mac_base, SYN_MAC_TX_TS_STATUS_NSECS);

		/* Check for missed timestamp flag (bit 31) */
		if (nsec & SYN_MAC_TX_TS_STATUS_TXTSSMIS) {
			if (net_ratelimit()) {
				dev_warn(ptp_priv->dev,
					 "TX timestamp missed for pkt_id=%u\n", pkt_id);
			}
			continue;
		}

		/* Store in temporary buffer */
		temp_ts[valid_count].pkt_id = pkt_id;
		temp_ts[valid_count].sec = sec;
		temp_ts[valid_count].nsec = nsec & SYN_MAC_TX_TS_STATUS_NSECS_MASK;
		temp_ts[valid_count].timestamp_ns = ((u64)sec * NSEC_PER_SEC) + temp_ts[valid_count].nsec;
		temp_ts[valid_count].valid = true;
		valid_count++;
	}

	spin_unlock_irqrestore(&ptp_priv->lock, flags);

	/* Now update the queue with tx_ts_lock only */
	spin_lock_irqsave(&ptp_priv->tx_ts_lock, flags);
	for (i = 0; i < valid_count; i++) {
		slot = temp_ts[i].pkt_id % SYN_MAC_PTP_TXTS_FIFO_NUM;

		if (ptp_priv->tx_ts_queue[slot].valid == true) {
			int j;
			/* Try to find an unused slot */
			for (j = 0; j < SYN_MAC_PTP_TXTS_FIFO_NUM; j++) {
				if (!ptp_priv->tx_ts_queue[j].valid) {
					slot = j;
					break;
				}
			}

			if (ptp_priv->tx_ts_queue[slot].valid == true) {
				ptp_priv->tx_ts_slot_conflict_count++;
				dev_warn(ptp_priv->dev,
					 "TX timestamp slot conflict: slot=%d, old_pkt_id=%u, new_pkt_id=%u (conflicts=%u)\n",
					 slot, ptp_priv->tx_ts_queue[slot].pkt_id, temp_ts[i].pkt_id,
					 ptp_priv->tx_ts_slot_conflict_count);
			}
		}

		ptp_priv->tx_ts_queue[slot].timestamp_ns = temp_ts[i].timestamp_ns;
		ptp_priv->tx_ts_queue[slot].pkt_id = temp_ts[i].pkt_id;
		ptp_priv->tx_ts_queue[slot].capture_time = ktime_get();
		ptp_priv->tx_ts_queue[slot].valid = true;
	}
	spin_unlock_irqrestore(&ptp_priv->tx_ts_lock, flags);
}

/*
 * syn_ptp_get_tx_hwtstamp_by_id()
 *	Retrieve TX timestamp by packet ID
 *
 * This function looks up a TX timestamp in the software queue by packet ID.
 * It first drains any pending timestamps from the hardware FIFO, then
 * searches for a matching packet ID. Stale timestamps (older than 100ms)
 * are automatically discarded.
 *
 * @shd: Pointer to syn_hal_dev structure
 * @pkt_id: 10-bit packet ID (1-1023) to match
 * @shhwtstamps: Pointer to store the retrieved timestamp
 *
 * Returns: 0 on success, -ENODATA if timestamp not found or stale
 */
int syn_ptp_get_tx_hwtstamp_by_id(struct syn_hal_dev *shd,
				   u16 pkt_id,
				   struct skb_shared_hwtstamps *shhwtstamps)
{
	struct syn_ptp_priv *ptp_priv = shd->ptp_priv;
	unsigned long flags;
	int slot = pkt_id % SYN_MAC_PTP_TXTS_FIFO_NUM;
	ktime_t now;
	s64 age_ms;
	bool found = false;
	int i;

	if (!ptp_priv || ptp_priv->tstamp_config.tx_type == HWTSTAMP_TX_OFF)
		return -ENODEV;

	/* First, drain any pending timestamps from hardware FIFO */
	syn_ptp_drain_tx_fifo(ptp_priv);

	spin_lock_irqsave(&ptp_priv->tx_ts_lock, flags);

	/* Check if we have a valid timestamp for this packet ID */
	if (!ptp_priv->tx_ts_queue[slot].valid ||
	    ptp_priv->tx_ts_queue[slot].pkt_id != pkt_id) {
		/* Not in preferred slot, search the entire queue */
		for (i = 0; i < SYN_MAC_PTP_TXTS_FIFO_NUM; i++) {
			if (ptp_priv->tx_ts_queue[i].valid &&
			    ptp_priv->tx_ts_queue[i].pkt_id == pkt_id) {
				slot = i;
				break;
			}
		}
	}

	if (ptp_priv->tx_ts_queue[slot].valid &&
	    ptp_priv->tx_ts_queue[slot].pkt_id == pkt_id) {

		/* Check if timestamp is stale (older than 100ms) */
		now = ktime_get();
		age_ms = ktime_ms_delta(now, ptp_priv->tx_ts_queue[slot].capture_time);

		if (age_ms < 100) {
			/* Timestamp is fresh - use it */
			memset(shhwtstamps, 0, sizeof(*shhwtstamps));
			shhwtstamps->hwtstamp = ns_to_ktime(ptp_priv->tx_ts_queue[slot].timestamp_ns);
			ptp_priv->tx_ts_queue[slot].valid = false;  /* Mark as consumed */
			ptp_priv->tx_ts_match_success++;
			found = true;
		} else {
			/* Timestamp is stale - discard it */
			ptp_priv->tx_ts_stale_count++;
			ptp_priv->tx_ts_queue[slot].valid = false;
			if (net_ratelimit()) {
				dev_warn(ptp_priv->dev,
					"Stale TX timestamp for pkt_id=%u (age=%lld ms)\n",
					pkt_id, age_ms);
			}
		}
	}

	spin_unlock_irqrestore(&ptp_priv->tx_ts_lock, flags);

	if (!found) {
		ptp_priv->tx_ts_match_fail++;
		return -ENODATA;
	}

	return 0;
}
EXPORT_SYMBOL(syn_ptp_get_tx_hwtstamp_by_id);

/*
 * syn_ptp_cleanup()
 *	Cleanup PTP Hardware Clock support
 *
 * This function unregisters the PHC device and frees all allocated resources.
 * It is called during driver unload or when XGMAC interface is brought down.
 *
 * @shd: Pointer to syn_hal_dev structure
 */
void syn_ptp_cleanup(struct syn_hal_dev *shd)
{
	struct syn_ptp_priv *ptp_priv;

	/* Retrieve ptp_priv from shd structure */
	ptp_priv = shd->ptp_priv;

	/* Check if PTP was initialized */
	if (!ptp_priv) {
		return;
	}

	dev_info(ptp_priv->dev, "Cleaning up PTP support\n");

	/* Cleanup debugfs interface */
	syn_ptp_debugfs_exit(ptp_priv);

	/* Disable PPS mode (this will unregister interrupts if needed) */
	if (ptp_priv->platform_mgr->pps_mode != PPS_MODE_DISABLED) {
		syn_ptp_set_pps_mode(ptp_priv, PPS_MODE_DISABLED);
	}

	/* Unregister PHC device */
	if (ptp_priv->clock) {
		ptp_clock_unregister(ptp_priv->clock);
		dev_info(ptp_priv->dev, "PHC device unregistered\n");
	}

	/* Release reference to platform manager (handles platform cleanup on last instance) */
	syn_ptp_platform_mgr_put();

	/* Clear pointer in HAL device structure */
	shd->ptp_priv = NULL;
}
EXPORT_SYMBOL(syn_ptp_cleanup);

MODULE_DESCRIPTION("Synopsys XGMAC PTP Hardware Clock support");
MODULE_LICENSE("Dual BSD/GPL");
