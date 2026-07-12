/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include <linux/string.h>
#include "edma.h"
#include "edma_regs.h"
#include "edma_debug.h"
#include "edma_debugfs.h"
#include "nss_dp_dev.h"
#if defined(NSS_DP_PON_SUPPORT)
#include <fal/fal_pon.h>
#endif

#define EDMA_DEBUGFS_RINGS_PER_SECTION	6
#define EDMA_DEBUGFS_FIELD_WIDTH	30
#define EDMA_DEBUGFS_RING_COL_WIDTH	11

/*
 * edma_debugfs_ring_usage_dump
 * 	Format to print the EDMA ring utilization (full)
 */
const char *edma_debugfs_ring_usage_dump[EDMA_RING_USAGE_MAX_FULL] = {
	"100_percentage_full",
	"90_to_100_percentage_full",
	"70_to_90_percentage_full",
	"50_to_70_percentage_full",
	"Less_than_50_percentage_full"
};

/*
 * edma_debugfs_ring_usage_rx_fill_dump
 * 	Format to print EDMA Rx fill ring empty
 */
const char *edma_debugfs_ring_usage_rx_fill_dump[EDMA_RING_USAGE_MAX_FULL] = {
	"100_percentage_empty",
	"90_to_100_percentage_empty",
	"70_to_90_percentage_empty",
	"50_to_70_percentage_empty",
	"Less_than_50_percentage_empty"
};

/*
 * edma_txcmpl_err_string
 *	EDMA Tx complete error string.
 */
const char *edma_txcmpl_err_string[EDMA_TX_CMPL_ERR_MAX] = {
	"IP_header_length",
	"TSO",
	"IPv6_data_length",
	"TCP_header",
	"TCP_header_offset",
	"TCP_data_offset",
	"UDP_header",
	"UDP_header_offset",
	"UDP_data_offset",
	"UDPLite_header",
	"UDPLite_header_offset",
	"UDPLite_csum_cov",
	"IP_version",
	"L4_offset_<_L3_offset",
	"L4_offset_oob",
	"L3_offset_oob",
	"Payload_offset_oob",
	"Custom_Chksum_offset_oob",
	"Reserved",
	"Reserved",
	"Reserved",
	"TSO_MSS",
	"TSO_TCP_packet"
};

/*
 * edma_debugfs_print_banner()
 *	API to print the banner for a node
 */
static void edma_debugfs_print_banner(struct seq_file *m, char *node)
{
	uint32_t banner_char_len, i;

	for (i = 0; i < EDMA_STATS_BANNER_MAX_LEN; i++) {
		seq_printf(m, "_");
	}

	banner_char_len = (EDMA_STATS_BANNER_MAX_LEN - (strlen(node) + 2)) / 2;

	seq_printf(m, "\n\n");

	for (i = 0; i < banner_char_len; i++) {
		seq_printf(m, "<");
	}

	seq_printf(m, " %s ", node);

	for (i = 0; i < banner_char_len; i++) {
		seq_printf(m, ">");
	}
	seq_printf(m, "\n");

	for (i = 0; i < EDMA_STATS_BANNER_MAX_LEN; i++) {
		seq_printf(m, "_");
	}

	seq_printf(m, "\n\n");
}

/*
 * edma_debugfs_print_rx_desc_section()
 *	Print RX descriptor ring stats
 */
static void edma_debugfs_print_rx_desc_section(struct seq_file *m,
				   struct edma_rx_desc_stats *stats,
				   uint32_t *ring_ids,
				   uint32_t count)
{
	uint32_t i, j;
	char ring_header[EDMA_DEBUGFS_RING_COL_WIDTH + 1];

	seq_printf(m, "\nRX_DESC_RING_STATS (Rings %d-%d):\n",
		   ring_ids[0], ring_ids[count - 1]);

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "Field");
	for (i = 0; i < count; i++) {
		snprintf(ring_header, sizeof(ring_header), "Ring_%d", ring_ids[i]);
		seq_printf(m, " %*s", EDMA_DEBUGFS_RING_COL_WIDTH, ring_header);
	}
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "src_port_inval");
	for (i = 0; i < count; i++)
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[ring_ids[i]].src_port_inval);
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "src_port_inval_type");
	for (i = 0; i < count; i++)
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[ring_ids[i]].src_port_inval_type);
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "src_port_inval_netdev");
	for (i = 0; i < count; i++)
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[ring_ids[i]].src_port_inval_netdev);
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "rx_napi_sched");
	for (i = 0; i < count; i++)
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[ring_ids[i]].rx_napi_sched);
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "payload_buf_alloc_failed");
	for (i = 0; i < count; i++) {
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[ring_ids[i]].payload_buf_alloc_failed);
	}
	seq_printf(m, "\n");

	for (j = 0; j < EDMA_RING_USAGE_MAX_FULL; j++) {
		seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH,
			   edma_debugfs_ring_usage_dump[j]);
		for (i = 0; i < count; i++)
			seq_printf(m, " %*d", EDMA_DEBUGFS_RING_COL_WIDTH,
				   stats[ring_ids[i]].ring_stats.util[j]);
		seq_printf(m, "\n");
	}
}

/*
 * edma_debugfs_print_rx_fill_section()
 *	Print RX fill ring stats
 */
static void edma_debugfs_print_rx_fill_section(struct seq_file *m,
				   struct edma_rx_fill_stats *stats,
				   uint32_t *ring_ids,
				   uint32_t count)
{
	uint32_t i, j;
	char ring_header[EDMA_DEBUGFS_RING_COL_WIDTH + 1];

	seq_printf(m, "\nRX_FILL_RING_STATS (Rings %d-%d):\n",
		   ring_ids[0], ring_ids[count - 1]);

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "Field");
	for (i = 0; i < count; i++) {
		snprintf(ring_header, sizeof(ring_header), "Ring_%d", ring_ids[i]);
		seq_printf(m, " %*s", EDMA_DEBUGFS_RING_COL_WIDTH, ring_header);
	}
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "alloc_failed");
	for (i = 0; i < count; i++)
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[ring_ids[i]].alloc_failed);
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "page_alloc_failed");
	for (i = 0; i < count; i++)
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[ring_ids[i]].page_alloc_failed);
	seq_printf(m, "\n");

	for (j = 0; j < EDMA_RING_USAGE_MAX_FULL; j++) {
		seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH,
			   edma_debugfs_ring_usage_rx_fill_dump[j]);
		for (i = 0; i < count; i++)
			seq_printf(m, " %*d", EDMA_DEBUGFS_RING_COL_WIDTH,
				   stats[ring_ids[i]].ring_stats.util[j]);
		seq_printf(m, "\n");
	}
}

/*
 * edma_debugfs_print_tx_cmpl_section()
 *	Print TX completion ring stats
 */
static void edma_debugfs_print_tx_cmpl_section(struct seq_file *m,
				   struct edma_tx_cmpl_stats *stats,
				   uint32_t *ring_ids,
				   uint32_t count)
{
	uint32_t i, j;
	char ring_header[EDMA_DEBUGFS_RING_COL_WIDTH + 1];

	seq_printf(m, "\nTX_CMPL_RING_STATS (Rings %d-%d):\n",
		   ring_ids[0], ring_ids[count - 1]);

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "Field/Errors");
	for (i = 0; i < count; i++) {
		snprintf(ring_header, sizeof(ring_header), "Ring_%d", ring_ids[i]);
		seq_printf(m, " %*s", EDMA_DEBUGFS_RING_COL_WIDTH, ring_header);
	}
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "invalid_buffer");
	for (i = 0; i < count; i++)
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[ring_ids[i]].invalid_buffer);
	seq_printf(m, "\n");

	for (j = 0; j < EDMA_TX_CMPL_ERR_MAX; j++) {
		if (strcmp(edma_txcmpl_err_string[j], "Reserved") == 0)
			continue;
		seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH,
			   edma_txcmpl_err_string[j]);
		for (i = 0; i < count; i++)
			seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
				   stats[ring_ids[i]].errors[j]);
		seq_printf(m, "\n");
	}

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "desc_with_more_bit");
	for (i = 0; i < count; i++)
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[ring_ids[i]].desc_with_more_bit);
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "no_pending_desc");
	for (i = 0; i < count; i++)
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[ring_ids[i]].no_pending_desc);
	seq_printf(m, "\n");
}

/*
 * edma_debugfs_print_tx_desc_section()
 *	Print TX descriptor ring stats
 */
static void edma_debugfs_print_tx_desc_section(struct seq_file *m,
				   struct edma_tx_desc_stats *stats,
				   uint32_t *ring_ids,
				   uint32_t count)
{
	uint32_t i, j;
	char ring_header[EDMA_DEBUGFS_RING_COL_WIDTH + 1];

	seq_printf(m, "\nTX_DESC_RING_STATS (Rings %d-%d):\n",
		   ring_ids[0], ring_ids[count - 1]);

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "Field");
	for (i = 0; i < count; i++) {
		snprintf(ring_header, sizeof(ring_header), "Ring_%d", ring_ids[i]);
		seq_printf(m, " %*s", EDMA_DEBUGFS_RING_COL_WIDTH, ring_header);
	}
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "no_desc_avail");
	for (i = 0; i < count; i++)
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[ring_ids[i]].no_desc_avail);
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "tso_max_seg_exceed");
	for (i = 0; i < count; i++)
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[ring_ids[i]].tso_max_seg_exceed);
	seq_printf(m, "\n");

#ifdef NSS_DP_TX_SMALL_PACKET_WAR
	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "tx_pad_fail");
	for (i = 0; i < count; i++)
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[ring_ids[i]].tx_pad_fail);
	seq_printf(m, "\n");
#endif

	for (j = 0; j < EDMA_RING_USAGE_MAX_FULL; j++) {
		seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH,
			   edma_debugfs_ring_usage_dump[j]);
		for (i = 0; i < count; i++)
			seq_printf(m, " %*d", EDMA_DEBUGFS_RING_COL_WIDTH,
				   stats[ring_ids[i]].ring_stats.util[j]);
		seq_printf(m, "\n");
	}
}

/*
 * edma_debugfs_rx_rings_stats_show()
 *	EDMA debugfs rx rings stats show API
 */
static int edma_debugfs_rx_rings_stats_show(struct seq_file *m, void __attribute__((unused))*p)
{
	struct edma_rx_fill_stats *rx_fill_stats;
	struct edma_rx_desc_stats *rx_desc_stats;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	struct edma_rxfill_ring *rxfill_ring;
	struct edma_rxdesc_ring *rxdesc_ring;
	struct edma_rx_fill_stats *fill_stats;
	struct edma_rx_desc_stats *desc_stats;
	uint32_t i, j, ring_id_cnt;
	uint32_t ring_ids[EDMA_DEBUGFS_RINGS_PER_SECTION];
	unsigned int start;
#ifdef NSS_DP_PPEDS_SUPPORT
	uint32_t ppeds_idx, valid_count;
	char ring_header[EDMA_DEBUGFS_RING_COL_WIDTH + 1];
	struct edma_ppeds_node_wifi8 *ppeds_node_wifi8_cfg;
	uint16_t rxfill_prod_idx, rxfill_cons_idx, rxfill_filled;
	struct edma_ppeds_node_wifi7 *ppeds_node_cfg;
	struct edma_ppeds_drv *drv = &edma_gbl_ctx->ppeds_drv;
	struct edma_ppeds *ppeds_node;
#endif

	rx_fill_stats = kzalloc(egc->rxfill_ring_max * sizeof(struct edma_rx_fill_stats),
				 GFP_KERNEL);
	if (!rx_fill_stats) {
		edma_err("Error in allocating the Rx fill stats buffer\n");
		return -ENOMEM;
	}

	rx_desc_stats = kzalloc(egc->rxdesc_ring_max * sizeof(struct edma_rx_desc_stats),
				 GFP_KERNEL);
	if (!rx_desc_stats) {
		edma_err("Error in allocating the Rx descriptor stats buffer\n");
		kfree(rx_fill_stats);
		return -ENOMEM;
	}

	/*
	 * Get stats for Rx fill rings
	 */
	for (i = 0; i < egc->rxfill_ring_max; i++) {
		if (!(egc->rxfill_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IS_CONFIGURED))
			continue;

		rxfill_ring = egc->rxfill_info[i].rxfill_ring;
		if (!rxfill_ring)
			continue;

		fill_stats = &rxfill_ring->rx_fill_stats;
		do {
			start = edma_dp_stats_fetch_begin(&fill_stats->syncp);
			rx_fill_stats[i].alloc_failed = fill_stats->alloc_failed;
			rx_fill_stats[i].page_alloc_failed = fill_stats->page_alloc_failed;
			memcpy(&rx_fill_stats[i].ring_stats, &fill_stats->ring_stats,
					sizeof(struct edma_ring_util_stats));
		} while (edma_dp_stats_fetch_retry(&fill_stats->syncp, start));
	}

	/*
	 * Get stats for Rx Desc rings
	 */
	for (i = 0; i < egc->rxdesc_ring_max; i++) {
		if (!(egc->rxdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IS_CONFIGURED))
			continue;

		rxdesc_ring = egc->rxdesc_info[i].rxdesc_ring;
		if (!rxdesc_ring)
			continue;

		desc_stats = &rxdesc_ring->rx_desc_stats;
		do {
			start = edma_dp_stats_fetch_begin(&desc_stats->syncp);
			rx_desc_stats[i].src_port_inval = desc_stats->src_port_inval;
			rx_desc_stats[i].src_port_inval_type = desc_stats->src_port_inval_type;
			rx_desc_stats[i].src_port_inval_netdev = desc_stats->src_port_inval_netdev;
			rx_desc_stats[i].rx_napi_sched = desc_stats->rx_napi_sched;
			rx_desc_stats[i].payload_buf_alloc_failed = desc_stats->payload_buf_alloc_failed;
			memcpy(&rx_desc_stats[i].ring_stats, &desc_stats->ring_stats,
					sizeof(struct edma_ring_util_stats));
		} while (edma_dp_stats_fetch_retry(&desc_stats->syncp, start));
	}

	edma_debugfs_print_banner(m, EDMA_RX_RING_STATS_NODE_NAME);

	ring_id_cnt = 0;
	for (i = 0; i < egc->rxdesc_ring_max; i++) {
		if (!(egc->rxdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IS_CONFIGURED))
			continue;
		ring_ids[ring_id_cnt++] = i;
		if (ring_id_cnt == EDMA_DEBUGFS_RINGS_PER_SECTION) {
			edma_debugfs_print_rx_desc_section(m, rx_desc_stats,
							   ring_ids, ring_id_cnt);
			ring_id_cnt = 0;
		}
	}
	if (ring_id_cnt > 0)
		edma_debugfs_print_rx_desc_section(m, rx_desc_stats,
						   ring_ids, ring_id_cnt);

	ring_id_cnt = 0;
	for (i = 0; i < egc->rxfill_ring_max; i++) {
		if (!(egc->rxfill_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IS_CONFIGURED))
			continue;
		ring_ids[ring_id_cnt++] = i;
		if (ring_id_cnt == EDMA_DEBUGFS_RINGS_PER_SECTION) {
			edma_debugfs_print_rx_fill_section(m, rx_fill_stats,
							   ring_ids, ring_id_cnt);
			ring_id_cnt = 0;
		}
	}
	if (ring_id_cnt > 0)
		edma_debugfs_print_rx_fill_section(m, rx_fill_stats,
						   ring_ids, ring_id_cnt);

#ifdef NSS_DP_PPEDS_SUPPORT
	edma_debugfs_print_banner(m, EDMA_RX_RING_PPEDS_STATS_NODE_NAME);

	/* Count valid PPE-DS wifi7 nodes */
	valid_count = 0;
	for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
		ppeds_node = drv->ppeds_node_cfg[ppeds_idx].ppeds_db;
		if (ppeds_node &&
		    ppeds_node->wifi_arch_mode == EDMA_PPEDS_WIFI_ARCH_MODE_WIFI7)
			valid_count++;
	}

	if (!valid_count)
		goto ppeds_rx_done;

	seq_printf(m, "\nPPE-DS RX_FILL_RING_STATS:\n");
	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "Field");
	for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
		ppeds_node = drv->ppeds_node_cfg[ppeds_idx].ppeds_db;
		if (!ppeds_node ||
		    ppeds_node->wifi_arch_mode != EDMA_PPEDS_WIFI_ARCH_MODE_WIFI7)
			continue;
		ppeds_node_cfg = &ppeds_node->wifi7_cfg;
		rxfill_ring = &ppeds_node_cfg->rxfill_ring;
		snprintf(ring_header, sizeof(ring_header), "Ring_%d",
			 rxfill_ring->ring_id);
		seq_printf(m, " %*s", EDMA_DEBUGFS_RING_COL_WIDTH, ring_header);
	}
	seq_printf(m, "\n");

	for (j = 0; j < EDMA_RING_USAGE_MAX_FULL; j++) {
		seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, edma_debugfs_ring_usage_rx_fill_dump[j]);
		for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
			ppeds_node = drv->ppeds_node_cfg[ppeds_idx].ppeds_db;
			if (!ppeds_node ||
			    ppeds_node->wifi_arch_mode != EDMA_PPEDS_WIFI_ARCH_MODE_WIFI7)
				continue;
			ppeds_node_cfg = &ppeds_node->wifi7_cfg;
			rxfill_ring = &ppeds_node_cfg->rxfill_ring;
			seq_printf(m, " %*d", EDMA_DEBUGFS_RING_COL_WIDTH,
				rxfill_ring->rx_fill_stats.ring_stats.util[j]);
		}
		seq_printf(m, "\n");
	}

	seq_printf(m, "\nPPE-DS RX_DESC_RING_STATS:\n");
	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "Field");
	for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
		ppeds_node = drv->ppeds_node_cfg[ppeds_idx].ppeds_db;
		if (!ppeds_node ||
		    ppeds_node->wifi_arch_mode != EDMA_PPEDS_WIFI_ARCH_MODE_WIFI7)
			continue;
		ppeds_node_cfg = &ppeds_node->wifi7_cfg;
		rxdesc_ring = &ppeds_node_cfg->rx_ring;
		snprintf(ring_header, sizeof(ring_header), "Ring_%d",
			 rxdesc_ring->ring_id);
		seq_printf(m, " %*s", EDMA_DEBUGFS_RING_COL_WIDTH, ring_header);
	}
	seq_printf(m, "\n");

	for (j = 0; j < EDMA_RING_USAGE_MAX_FULL; j++) {
		seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, edma_debugfs_ring_usage_dump[j]);
		for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
			ppeds_node = drv->ppeds_node_cfg[ppeds_idx].ppeds_db;
			if (!ppeds_node ||
			    ppeds_node->wifi_arch_mode != EDMA_PPEDS_WIFI_ARCH_MODE_WIFI7)
				continue;
			ppeds_node_cfg = &ppeds_node->wifi7_cfg;
			rxdesc_ring = &ppeds_node_cfg->rx_ring;
			seq_printf(m, " %*d", EDMA_DEBUGFS_RING_COL_WIDTH,
				rxdesc_ring->rx_desc_stats.ring_stats.util[j]);
		}
		seq_printf(m, "\n");
	}
ppeds_rx_done:

	/* PPE-DS wifi8: dump rxfill filled size per node */
	seq_printf(m, "\nPPE-DS WIFI8 RXFILL_FILLED_SIZE:\n");
	seq_printf(m, "%-*s %*s %*s %*s\n",
		   EDMA_DEBUGFS_FIELD_WIDTH, "Ring",
		   EDMA_DEBUGFS_RING_COL_WIDTH, "prod_idx",
		   EDMA_DEBUGFS_RING_COL_WIDTH, "cons_idx",
		   EDMA_DEBUGFS_RING_COL_WIDTH, "filled_size");

	for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
		ppeds_node = drv->ppeds_node_cfg[ppeds_idx].ppeds_db;
		if (!ppeds_node ||
		    ppeds_node->wifi_arch_mode != EDMA_PPEDS_WIFI_ARCH_MODE_WIFI8)
			continue;
		ppeds_node_wifi8_cfg = &ppeds_node->wifi8_cfg;

		if (ppeds_node->ppeds_handle.wifi8_hdl.hw_buff_mgmt_en) {
			rxfill_ring = &ppeds_node_wifi8_cfg->hw_buf_mgmt.rxfill_ring;
			if (rxfill_ring->count && rxfill_ring->hw_ring_size) {
				uint32_t fmt = edma_reg_read(EDMA_REG_RXFILL_FORMAT(rxfill_ring->ring_id))
						& EDMA_RXFILL_FORMAT_MASK;
				uint32_t step = (fmt == EDMA_RXFILL_FORMAT_8B) ? 2 : 1;
				rxfill_prod_idx = edma_reg_read(
						EDMA_REG_RXFILL_PROD_IDX(rxfill_ring->ring_id))
						& EDMA_RXFILL_PROD_IDX_MASK;
				rxfill_cons_idx = edma_reg_read(
						EDMA_REG_RXFILL_CONS_IDX(rxfill_ring->ring_id))
						& EDMA_RXFILL_CONS_IDX_MASK;
				rxfill_filled = ((rxfill_prod_idx - rxfill_cons_idx + rxfill_ring->hw_ring_size)
						& (rxfill_ring->hw_ring_size - 1)) / step;
				snprintf(ring_header, sizeof(ring_header), "HW_Ring_%d", rxfill_ring->ring_id);
				seq_printf(m, "%-*s %*u %*u %*u\n",
					   EDMA_DEBUGFS_FIELD_WIDTH, ring_header,
					   EDMA_DEBUGFS_RING_COL_WIDTH, rxfill_prod_idx,
					   EDMA_DEBUGFS_RING_COL_WIDTH, rxfill_cons_idx,
					   EDMA_DEBUGFS_RING_COL_WIDTH, rxfill_filled);
			}
		}

		rxfill_ring = &ppeds_node_wifi8_cfg->rxfill_ring;
		if (rxfill_ring->count && rxfill_ring->hw_ring_size) {
			rxfill_prod_idx = rxfill_ring->prod_idx;
			rxfill_cons_idx = edma_reg_read(
					EDMA_REG_RXFILL_CONS_IDX(rxfill_ring->ring_id))
					& EDMA_RXFILL_CONS_IDX_MASK;
			{
				uint32_t fmt = edma_reg_read(EDMA_REG_RXFILL_FORMAT(rxfill_ring->ring_id))
						& EDMA_RXFILL_FORMAT_MASK;
				uint32_t step = (fmt == EDMA_RXFILL_FORMAT_8B) ? 2 : 1;
				rxfill_filled = ((rxfill_prod_idx - rxfill_cons_idx + rxfill_ring->hw_ring_size)
						& (rxfill_ring->hw_ring_size - 1)) / step;
			}
			snprintf(ring_header, sizeof(ring_header), "SW_Ring_%d", rxfill_ring->ring_id);
			seq_printf(m, "%-*s %*u %*u %*u\n",
				   EDMA_DEBUGFS_FIELD_WIDTH, ring_header,
				   EDMA_DEBUGFS_RING_COL_WIDTH, rxfill_prod_idx,
				   EDMA_DEBUGFS_RING_COL_WIDTH, rxfill_cons_idx,
				   EDMA_DEBUGFS_RING_COL_WIDTH, rxfill_filled);
		}
	}
#endif
	kfree(rx_fill_stats);
	kfree(rx_desc_stats);
	return 0;
}

/*
 * edma_debugfs_tx_rings_stats_show()
 *	EDMA debugfs Tx rings stats show API
 */
static int edma_debugfs_tx_rings_stats_show(struct seq_file *m, void __attribute__((unused))*p)
{
	struct edma_tx_cmpl_stats *tx_cmpl_stats;
	struct edma_tx_desc_stats *tx_desc_stats;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	struct edma_txdesc_ring *txdesc_ring;
	struct edma_txcmpl_ring *txcmpl_ring;
	struct edma_tx_desc_stats *tx_desc_stats_ptr;
	struct edma_tx_cmpl_stats *tx_cmpl_stats_ptr;
	uint32_t i, j, ring_id_cnt;
	uint32_t ring_ids[EDMA_DEBUGFS_RINGS_PER_SECTION];
	unsigned int start;
#ifdef NSS_DP_PPEDS_SUPPORT
	uint32_t ppeds_idx, valid_count;
	char ring_header[EDMA_DEBUGFS_RING_COL_WIDTH + 1];
	struct edma_ppeds_node_wifi7 *ppeds_node_cfg;
	struct edma_txdesc_ring *tx_ring;
	struct edma_ppeds_drv *drv = &edma_gbl_ctx->ppeds_drv;
	struct edma_ppeds *ppeds_node;
#endif

	tx_cmpl_stats = kzalloc(egc->txcmpl_ring_max * sizeof(struct edma_tx_cmpl_stats), GFP_KERNEL);
	if (!tx_cmpl_stats) {
		edma_err("Error in allocating the Tx complete stats buffer\n");
		return -ENOMEM;
	}

	tx_desc_stats = kzalloc(egc->txdesc_ring_max * sizeof(struct edma_tx_desc_stats), GFP_KERNEL);
	if (!tx_desc_stats) {
		edma_err("Error in allocating the Tx descriptor stats buffer\n");
		kfree(tx_cmpl_stats);
		return -ENOMEM;
	}

	/*
	 * Get stats for Tx desc rings
	 */
	for (i = 0; i < egc->txdesc_ring_max; i++) {
		if (!(egc->txdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IS_CONFIGURED))
			continue;

		txdesc_ring = egc->txdesc_info[i].txdesc_ring;
		if (!txdesc_ring)
			continue;

		tx_desc_stats_ptr = &txdesc_ring->tx_desc_stats;
		do {
			start = edma_dp_stats_fetch_begin(&tx_desc_stats_ptr->syncp);
			tx_desc_stats[i].no_desc_avail = tx_desc_stats_ptr->no_desc_avail;
			tx_desc_stats[i].tso_max_seg_exceed = tx_desc_stats_ptr->tso_max_seg_exceed;
#ifdef NSS_DP_TX_SMALL_PACKET_WAR
			tx_desc_stats[i].tx_pad_fail = tx_desc_stats_ptr->tx_pad_fail;
#endif
			memcpy(&tx_desc_stats[i].ring_stats, &tx_desc_stats_ptr->ring_stats,
			       sizeof(struct edma_ring_util_stats));
		} while (edma_dp_stats_fetch_retry(&tx_desc_stats_ptr->syncp, start));
	}

	/*
	 * Get stats for Tx Complete rings
	 */
	for (i = 0; i < egc->txcmpl_ring_max; i++) {
		if (!(egc->txcmpl_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IS_CONFIGURED))
			continue;

		txcmpl_ring = egc->txcmpl_info[i].txcmpl_ring;
		if (!txcmpl_ring)
			continue;

		tx_cmpl_stats_ptr = &txcmpl_ring->tx_cmpl_stats;
		do {
			start = edma_dp_stats_fetch_begin(&tx_cmpl_stats_ptr->syncp);
			tx_cmpl_stats[i].invalid_buffer = tx_cmpl_stats_ptr->invalid_buffer;
			memcpy(tx_cmpl_stats[i].errors, tx_cmpl_stats_ptr->errors, sizeof(uint64_t) * EDMA_TX_CMPL_ERR_MAX);
			tx_cmpl_stats[i].desc_with_more_bit = tx_cmpl_stats_ptr->desc_with_more_bit;
			tx_cmpl_stats[i].no_pending_desc = tx_cmpl_stats_ptr->no_pending_desc;
		} while (edma_dp_stats_fetch_retry(&tx_cmpl_stats_ptr->syncp, start));
	}

	edma_debugfs_print_banner(m, EDMA_TX_RING_STATS_NODE_NAME);

	ring_id_cnt = 0;
	for (i = 0; i < egc->txcmpl_ring_max; i++) {
		if (!(egc->txcmpl_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IS_CONFIGURED))
			continue;
		ring_ids[ring_id_cnt++] = i;
		if (ring_id_cnt == EDMA_DEBUGFS_RINGS_PER_SECTION) {
			edma_debugfs_print_tx_cmpl_section(m, tx_cmpl_stats,
							   ring_ids, ring_id_cnt);
			ring_id_cnt = 0;
		}
	}
	if (ring_id_cnt > 0)
		edma_debugfs_print_tx_cmpl_section(m, tx_cmpl_stats,
						   ring_ids, ring_id_cnt);

	ring_id_cnt = 0;
	for (i = 0; i < egc->txdesc_ring_max; i++) {
		if (!(egc->txdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IS_CONFIGURED))
			continue;
		ring_ids[ring_id_cnt++] = i;
		if (ring_id_cnt == EDMA_DEBUGFS_RINGS_PER_SECTION) {
			edma_debugfs_print_tx_desc_section(m, tx_desc_stats,
							   ring_ids, ring_id_cnt);
			ring_id_cnt = 0;
		}
	}
	if (ring_id_cnt > 0)
		edma_debugfs_print_tx_desc_section(m, tx_desc_stats,
						   ring_ids, ring_id_cnt);

#ifdef NSS_DP_PPEDS_SUPPORT
	edma_debugfs_print_banner(m, EDMA_TX_RING_PPEDS_STATS_NODE_NAME);

	/* Count valid PPE-DS wifi7 nodes */
	valid_count = 0;
	for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
		ppeds_node = drv->ppeds_node_cfg[ppeds_idx].ppeds_db;
		if (ppeds_node &&
		    ppeds_node->wifi_arch_mode == EDMA_PPEDS_WIFI_ARCH_MODE_WIFI7)
			valid_count++;
	}

	if (!valid_count)
		goto ppeds_tx_done;

	seq_printf(m, "\nPPE-DS TX_DESC_RING_STATS:\n");
	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "Field");
	for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
		ppeds_node = drv->ppeds_node_cfg[ppeds_idx].ppeds_db;
		if (!ppeds_node ||
		    ppeds_node->wifi_arch_mode != EDMA_PPEDS_WIFI_ARCH_MODE_WIFI7)
			continue;
		ppeds_node_cfg = &ppeds_node->wifi7_cfg;
		tx_ring = &ppeds_node_cfg->tx_ring;
		snprintf(ring_header, sizeof(ring_header), "Ring_%d", tx_ring->id);
		seq_printf(m, " %*s", EDMA_DEBUGFS_RING_COL_WIDTH, ring_header);
	}
	seq_printf(m, "\n");

	for (j = 0; j < EDMA_RING_USAGE_MAX_FULL; j++) {
		seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, edma_debugfs_ring_usage_dump[j]);
		for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
			ppeds_node = drv->ppeds_node_cfg[ppeds_idx].ppeds_db;
			if (!ppeds_node ||
			    ppeds_node->wifi_arch_mode != EDMA_PPEDS_WIFI_ARCH_MODE_WIFI7)
				continue;
			ppeds_node_cfg = &ppeds_node->wifi7_cfg;
			tx_ring = &ppeds_node_cfg->tx_ring;
			seq_printf(m, " %*d", EDMA_DEBUGFS_RING_COL_WIDTH,
				tx_ring->tx_desc_stats.ring_stats.util[j]);
		}
		seq_printf(m, "\n");
	}

	seq_printf(m, "\nPPE-DS TX_CMPL_RING_STATS:\n");
	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "Field");
	for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
		ppeds_node = drv->ppeds_node_cfg[ppeds_idx].ppeds_db;
		if (!ppeds_node ||
		    ppeds_node->wifi_arch_mode != EDMA_PPEDS_WIFI_ARCH_MODE_WIFI7)
			continue;
		ppeds_node_cfg = &ppeds_node->wifi7_cfg;
		txcmpl_ring = &ppeds_node_cfg->txcmpl_ring;
		snprintf(ring_header, sizeof(ring_header), "Ring_%d", txcmpl_ring->id);
		seq_printf(m, " %*s", EDMA_DEBUGFS_RING_COL_WIDTH, ring_header);
	}
	seq_printf(m, "\n");

	for (j = 0; j < EDMA_RING_USAGE_MAX_FULL; j++) {
		seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, edma_debugfs_ring_usage_dump[j]);
		for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
			ppeds_node = drv->ppeds_node_cfg[ppeds_idx].ppeds_db;
			if (!ppeds_node ||
			    ppeds_node->wifi_arch_mode != EDMA_PPEDS_WIFI_ARCH_MODE_WIFI7)
				continue;
			ppeds_node_cfg = &ppeds_node->wifi7_cfg;
			txcmpl_ring = &ppeds_node_cfg->txcmpl_ring;
			seq_printf(m, " %*d", EDMA_DEBUGFS_RING_COL_WIDTH,
				txcmpl_ring->tx_cmpl_stats.ring_stats.util[j]);
		}
		seq_printf(m, "\n");
	}
ppeds_tx_done:
#endif

	kfree(tx_cmpl_stats);
	kfree(tx_desc_stats);
	return 0;
}

/*
 * edma_debugfs_misc_stats_show()
 *	EDMA debugfs miscellaneous stats show API
 */
static int edma_debugfs_misc_stats_show(struct seq_file *m, void __attribute__((unused))*p)
{
	struct edma_misc_stats *misc_stats, *pcpu_misc_stats;
	uint32_t cpu;
	unsigned int start;

	misc_stats = kzalloc(sizeof(struct edma_misc_stats), GFP_KERNEL);
	if (!misc_stats) {
		edma_err("Error in allocating the miscellaneous stats buffer\n");
		return -ENOMEM;
	}

	/*
	 * Get percpu EDMA miscellaneous stats
	 */
	for_each_possible_cpu(cpu) {
		pcpu_misc_stats = per_cpu_ptr(edma_gbl_ctx->misc_stats, cpu);
		do {
			start = edma_dp_stats_fetch_begin(&pcpu_misc_stats->syncp);
			misc_stats->edma_misc_axi_read_err +=
				pcpu_misc_stats->edma_misc_axi_read_err;
			misc_stats->edma_misc_axi_write_err +=
				pcpu_misc_stats->edma_misc_axi_write_err;
			misc_stats->edma_misc_rx_desc_fifo_full +=
				pcpu_misc_stats->edma_misc_rx_desc_fifo_full;
			misc_stats->edma_misc_rx_buf_size_err +=
				pcpu_misc_stats->edma_misc_rx_buf_size_err;
			misc_stats->edma_misc_tx_sram_full +=
				pcpu_misc_stats->edma_misc_tx_sram_full;
			misc_stats->edma_misc_tx_data_len_err +=
				pcpu_misc_stats->edma_misc_tx_data_len_err;
			misc_stats->edma_misc_tx_timeout +=
				pcpu_misc_stats->edma_misc_tx_timeout;
			misc_stats->edma_misc_tx_cmpl_buf_full +=
				pcpu_misc_stats->edma_misc_tx_cmpl_buf_full;
			misc_stats->edma_misc_pass_thr_err_fwd +=
				pcpu_misc_stats->edma_misc_pass_thr_err_fwd;
			misc_stats->edma_misc_txq_passthr_offset_miss +=
				pcpu_misc_stats->edma_misc_txq_passthr_offset_miss;
			misc_stats->edma_misc_txq_ds_cmpl_err +=
				pcpu_misc_stats->edma_misc_txq_ds_cmpl_err;
		} while (edma_dp_stats_fetch_retry(&pcpu_misc_stats->syncp, start));
	}

	edma_debugfs_print_banner(m, EDMA_MISC_STATS_NODE_NAME);

	seq_printf(m, "\n#EDMA miscellaneous stats:\n\n");
	seq_printf(m, "\t\t miscellaneous axi read error = %llu\n",
			misc_stats->edma_misc_axi_read_err);
	seq_printf(m, "\t\t miscellaneous axi write error = %llu\n",
			misc_stats->edma_misc_axi_write_err);
	seq_printf(m, "\t\t miscellaneous Rx descriptor fifo full = %llu\n",
			misc_stats->edma_misc_rx_desc_fifo_full);
	seq_printf(m, "\t\t miscellaneous Rx buffer size error = %llu\n",
			misc_stats->edma_misc_rx_buf_size_err);
	seq_printf(m, "\t\t miscellaneous Tx SRAM full = %llu\n",
			misc_stats->edma_misc_tx_sram_full);
	seq_printf(m, "\t\t miscellaneous Tx data length error = %llu\n",
			misc_stats->edma_misc_tx_data_len_err);
	seq_printf(m, "\t\t miscellaneous Tx timeout = %llu\n",
			misc_stats->edma_misc_tx_timeout);
	seq_printf(m, "\t\t miscellaneous Tx completion buffer full = %llu\n",
			misc_stats->edma_misc_tx_cmpl_buf_full);
	seq_printf(m, "\t\t miscellaneous Pass through packet forward error = %llu\n",
			misc_stats->edma_misc_pass_thr_err_fwd);
	seq_printf(m, "\t\t miscellaneous TXQ pass through offset miss error = %llu\n",
			misc_stats->edma_misc_txq_passthr_offset_miss);
	seq_printf(m, "\t\t miscellaneous TXQ DS CMPL error = %llu\n",
			misc_stats->edma_misc_txq_ds_cmpl_err);

	kfree(misc_stats);
	return 0;
}

/*
 * edma_debugfs_clear_ring_stats()
 *      EDMA debugfs clearing the ring stats
 */
static int edma_debugfs_clear_ring_stats(struct seq_file *m, void __attribute__((unused))*p)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	uint32_t i;
#ifdef NSS_DP_PPEDS_SUPPORT
	struct edma_ppeds_drv *drv = &edma_gbl_ctx->ppeds_drv;
	struct edma_ppeds *ppeds_node;
#endif

	for (i = 0; i < egc->rxfill_ring_max; i++) {
		if (egc->rxfill_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IS_CONFIGURED)
			memset(&egc->rxfill_info[i].rxfill_ring->rx_fill_stats, 0, sizeof(struct edma_rx_fill_stats));
	}

	for (i = 0; i < edma_gbl_ctx->rxdesc_ring_max; i++) {
		if (egc->rxdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IS_CONFIGURED)
			memset(&egc->rxdesc_info[i].rxdesc_ring->rx_desc_stats, 0, sizeof(struct edma_rx_desc_stats));
	}

	for (i = 0; i < egc->txdesc_ring_max; i++) {
		if (egc->txdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IS_CONFIGURED)
			memset(&egc->txdesc_info[i].txdesc_ring->tx_desc_stats, 0, sizeof(struct edma_tx_desc_stats));
	}

#ifdef NSS_DP_PPEDS_SUPPORT
	for (i = 0; i < drv->num_nodes; i++) {
		struct edma_ppeds_node_wifi7 *ppeds_node_cfg;
		ppeds_node = drv->ppeds_node_cfg[i].ppeds_db;
		if (!ppeds_node) {
			continue;
		}

		if (ppeds_node->wifi_arch_mode != EDMA_PPEDS_WIFI_ARCH_MODE_WIFI7) {
			continue;
		}

		ppeds_node_cfg = &ppeds_node->wifi7_cfg;
		memset(&ppeds_node_cfg->rxfill_ring.rx_fill_stats, 0, sizeof(struct edma_rx_fill_stats));
		memset(&ppeds_node_cfg->rx_ring.rx_desc_stats, 0, sizeof(struct edma_rx_desc_stats));
		memset(&ppeds_node_cfg->tx_ring.tx_desc_stats, 0, sizeof(struct edma_tx_desc_stats));
		memset(&ppeds_node_cfg->txcmpl_ring.tx_cmpl_stats, 0, sizeof(struct edma_tx_cmpl_stats));
	}
#endif

	seq_printf(m, "Resetting the EDMA Ring stats\n");
	return 0;
}

#ifdef NSS_DP_HW_GRO
/*
 * edma_debugfs_hw_gro_stats_show()
 *	EDMA debugfs hw_gro stats show API
 */
static int edma_debugfs_hw_gro_stats_show(struct seq_file *m, void __attribute__((unused))*p)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	seq_printf(m, "\t\t EDMA HW_GRO timeout = %d\n", egc->hw_gro_ctx.gro_timeout_usecs);
	seq_printf(m, "\t\t EDMA HW_GRO buffer len = %d\n", egc->hw_gro_ctx.gro_buffer_len);
	seq_printf(m, "\t\t EDMA HW_GRO descriptor count = %d\n", egc->hw_gro_ctx.gro_desc_count);
	return 0;
}
#endif

#ifdef NSS_DP_DDRQ_SUPPORT
/*
 * edma_debugfs_ddrq_port_en_bitmask_show()
 *	EDMA debugfs DDRQ port enable bitmask show API
 */
static int edma_debugfs_ddrq_port_en_bitmask_show(struct seq_file *m, void __attribute__((unused))*p)
{
	seq_printf(m, "\t\t EDMA DDRQ port enable bitmask = 0x%0x\n", edma_ddrq_en_port_bm);
	return 0;
}

/*
 * edma_debugfs_ddrq_blk_num_show()
 *	EDMA debugfs DDRQ block number show API
 */
static int edma_debugfs_ddrq_blk_num_show(struct seq_file *m, void __attribute__((unused))*p)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	edma_ddrq_gbl_cfg_t *ddrq_gbl_cfg = &egc->ddrq_def_cfg.ddrq_gbl_cfg;
	int ddrq_blk_num_cfg = ddrq_gbl_cfg->ddrq_blk_num_cfg;

	seq_printf(m, "\t\t EDMA DDRQ block number = %d\n", edma_ddrq_blk_num_map[ddrq_blk_num_cfg]);
	return 0;
}

/*
 * edma_debugfs_ddrq_blk_size_show()
 *	EDMA debugfs DDRQ block size show API
 */
static int edma_debugfs_ddrq_blk_size_show(struct seq_file *m, void __attribute__((unused))*p)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	edma_ddrq_gbl_cfg_t *ddrq_gbl_cfg = &egc->ddrq_def_cfg.ddrq_gbl_cfg;
	int ddrq_blk_size_cfg = ddrq_gbl_cfg->ddrq_blk_size_cfg;

	seq_printf(m, "\t\t EDMA DDRQ block size = %d\n", edma_ddrq_blk_size_map[ddrq_blk_size_cfg]);
	return 0;
}
#endif

#if defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
/*
 * edma_debugfs_loopback_stats_show()
 *	EDMA debugfs loopback stats show API
 */
static int edma_debugfs_loopback_stats_show(struct seq_file *m, void __attribute__((unused))*p)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	int i = 0;

	seq_printf(m, "\n#EDMA loopback configuration stats:\n\n");
	seq_printf(m, "\t\t Loopback enabled = %d\n", egc->loopback_en);
	seq_printf(m, "\t\t Loopback ring size = %d\n", egc->loopback_ring_size);
	seq_printf(m, "\t\t Loopback buffer size = %d\n",egc->loopback_buf_size);
	seq_printf(m, "\t\t Number of loopback rings = %d\n", egc->num_loopback_rings);
	seq_printf(m, "\t\t Loopback queue base = %d\n", egc->loopback_queue_base);
	seq_printf(m, "\t\t Number of loopback queues = %d\n", egc->loopback_num_queues);

	for (i = 0; i < egc->num_loopback_rings; i++) {
		seq_printf(m, "\t\t#EDMA loopback ring info: %d\n\n", i);
		seq_printf(m, "\t\t TX descriptor loopback_ring_id = %d\n", egc->txdesc_loopback_ring_id_arr[i]);
		seq_printf(m, "\t\t TX completion loopback_ring_id = %d\n", egc->txcmpl_loopback_ring_id_arr[i]);
		seq_printf(m, "\t\t RX descriptor loopback_ring_id = %d\n", egc->rxdesc_loopback_ring_id_arr[i]);
		seq_printf(m, "\t\t RX fill loopback_ring_id = %d\n", egc->rxfill_loopback_ring_id_arr[i]);
	}

	return 0;
}

/*
 * edma_debugs_loopback_stats_open()
 *	EDMA debugfs loopback stats open callback API
 */
static int edma_debugs_loopback_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_loopback_stats_show, inode->i_private);
}

/*
 * edma_debugfs_loopback_file_ops
 *	File operations for EDMA loopback stats
 */
const struct file_operations edma_debugfs_loopback_file_ops = {
	.open = edma_debugs_loopback_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
	};
#endif

#ifdef NSS_DP_HW_GRO
/*
 * edma_debugs_hw_gro_stats_open()
 *	EDMA debugfs HW gro stats open callback API
 */
static int edma_debugs_hw_gro_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_hw_gro_stats_show, inode->i_private);
}

/*
 * edma_debugfs_hw_gro_file_ops
 *	File operations for EDMA HW GRO stats
 */
const struct file_operations edma_debugfs_hw_gro_file_ops = {
	.open = edma_debugs_hw_gro_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};
#endif

#ifdef NSS_DP_DDRQ_SUPPORT
/*
 * edma_debugs_ddrq_port_en_bitmask_open()
 *	EDMA DDRQ port enable bitmask debugfs open callback API
 */
static int edma_debugs_ddrq_port_en_bitmask_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_ddrq_port_en_bitmask_show, inode->i_private);
}

/*
 * edma_debugfs_ddrq_port_en_bitmask_file_ops
 *	File operations for EDMA DDRQ port enable bitmask information
 */
const struct file_operations edma_debugfs_ddrq_port_en_bitmask_file_ops = {
	.open = edma_debugs_ddrq_port_en_bitmask_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/*
 * edma_debugs_ddrq_blk_num_open()
 *	EDMA DDRQ block number debugfs open callback API
 */
static int edma_debugs_ddrq_blk_num_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_ddrq_blk_num_show, inode->i_private);
}

/*
 * edma_debugfs_ddrq_blk_num_file_ops
 *	File operations for EDMA DDRQ block number information
 */
const struct file_operations edma_debugfs_ddrq_blk_num_file_ops = {
	.open = edma_debugs_ddrq_blk_num_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/*
 * edma_debugs_ddrq_blk_size_open()
 *	EDMA DDRQ block size debugfs open callback API
 */
static int edma_debugs_ddrq_blk_size_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_ddrq_blk_size_show, inode->i_private);
}

/*
 * edma_debugfs_ddrq_blk_size_file_ops
 *	File operations for EDMA DDRQ block size information
 */
const struct file_operations edma_debugfs_ddrq_blk_size_file_ops = {
	.open = edma_debugs_ddrq_blk_size_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};
#endif

#ifdef NSS_DP_MHT_SW_PORT_MAP
/*
 * edma_debugfs_mht_tx_fcgrp_show()
 *	EDMA debugfs tx ring fcgrp on mht ports show API
 */
static int edma_debugfs_mht_tx_fcgrp_show(struct seq_file *m, void __attribute__((unused))*p)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	struct net_device *netdev = NULL;
	struct nss_dp_dev *dp_dev = NULL;
	struct edma_txdesc_ring *tx_ring = NULL;
	uint32_t i = 0, sw_port = 0;

	seq_printf(m, "\n#EDMA MHT tx ring fc_group info (port_id : group_id):\n\n");

	for (i = 0; i < EDMA_MAX_PORTS; i++) {
		netdev = egc->netdev_arr[i];

		if (!netdev)
			continue;

		dp_dev = (struct nss_dp_dev *)netdev_priv(netdev);
		if (!dp_dev || !dp_dev->nss_dp_mht_dev)
			continue;

		for (sw_port = 0; sw_port < NSS_DP_HAL_SW_MAX_TX_PORT; sw_port++) {
			/* one port's tx rings maps with same fc_grp_id, print once */
			tx_ring = dp_dev->dp_info.txr_sw_port_map[sw_port][0];
			if (tx_ring)
				seq_printf(m, "\t\t%d:%d", (sw_port + 1), tx_ring->fc_grp_id);
		}

		seq_printf(m, "\n\n#switch_netdev:%s, macid:%d", netdev->name, dp_dev->macid);

		seq_printf(m, "\n\n");
	}

	return 0;
}

/*
 * edma_debugs_mht_tx_fcgrp_open()
 *	EDMA debugfs tx ring fcgrp on mht ports open callback API
 */
static int edma_debugs_mht_tx_fcgrp_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_mht_tx_fcgrp_show, inode->i_private);
}

/*
 * edma_debugfs_mht_tx_fcgrp_file_ops
 *	File operations for EDMA tx ring fcgrp on mht ports
 */
const struct file_operations edma_debugfs_mht_tx_fcgrp_file_ops = {
	.open = edma_debugs_mht_tx_fcgrp_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};
#endif

/*
 * edma_debugs_rx_rings_stats_open()
 *	EDMA debugfs Rx rings open callback API
 */
static int edma_debugs_rx_rings_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_rx_rings_stats_show, inode->i_private);
}

/*
 * edma_debugfs_rx_rings_file_ops
 *	File operations for EDMA Rx rings stats
 */
const struct file_operations edma_debugfs_rx_rings_file_ops = {
	.open = edma_debugs_rx_rings_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/*
 * edma_debugs_tx_rings_stats_open()
 *	EDMA debugfs Tx rings open callback API
 */
static int edma_debugs_tx_rings_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_tx_rings_stats_show, inode->i_private);
}

/*
 * edma_debugfs_tx_rings_file_ops
 *	File operations for EDMA Tx rings stats
 */
const struct file_operations edma_debugfs_tx_rings_file_ops = {
	.open = edma_debugs_tx_rings_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/*
 * edma_debugs_misc_stats_open()
 *	EDMA debugfs miscellaneous stats open callback API
 */
static int edma_debugs_misc_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_misc_stats_show, inode->i_private);
}

/*
 * edma_debugfs_misc_file_ops
 *	File operations for EDMA miscellaneous stats
 */
const struct file_operations edma_debugfs_misc_file_ops = {
	.open = edma_debugs_misc_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/* edma_debugs_clear_ring_stats()
 * 	EDMA debugfs clear stats open callback API
 */
static int edma_debugs_clear_ring_stats(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_clear_ring_stats, inode->i_private);
}

/*
 * edma_debugfs_clear_ring_stats_ops
 * 	File operations for clearing ring stats
 */
const struct file_operations edma_debugfs_clear_ring_stats_ops = {
	.open = edma_debugs_clear_ring_stats,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/*
 * edma_debugfs_bp_counter_clk_cycle_show()
 *	EDMA debugfs bp counter clk cycle show
 *
 * Show counter clk cycle.
 * 0: 64 cycle, 1: 128, 2: 256, 3: 512, 4: 1024, 5: 2048, 6: 4096, 7: 8192
 * Backpressure should be observed for whole duration of the configured cycle
 * post which backpressure counter will be incremented by 1.
 */
static int edma_debugfs_bp_counter_clk_cycle_show(struct seq_file *seq, void *v)
{
	uint32_t val = edma_reg_read(EDMA_REG_DBG_CNT_TIME);
	seq_printf(seq, "%u\n", (val & EDMA_DBG_CNT_DUR_TIME_MASK) >> EDMA_DBG_CNT_DUR_TIME_SHIFT);
	return 0;
}

/*
 * edma_debugfs_bp_counter_clk_cycle_open()
 *	EDMA debugfs bp counter clk cycle open callback
 */
static int edma_debugfs_bp_counter_clk_cycle_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_bp_counter_clk_cycle_show, inode->i_private);
}

/*
 * edma_debugfs_bp_counter_clk_cycle_write()
 *	EDMA debugfs bp counter clk cycle write callback
 *
 * Write counter clk cycle.
 * 0: 64 cycle, 1: 128, 2: 256, 3: 512, 4: 1024, 5: 2048, 6: 4096, 7: 8192
 * Backpressure should be observed for whole duration of the configured cycle
 * post which backpressure counter will be incremented by 1.
 */
static ssize_t edma_debugfs_bp_counter_clk_cycle_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char buffer[16];
	unsigned int time_val;
	uint32_t reg;

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;
	if (copy_from_user(buffer, buf, count))
		return -EFAULT;
	buffer[count] = '\0';

	if (kstrtouint(strstrip(buffer), 0, &time_val))
		return -EINVAL;
	if (time_val > EDMA_DBG_CNT_DUR_TIME_MASK)
		return -EINVAL;

	reg = edma_reg_read(EDMA_REG_DBG_CNT_TIME);
	reg &= ~EDMA_DBG_CNT_DUR_TIME_MASK;
	reg |= (time_val << EDMA_DBG_CNT_DUR_TIME_SHIFT) & EDMA_DBG_CNT_DUR_TIME_MASK;
	edma_reg_write(EDMA_REG_DBG_CNT_TIME, reg);

	return count;
}

static const struct file_operations edma_debugfs_bp_counter_clk_cycle_file_ops = {
	.open    = edma_debugfs_bp_counter_clk_cycle_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.write   = edma_debugfs_bp_counter_clk_cycle_write,
	.release = single_release,
};

/*
 * edma_debugfs_bp_counter_clean_bitmap_write()
 *	EDMA debugfs debug bp counter clean bitmap write callback
 *
 * Write bitmap [17:0], where position of bit indicate idx of bp counter
 * Set bit reset the corresponding bp counter
 */
static ssize_t edma_debugfs_bp_counter_clean_bitmap_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char buffer[32];
	unsigned int idx_or_bm;

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;
	if (copy_from_user(buffer, buf, count))
		return -EFAULT;
	buffer[count] = '\0';

	if (kstrtouint(strstrip(buffer), 0, &idx_or_bm))
		return -EINVAL;

	edma_reg_write(EDMA_REG_DBG_CNT_CLEAN, idx_or_bm & EDMA_DBG_CNT_SINGLE_CLEAN_MASK);
	return count;
}

/*
 * edma_debugfs_bp_counter_clean_bitmap_show()
 *	EDMA debugfs debug bp counter clean bitmap show
 *
 * Show bitmap [17:0], where position of bit indicate idx of bp counter
 * Set bit reset the corresponding bp counter
 */
static int edma_debugfs_bp_counter_clean_bitmap_show(struct seq_file *seq, void *v)
{
        seq_printf(seq, "%u\n", (edma_reg_read(EDMA_REG_DBG_CNT_CLEAN) & EDMA_DBG_CNT_SINGLE_CLEAN_MASK));
        return 0;
}

/*
 * edma_debugfs_bp_counter_clean_bitmap_open()
 *	EDMA debugfs debug bp counter clean bitmap open callback
 */
static int edma_debugfs_bp_counter_clean_bitmap_open(struct inode *inode, struct file *file)
{
        return single_open(file, edma_debugfs_bp_counter_clean_bitmap_show, inode->i_private);
}


static const struct file_operations edma_debugfs_bp_counter_clean_bitmap_file_ops = {
	.open    = edma_debugfs_bp_counter_clean_bitmap_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.write   = edma_debugfs_bp_counter_clean_bitmap_write,
	.release = single_release,
};

/*
 * edma_debugfs_bp_counters_enable_show()
 *	EDMA debugfs debug bp counters enable show
 *
 * 1 indicates enabled bp counters and 0 indicates disabled bp counters
 */
static int edma_debugfs_bp_counters_enable_show(struct seq_file *seq, void *v)
{
        seq_printf(seq, "%u\n", (edma_reg_read(EDMA_REG_DBG_TOTAL_GO) & EDMA_DBG_CNT_TOTAL_GO_MASK));
        return 0;
}

/*
 * edma_debugfs_bp_counters_enable_open()
 *	EDMA debugfs debug bp counters enable open callback
 */
static int edma_debugfs_bp_counters_enable_open(struct inode *inode, struct file *file)
{
        return single_open(file, edma_debugfs_bp_counters_enable_show, inode->i_private);
}

/*
 * edma_debugfs_bp_counters_enable_write()
 *	EDMA debugfs debug bp counters enable write callback
 *
 * Set 1 to enable bp counters and 0 to disable
 */
static ssize_t edma_debugfs_bp_counters_enable_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char buffer[8];
	unsigned int v;
	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;
	if (copy_from_user(buffer, buf, count))
		return -EFAULT;
	buffer[count] = '\0';
	if (kstrtouint(strstrip(buffer), 0, &v))
		return -EINVAL;

	if (v)
		edma_reg_write(EDMA_REG_DBG_TOTAL_GO, EDMA_DBG_CNT_TOTAL_GO_MASK);
	else
		edma_reg_write(EDMA_REG_DBG_TOTAL_GO, 0);

	return count;
}

static const struct file_operations edma_debugfs_bp_counters_enable_file_ops = {
	.open    = edma_debugfs_bp_counters_enable_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.write   = edma_debugfs_bp_counters_enable_write,
	.release = single_release,
};

/*
 * edma_debugfs_bp_all_counters_clean_write()
 *	EDMA debugfs debug bp all counters clean write callback
 *
 * Set 1 to reset all bp counters and 0 to enable again.
 */
static ssize_t edma_debugfs_bp_all_counters_clean_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char buffer[8];
	unsigned int v;
	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;
	if (copy_from_user(buffer, buf, count))
		return -EFAULT;
	buffer[count] = '\0';
	if (kstrtouint(strstrip(buffer), 0, &v))
		return -EINVAL;

	if (v)
		edma_reg_write(EDMA_REG_DBG_TOTAL_CLEAN, EDMA_DBG_CNT_TOTAL_CLEAN_MASK);
	else
		edma_reg_write(EDMA_REG_DBG_TOTAL_CLEAN, 0);

	return count;
}

/*
 * edma_debugfs_bp_all_counters_clean_show()
 *	EDMA debugfs debug bp all counters clean show
 *
 * 1 indicates all bp counter are resetted, set 0 to enable again
 */
static int edma_debugfs_bp_all_counters_clean_show(struct seq_file *seq, void *v)
{
        seq_printf(seq, "%u\n", (edma_reg_read(EDMA_REG_DBG_TOTAL_CLEAN) & EDMA_DBG_CNT_TOTAL_CLEAN_MASK));
        return 0;
}

/*
 * edma_debugfs_bp_all_counters_clean_open()
 *	EDMA debugfs debug bp all counters clean open callback
 */
static int edma_debugfs_bp_all_counters_clean_open(struct inode *inode, struct file *file)
{
        return single_open(file, edma_debugfs_bp_all_counters_clean_show, inode->i_private);
}

static const struct file_operations edma_debugfs_bp_all_counters_clean_file_ops = {
	.open    = edma_debugfs_bp_all_counters_clean_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.write   = edma_debugfs_bp_all_counters_clean_write,
	.release = single_release,
};

/*
 * edma_debugfs_bp_counters_show()
 *	EDMA debugfs debug bp counters show
 *
 * Show the BP Stats for the bp enabled ring ids.
 */
static int edma_debugfs_bp_counters_show(struct seq_file *seq, void *v)
{
	uint8_t mapped_ring_id;
	uint8_t ring_id;
	uint8_t idx = 0;

	while (idx < EDMA_REG_RXDESC_BP_IDX_OFFSET) {
		mapped_ring_id = edma_reg_read(EDMA_REG_DBG_CNT_PORT_MAP(idx)) & EDMA_DBG_CNT_PORT_MAP_VAL_MASK;
		ring_id = mapped_ring_id - EDMA_REG_BP_TXCMPL_RING_ID_OFFSET;
		seq_printf(seq, "BP Stats for txcmpl_%u = %u\n", ring_id, edma_reg_read(EDMA_REG_DBG_CNT_NUM(idx)));
		idx++;
	}

	while (idx < EDMA_REG_RXFILL_BP_IDX_OFFSET) {
		mapped_ring_id = edma_reg_read(EDMA_REG_DBG_CNT_PORT_MAP(idx)) & EDMA_DBG_CNT_PORT_MAP_VAL_MASK;
		ring_id = mapped_ring_id - EDMA_REG_BP_RXDESC_RING_ID_OFFSET;
		seq_printf(seq, "BP Stats for rxdesc_%u = %u\n", ring_id, edma_reg_read(EDMA_REG_DBG_CNT_NUM(idx)));
		idx++;
	}

	while (idx < EDMA_REG_BP_COUNTERS_NUM) {
		mapped_ring_id = edma_reg_read(EDMA_REG_DBG_CNT_PORT_MAP(idx)) & EDMA_DBG_CNT_PORT_MAP_VAL_MASK;
		ring_id = mapped_ring_id - EDMA_REG_BP_RXFILL_RING_ID_OFFSET;
		seq_printf(seq, "BP Stats for rxfill_%u = %u\n", ring_id, edma_reg_read(EDMA_REG_DBG_CNT_NUM(idx)));
		idx++;
	}

        return 0;
}

/*
 * edma_debugfs_bp_counters_open()
 *	EDMA debugfs debug bp counters open callback
 */
static int edma_debugfs_bp_counters_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_bp_counters_show, inode->i_private);
}

static const struct file_operations edma_debugfs_bp_counters_file_ops = {
	.open    = edma_debugfs_bp_counters_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

/*
 * edma_debugfs_bp_cnt_total_show()
 *	EDMA debugfs debug bp cnt total show
 *
 * Shows the total bp counter
 */
static int edma_debugfs_bp_cnt_total_show(struct seq_file *seq, void *v)
{
	seq_printf(seq, "%u\n", edma_reg_read(EDMA_REG_DBG_CNT_TOTAL_CNT));
	return 0;
}

/*
 * edma_debugfs_bp_cnt_total_open()
 *	EDMA debugfs debug bp cnt total open callback
 */
static int edma_debugfs_bp_cnt_total_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_bp_cnt_total_show, inode->i_private);
}

static const struct file_operations edma_debugfs_bp_cnt_total_file_ops = {
	.open    = edma_debugfs_bp_cnt_total_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

#if defined(NSS_DP_PON_SUPPORT)
/*
 * edma_debugfs_is_pon_netdev()
 *	Check whether netdev corresponds to PON (GEM) interface.
 */
static bool edma_debugfs_is_pon_netdev(struct net_device *netdev)
{
	struct nss_dp_dev *dp_dev;

	if (!netdev)
		return false;

	dp_dev = (struct nss_dp_dev *)netdev_priv(netdev);
	return dp_dev && dp_dev->gem_port;
}

/*
 * edma_debugfs_is_pon_selected()
 *	Check whether selected bitmap includes a PON (GEM) interface.
 */
static bool edma_debugfs_is_pon_selected(struct edma_gbl_ctx *egc, uint16_t bitmap)
{
	int idx;

	if (!egc || !bitmap)
		return false;

	for (idx = 0; idx < EDMA_MAX_PORTS; idx++) {
		if (!(bitmap & BIT(idx)))
			continue;

		if (edma_debugfs_is_pon_netdev(egc->netdev_arr[idx]))
			return true;
	}

	return false;
}
#endif

/*
 * edma_port_bitmap_info_show()
 *	show() callback for port_to_bitmap debugfs file.
 *	Prints the bit position and bitmap value for each valid ethernet port.
 *	Called when user does: cat port_to_bitmap
 */
static int edma_port_bitmap_info_show(struct seq_file *m, void *p)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	struct net_device *netdev;
	int idx;
	int found = 0;

	if (!egc){
		seq_printf(m, "EDMA not intialized\n");
		return -ENODEV;
	}

	seq_printf(m, "____________________________________________\n");
	seq_printf(m, "Valid Ethernet Ports on this DUT\n");
	seq_printf(m, "____________________________________________\n");
	seq_printf(m, "%-6s%-12s%-20s\n", "BIT", "PORT", "BITMAP VALUE");
	seq_printf(m, "%-6s%-12s%-20s\n", "----", "----------", "------------------");

	for (idx = 0; idx < EDMA_MAX_PORTS; idx++) {
		netdev = egc->netdev_arr[idx];
		if (!netdev){
			continue;
		}

		if (strchr(netdev->name, '%') != NULL){
			continue;
		}

#if defined(NSS_DP_PON_SUPPORT)
		if (edma_debugfs_is_pon_netdev(netdev)) {
			continue;
		}
#endif

		seq_printf(m, "%-6d%-12s0x%lx\n", idx, netdev->name, BIT(idx));
		found++;
	}

#if defined(NSS_DP_PON_SUPPORT)
	{
		int pon_idx = -1;

		for (idx = 0; idx < EDMA_MAX_PORTS; idx++) {
			netdev = egc->netdev_arr[idx];
			if (!netdev)
				continue;

			if (edma_debugfs_is_pon_netdev(netdev)) {
				pon_idx = idx;
				break;
			}
		}

		if (pon_idx >= 0) {
			seq_printf(m, "%-6d%-12s0x%lx\n", pon_idx, "PON", BIT(pon_idx));
			found++;
		}
	}
#endif

	/*
	 * If no ports were found at all, print a clear message.
	 * This should not happen on a correctly initialized board
	 * but is a useful safety net during bringup.
	 */
	if (!found) {
		seq_printf(m, "(no ethernet ports found in netdev_arr)\n");
		seq_printf(m, "____________________________________________\n");
		return 0;
	}

	return 0;
}

/*
 * edma_port_bitmap_info_open()
 *      open() callback for port_to_bitmap debugfs file.
 */
static int edma_port_bitmap_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_port_bitmap_info_show, inode->i_private);
}

/*
 * edma_port_bitmap_info_file_ops
 *      File operations for port_to_bitmap debugfs file.
 */
static const struct file_operations edma_port_bitmap_info_file_ops = {
	.open    = edma_port_bitmap_info_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

/*
 * port_stats_bitmap_gbl
 *	Stores the port bitmap written by the user via echo.
 *	Read back by edma_debugfs_port_stats_show() on cat.
 */
static uint16_t port_stats_bitmap_gbl;

/*
 * edma_debugfs_enable_queue_counters()
 *	Enable SRAM queue counters in hardware.
 *	Called once before any stats read or accumulate-stats start so that
 *	fal_queue_counter_get() returns valid data.
 */
static int edma_debugfs_enable_queue_counters(const char *caller)
{
	if (fal_queue_counter_ctrl_set(EDMA_SWITCH_DEV_ID, A_TRUE) != SW_OK) {
		edma_err("%s: failed to enable queue counters\n", caller);
		return -EIO;
	}
	return 0;
}

/*
 * edma_debugfs_sram_drop_sum()
 *	Sum all per-colour drop packet and byte counts from a fal_queue_stats_t
 *	into the caller-supplied accumulators.
 *	Callers must zero the accumulators before the first call per queue.
 */
static void edma_debugfs_sram_drop_sum(const fal_queue_stats_t *qs,
				       a_uint32_t *drop_pkts,
				       a_uint64_t *drop_bytes)
{
	int k;

	for (k = 0; k < FAL_QM_DROP_ITEMS; k++) {
		*drop_pkts  += qs->drop_packets[k];
		*drop_bytes += qs->drop_bytes[k];
	}
}

#if defined(NSS_DP_DDRQ_SUPPORT)
/*
 * ddrq_drop_pkts_baseline_gbl / ddrq_drop_bytes_baseline_gbl
 *	Per-queue DDRQ drop pkt/byte counts captured on the last flush.
 *	The DDRQ drop counters are free-running in hardware with no clear-on-write
 *	support, so flush takes a snapshot here and edma_debugfs_port_stats_show()
 *	reports (current - baseline) so counting appears to restart from zero.
 */
static uint32_t ddrq_drop_pkts_baseline_gbl[NSS_DP_DDRQ_MAX_CNT];
static uint64_t ddrq_drop_bytes_baseline_gbl[NSS_DP_DDRQ_MAX_CNT];
#endif

/*
 * edma_debugfs_port_queue_stats_show()
 *	Common per-port queue stats printer for both ETH and PON interfaces.
 */
static void edma_debugfs_port_queue_stats_show(struct seq_file *m,
					       const char *port_name,
					       int32_t hw_port,
					       int32_t queue_base,
					       uint32_t num_queues,
					       bool show_accu_stats)
{
	fal_queue_stats_t queue_stats_info;
	a_uint32_t qid;
	a_uint32_t sram_queue_drop_pkts;
	a_uint64_t sram_queue_drop_bytes;
	uint32_t j;
#if defined(NSS_DP_DDRQ_SUPPORT)
	nss_dp_ddrq_obj_id_t ddrq_obj;
	nss_dp_ddrq_ac_queue_drop_cnt_tbl_t ddrq_drop_stats;
	nss_dp_ddrq_pending_stats_t ddrq_pending_stats;
	uint32_t ddrq_drop_pkts;
	uint64_t ddrq_drop_bytes;
	nss_dp_ddrq_accu_stats_t ddrq_stats;
	nss_dp_ddrq_accu_stats_info_t stats_info;
	uint32_t accu_stats_queues = num_queues;
#else
	(void)show_accu_stats;
#endif

	seq_printf(m, "*************** PER QUEUE STATS OF PORT %s ***************\n\n", port_name);
	seq_printf(m, "------------------------------------\n");
	seq_printf(m, "Port         : %s\n", port_name);
	seq_printf(m, "HW Port ID   : %d\n", hw_port);
	seq_printf(m, "Queue Base   : %d\n", queue_base);
	seq_printf(m, "No Of Queues : %u\n", num_queues);
	seq_printf(m, "------------------------------------\n");
	seq_printf(m, "\n\n");
	seq_printf(m, "SRAM stats:\n");
	seq_printf(m, "\n\n");
	seq_printf(m, "%-10s %-20s %-20s %-20s %-15s %-15s %-20s %-20s\n",
		   "Queue_ID", "SRAM_Tx_Pkts", "SRAM_Tx_Bytes", "Pending_Buf",
		   "Drop_Pkts", "Drop_Bytes", "DDRQ_Pending_Pkts",
		   "DDRQ_Pending_Bytes");
	seq_printf(m, "%-10s %-20s %-20s %-20s %-15s %-15s %-20s %-20s\n",
		   "--------", "------------", "-------------", "-------------",
		   "----------", "----------", "-----------------",
		   "------------------");

	for (j = 0; j < num_queues; j++) {
		sram_queue_drop_pkts = 0;
		sram_queue_drop_bytes = 0;
		qid = queue_base + j;

		memset(&queue_stats_info, 0, sizeof(queue_stats_info));
		if (fal_queue_counter_get(EDMA_SWITCH_DEV_ID, qid, &queue_stats_info) != SW_OK) {
			seq_printf(m, "Error: fal_queue_counter_get failed for qid %u\n", qid);
			continue;
		}

		edma_debugfs_sram_drop_sum(&queue_stats_info, &sram_queue_drop_pkts,
					   &sram_queue_drop_bytes);

		seq_printf(m, "%-10u %-20u %-20llu %-20u %-15u %-15llu %-20u %-20llu\n",
			   qid, queue_stats_info.tx_packets, queue_stats_info.tx_bytes,
			   queue_stats_info.pending_buff_num, sram_queue_drop_pkts,
			   sram_queue_drop_bytes, queue_stats_info.ddrq_packets,
			   queue_stats_info.ddrq_bytes);
	}

	seq_printf(m, "\n\n");

#if defined(NSS_DP_DDRQ_SUPPORT)
	seq_printf(m, "DDRQ stats:\n");
	seq_printf(m, "\n\n");
	seq_printf(m, "%-10s %-15s %-15s %-20s %-20s %-20s\n",
		   "Queue_ID", "Drop_Pkts", "Drop_Bytes", "Enq_Bytes",
		   "Pending_Pkts", "ISRAMQ_Enq_Pkts");
	seq_printf(m, "%-10s %-15s %-15s %-20s %-20s %-20s\n",
		   "--------", "----------", "----------", "-----------------",
		   "-----------------", "-----------------");

	for (j = 0; j < num_queues; j++) {
		qid = queue_base + j;

		memset(&ddrq_obj, 0, sizeof(ddrq_obj));
		ddrq_obj.cfg_type = NSS_DP_DDRQ_CFG_TYPE_QUEUE;
		ddrq_obj.cfg_id = qid;

		memset(&ddrq_drop_stats, 0, sizeof(ddrq_drop_stats));
		if (edma_ddrq_drop_stats_get(&ddrq_obj, &ddrq_drop_stats) != DDRQ_RET_SUCCESS) {
			seq_printf(m, "Error: edma_ddrq_drop_stats_get failed for qid %u\n", qid);
			continue;
		}

		ddrq_drop_pkts = ddrq_drop_stats.ac_drop_cnt_tbl_grn_pro_drop_pkt +
			ddrq_drop_stats.ac_drop_cnt_tbl_grn_tail_drop_pkt +
			ddrq_drop_stats.ac_drop_cnt_tbl_yel_pro_drop_pkt +
			ddrq_drop_stats.ac_drop_cnt_tbl_yel_tail_drop_pkt +
			ddrq_drop_stats.ac_drop_cnt_tbl_red_pro_drop_pkt +
			ddrq_drop_stats.ac_drop_cnt_tbl_red_tail_drop_pkt;

		ddrq_drop_bytes = ddrq_drop_stats.ac_drop_cnt_tbl_grn_pro_drop_byte +
			ddrq_drop_stats.ac_drop_cnt_tbl_grn_tail_drop_byte +
			ddrq_drop_stats.ac_drop_cnt_tbl_yel_pro_drop_byte +
			ddrq_drop_stats.ac_drop_cnt_tbl_yel_tail_drop_byte +
			ddrq_drop_stats.ac_drop_cnt_tbl_red_pro_drop_byte +
			ddrq_drop_stats.ac_drop_cnt_tbl_red_tail_drop_byte;

		if (qid < NSS_DP_DDRQ_MAX_CNT) {
			ddrq_drop_pkts -= ddrq_drop_pkts_baseline_gbl[qid];
			ddrq_drop_bytes -= ddrq_drop_bytes_baseline_gbl[qid];
		}

		memset(&ddrq_pending_stats, 0, sizeof(ddrq_pending_stats));
		if (edma_ddrq_pending_stats_get(&ddrq_obj, &ddrq_pending_stats, 1) != DDRQ_RET_SUCCESS) {
			seq_printf(m, "Error: edma_ddrq_pending_stats_get failed for qid %u\n", qid);
			continue;
		}

		seq_printf(m, "%-10u %-15u %-15llu %-20llu %-20u %-20u\n",
			   qid, ddrq_drop_pkts, ddrq_drop_bytes,
			   ddrq_pending_stats.ddrq_enq_stats,
			   ddrq_pending_stats.ddrq_pending_stats,
			   ddrq_pending_stats.isramq_enq_stats);
	}

	seq_printf(m, "\n\n");

	if (show_accu_stats) {
		if (accu_stats_queues > EDMA_DDRQ_ESRAMQ_CNT_PER_PORT)
			accu_stats_queues = EDMA_DDRQ_ESRAMQ_CNT_PER_PORT;

		seq_printf(m, "\n*************** SRAM & DDRQ ACCUMULATE STATS ***************\n\n");
		seq_printf(m, "%-15s %-20s %-20s %-20s %-25s %-25s\n",
			   "Context_ID", "SRAM_Queue_ID", "SRAM_Tx_Pkts",
			   "SRAM_Tx_Bytes", "DDRQ_Accu_Tx_Pkts",
			   "DDRQ_Accu_Tx_Bytes");
		seq_printf(m, "%-15s %-20s %-20s %-20s %-25s %-25s\n",
			   "----------", "-------------", "------------",
			   "-------------", "-----------------",
			   "------------------");

		for (j = 0; j < accu_stats_queues; j++) {
			qid = queue_base + j;

			memset(&queue_stats_info, 0, sizeof(queue_stats_info));
			if (fal_queue_counter_get(EDMA_SWITCH_DEV_ID, qid, &queue_stats_info) != SW_OK) {
				seq_printf(m, "Error: fal_queue_counter_get failed for qid %u\n", qid);
				continue;
			}

			memset(&ddrq_obj, 0, sizeof(ddrq_obj));
			ddrq_obj.cfg_type = NSS_DP_DDRQ_CFG_TYPE_QUEUE;
			ddrq_obj.cfg_id = qid;

			/*
			 * DDRQ accumulate stats are started once (on write to
			 * port_queue_stats) — only GET here, since START also
			 * resets the HW accumulate counters to zero.
			 */
			memset(&stats_info, 0, sizeof(stats_info));
			stats_info.stats_id = j;
			stats_info.stats_count = 1;
			stats_info.oper_type = NSS_DP_DDRQ_SHRD_STATS_OPER_TYPE_GET;

			memset(&ddrq_stats, 0, sizeof(ddrq_stats));
			if (edma_ddrq_accu_stats_get(&ddrq_obj, &stats_info, &ddrq_stats) != DDRQ_RET_SUCCESS) {
				seq_printf(m, "Error: edma_ddrq_accu_stats_get failed for qid %u (stats_id %u)\n",
					   qid, stats_info.stats_id);
				continue;
			}

			seq_printf(m, "%-15u %-20u %-20u %-20llu %-25u %-25llu\n",
				   stats_info.stats_id, qid, queue_stats_info.tx_packets,
				   queue_stats_info.tx_bytes, ddrq_stats.ddrq_accu_tx_pkts,
				   ddrq_stats.ddrq_accu_tx_bytes);
		}

		if (accu_stats_queues < num_queues) {
			seq_printf(m, "\nNote: Accumulate stats are limited to first %u queues per port.\n",
				   accu_stats_queues);
		}

		seq_printf(m, "\n*************** SRAM & DDRQ ACCUMULATE STATS ***************\n\n");
	}
#endif

	seq_printf(m, "*************** PER QUEUE STATS OF PORT %s ***************\n\n", port_name);
}

/*
 * edma_debugfs_port_stats_write()
 *	Write callback — stores the port bitmap supplied by the user.
 *
 *	Usage:   echo <bitmap> > /sys/kernel/debug/qca-nss-dp/queue_stats/port_queue_stats
 *	Example: echo 0x5 > /sys/kernel/debug/qca-nss-dp/queue_stats/port_queue_stats  (selects ports at bit 0 and bit 2)
 *	BIT0 and BIT2 indicates Port1 and Port2 respectively.
 *
 *	After writing, run: cat port_queue_stats  to see the output.
 */
static ssize_t edma_debugfs_port_stats_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char buffer[32];
#if defined(NSS_DP_DDRQ_SUPPORT)
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	struct net_device *netdev;
	uint32_t num_queues = EDMA_DDRQ_ESRAMQ_CNT_PER_PORT;
	int idx, i, j;
	uint32_t n = (unsigned int)(sizeof(egc->netdev_arr) / sizeof(egc->netdev_arr[0]));
	int32_t hw_port, queue_base;
	a_uint32_t qid;
	nss_dp_ddrq_obj_id_t ddrq_obj;
	nss_dp_ddrq_accu_stats_info_t stats_info;
#endif

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	buffer[count] = '\0';

	if (kstrtou16(strstrip(buffer), 0, &port_stats_bitmap_gbl))
		return -EINVAL;

	if (edma_debugfs_enable_queue_counters("queue_stats"))
		return -EIO;

#if defined(NSS_DP_DDRQ_SUPPORT)
	/*
	 * Start (reset) the DDRQ accumulate stats HW slots exactly once, here on
	 * write, rather than on every cat/show. Re-issuing START on every show
	 * would clear the HW accumulate counters right before reading them,
	 * making DDrq_Accu_Tx_Pkts/Bytes always read back as 0.
	 */
	if (port_stats_bitmap_gbl && hweight32(port_stats_bitmap_gbl) == 1) {
		for (idx = 0; idx < EDMA_MAX_PORTS; idx++) {
			if (!(port_stats_bitmap_gbl & BIT(idx)))
				continue;

			for (i = 0; i < n; i++) {
				netdev = egc->netdev_arr[i];
				if (!netdev)
					continue;

				if (strchr(netdev->name, '%') != NULL)
					continue;

				if (egc->netdev_arr[idx] != netdev)
					continue;

#if defined(NSS_DP_PON_SUPPORT)
				if (edma_debugfs_is_pon_netdev(netdev))
					continue;
#endif

				hw_port = ppe_drv_port_num_from_dev(netdev);
				if (hw_port < 0)
					continue;

				queue_base = ppe_drv_port_ucast_queue_get_by_port(hw_port);
				if (queue_base < 0)
					continue;

				for (j = 0; j < num_queues; j++) {
					qid = queue_base + j;

					memset(&ddrq_obj, 0, sizeof(ddrq_obj));
					ddrq_obj.cfg_type = NSS_DP_DDRQ_CFG_TYPE_QUEUE;
					ddrq_obj.cfg_id = qid;

					memset(&stats_info, 0, sizeof(stats_info));
					stats_info.stats_id = j;
					stats_info.stats_count = 1;
					stats_info.oper_type = NSS_DP_DDRQ_SHRD_STATS_OPER_TYPE_START;

					if (edma_ddrq_accu_stats_get(&ddrq_obj, &stats_info, NULL) != DDRQ_RET_SUCCESS) {
						edma_err("queue_stats: failed to start DDRQ accumulate stats for qid %u (stats_id %u)\n", qid, j);
					}
				}
			}
		}

#if defined(NSS_DP_PON_SUPPORT)
		if (edma_debugfs_is_pon_selected(egc, port_stats_bitmap_gbl)) {
			fal_portscheduler_resource_t pon_sched;

			memset(&pon_sched, 0, sizeof(pon_sched));
			if (fal_port_scheduler_resource_get(EDMA_SWITCH_DEV_ID, PON_PORT_ID, &pon_sched) != SW_OK) {
				edma_err("queue_stats: failed to get scheduler resource for PON port %d\n", PON_PORT_ID);
			} else {
				uint32_t pon_num_queues;

				pon_num_queues = pon_sched.ucastq_num;

				if (pon_num_queues > EDMA_DDRQ_ESRAMQ_CNT_PER_PORT)
					pon_num_queues = EDMA_DDRQ_ESRAMQ_CNT_PER_PORT;

				for (j = 0; j < pon_num_queues; j++) {
					qid = pon_sched.ucastq_start + j;

					memset(&ddrq_obj, 0, sizeof(ddrq_obj));
					ddrq_obj.cfg_type = NSS_DP_DDRQ_CFG_TYPE_QUEUE;
					ddrq_obj.cfg_id = qid;

					memset(&stats_info, 0, sizeof(stats_info));
					stats_info.stats_id = j;
					stats_info.stats_count = 1;
					stats_info.oper_type = NSS_DP_DDRQ_SHRD_STATS_OPER_TYPE_START;

					if (edma_ddrq_accu_stats_get(&ddrq_obj, &stats_info, NULL) != DDRQ_RET_SUCCESS) {
						edma_err("queue_stats: failed to start DDRQ accumulate stats for PON qid %u (stats_id %u)\n",
							 qid, j);
					}
				}
			}
		}
#endif
	}
#endif

	return count;
}

/*
 * edma_debugfs_port_stats_show()
 *	Show callback — prints per-queue SRAM/DDRQ stats for the ports selected
 *	via edma_debugfs_port_stats_write().
 *	Called when user does: cat port_queue_stats
 */
static int edma_debugfs_port_stats_show(struct seq_file *m, void __attribute__((unused))*p)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	struct net_device *netdev;
	uint32_t num_queues = EDMA_DDRQ_ESRAMQ_CNT_PER_PORT;
	bool show_accu_stats;
	int idx;
	int32_t hw_port, queue_base;
#if defined(NSS_DP_PON_SUPPORT)
	fal_portscheduler_resource_t pon_sched;
#endif

	if (!port_stats_bitmap_gbl) {
		seq_printf(m, "No port selected.\n Usage: echo <bitmap> > /sys/kernel/debug/qca-nss-dp/queue_stats/port_queue_stats\n");
		return 0;
	}

	show_accu_stats = (hweight32(port_stats_bitmap_gbl) == 1);

	seq_printf(m, "\n\n*********************** Port stats for bitmap: 0x%x (%u) ***********************\n\n", port_stats_bitmap_gbl, port_stats_bitmap_gbl);

	for (idx = 0; idx < EDMA_MAX_PORTS; idx++) {
		if (!(port_stats_bitmap_gbl & BIT(idx)))
			continue;

		netdev = egc->netdev_arr[idx];
		if (!netdev)
			continue;

		if (strchr(netdev->name, '%') != NULL)
			continue;

#if defined(NSS_DP_PON_SUPPORT)
		if (edma_debugfs_is_pon_netdev(netdev))
			continue;
#endif

		hw_port = ppe_drv_port_num_from_dev(netdev);
		if (hw_port < 0) {
			seq_printf(m, "[%s] error : Unable to get hardware port \n", netdev->name);
			continue;
		}

		queue_base = ppe_drv_port_ucast_queue_get_by_port(hw_port);
		if (queue_base < 0) {
			seq_printf(m, "[%s] Error: Unable to get queue base for hardware port %d\n", netdev->name, hw_port);
			continue;
		}

		edma_debugfs_port_queue_stats_show(m, netdev->name, hw_port, queue_base,
						   num_queues, show_accu_stats);
	}

#if defined(NSS_DP_PON_SUPPORT)
	if (edma_debugfs_is_pon_selected(egc, port_stats_bitmap_gbl)) {
		memset(&pon_sched, 0, sizeof(pon_sched));
		if (fal_port_scheduler_resource_get(EDMA_SWITCH_DEV_ID, PON_PORT_ID, &pon_sched) != SW_OK) {
			seq_printf(m, "Error: Unable to get scheduler resource for PON port %d\n", PON_PORT_ID);
		} else {
			edma_debugfs_port_queue_stats_show(m, "PON", PON_PORT_ID,
							   pon_sched.ucastq_start,
							   pon_sched.ucastq_num,
							   show_accu_stats);
		}
	}
#endif

	return 0;
}

/*
 * edma_debugfs_port_stats_open()
 *	open() callback for port_queue_stats debugfs file.
 */
static int edma_debugfs_port_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_port_stats_show, inode->i_private);
}

/*
 * edma_debugfs_port_stats_file_ops
 *	File operations for port_queue_stats debugfs file.
 */
static const struct file_operations edma_debugfs_port_stats_file_ops = {
        .open = edma_debugfs_port_stats_open,
        .read = seq_read,
        .llseek = seq_lseek,
		.write = edma_debugfs_port_stats_write,
        .release = single_release,
};

/*
 * edma_debugfs_queue_stats_flush()
 *	Write callback — flushes queue stats for the ports selected via port_stats.
 *
 *	Usage: echo "flush" > /sys/kernel/debug/qca-nss-dp/queue_stats/flush_stats
 */
static ssize_t edma_debugfs_queue_stats_flush(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	struct net_device *netdev;
	uint32_t num_queues = EDMA_DDRQ_ESRAMQ_CNT_PER_PORT;
	int idx, i, j;
	uint32_t n = (unsigned int)(sizeof(egc->netdev_arr) / sizeof(egc->netdev_arr[0]));
	int32_t hw_port, queue_base;
	a_uint32_t qid;
	char buffer[16];
#if defined(NSS_DP_DDRQ_SUPPORT)
	nss_dp_ddrq_obj_id_t ddrq_obj;
	nss_dp_ddrq_ac_queue_drop_cnt_tbl_t ddrq_drop_stats;
	nss_dp_ddrq_accu_stats_info_t stats_info;
#endif

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	buffer[count] = '\0';

	if (strcmp(strstrip(buffer), "flush")) {
		edma_err("queue_stats: invalid input. Usage: echo \"flush\" > /sys/kernel/debug/qca-nss-dp/queue_stats/flush_stats\n");
		return -EINVAL;
	}

	if (!port_stats_bitmap_gbl) {
		edma_err("queue_stats: No port selected. Usage: echo <bitmap> > /sys/kernel/debug/qca-nss-dp/queue_stats/port_stats\n");
		return -EINVAL;
	}

	for (idx = 0; idx < EDMA_MAX_PORTS; idx++) {
		if (!(port_stats_bitmap_gbl & BIT(idx)))
			continue;

		for (i = 0; i < n; i++) {
			netdev = egc->netdev_arr[i];
			if (!netdev)
				continue;
			if (strchr(netdev->name, '%') != NULL)
				continue;
			if (egc->netdev_arr[idx] != netdev)
				continue;

#if defined(NSS_DP_PON_SUPPORT)
			if (edma_debugfs_is_pon_netdev(netdev))
				continue;
#endif

			hw_port = ppe_drv_port_num_from_dev(netdev);
			if (hw_port < 0) {
				edma_err("queue_stats: [%s] error : Unable to get hardware port\n", netdev->name);
				continue;
			}

			queue_base = ppe_drv_port_ucast_queue_get_by_port(hw_port);
			if (queue_base < 0) {
				edma_err("queue_stats: [%s] Error: Unable to get queue base for hardware port %d\n", netdev->name, hw_port);
				continue;
			}

			for (j = 0 ; j < num_queues; j++){
				qid = queue_base + j;

				if (fal_queue_counter_cleanup(EDMA_SWITCH_DEV_ID, qid) != SW_OK) {
					edma_err("queue_stats: Warning: cleanup failed for qid %u\n", qid);
					continue;
				}

#if defined(NSS_DP_DDRQ_SUPPORT)
					if (qid >= NSS_DP_DDRQ_MAX_CNT) {
						continue;
					}

					memset(&ddrq_obj, 0, sizeof(ddrq_obj));
					ddrq_obj.cfg_type = NSS_DP_DDRQ_CFG_TYPE_QUEUE;
					ddrq_obj.cfg_id = qid;

					/*
					 * START operation rebinds/restarts the corresponding accumulate
					 * context and resets DDRQ accumulate packet/byte counters.
					 */
					memset(&stats_info, 0, sizeof(stats_info));
					stats_info.stats_id = j;
					stats_info.stats_count = 1;
					stats_info.oper_type = NSS_DP_DDRQ_SHRD_STATS_OPER_TYPE_START;
					if (edma_ddrq_accu_stats_get(&ddrq_obj, &stats_info, NULL) != DDRQ_RET_SUCCESS) {
						edma_err("queue_stats: Warning: ddrq accumulate stats reset failed for qid %u (stats_id %u)\n",
							 qid, stats_info.stats_id);
					}

					memset(&ddrq_drop_stats, 0, sizeof(ddrq_drop_stats));
					if (edma_ddrq_drop_stats_get(&ddrq_obj, &ddrq_drop_stats) != DDRQ_RET_SUCCESS) {
						edma_err("queue_stats: Warning: ddrq drop stats get failed for qid %u\n", qid);
						continue;
					}

					ddrq_drop_pkts_baseline_gbl[qid] = ddrq_drop_stats.ac_drop_cnt_tbl_grn_pro_drop_pkt +
						ddrq_drop_stats.ac_drop_cnt_tbl_grn_tail_drop_pkt +
						ddrq_drop_stats.ac_drop_cnt_tbl_yel_pro_drop_pkt +
						ddrq_drop_stats.ac_drop_cnt_tbl_yel_tail_drop_pkt +
						ddrq_drop_stats.ac_drop_cnt_tbl_red_pro_drop_pkt +
						ddrq_drop_stats.ac_drop_cnt_tbl_red_tail_drop_pkt;

					ddrq_drop_bytes_baseline_gbl[qid] = ddrq_drop_stats.ac_drop_cnt_tbl_grn_pro_drop_byte +
						ddrq_drop_stats.ac_drop_cnt_tbl_grn_tail_drop_byte +
						ddrq_drop_stats.ac_drop_cnt_tbl_yel_pro_drop_byte +
						ddrq_drop_stats.ac_drop_cnt_tbl_yel_tail_drop_byte +
						ddrq_drop_stats.ac_drop_cnt_tbl_red_pro_drop_byte +
						ddrq_drop_stats.ac_drop_cnt_tbl_red_tail_drop_byte;
#endif
			}

			edma_info("queue_stats: flushed queue stats for [%s]\n", netdev->name);
		}
	}

#if defined(NSS_DP_PON_SUPPORT)
	if (edma_debugfs_is_pon_selected(egc, port_stats_bitmap_gbl)) {
		fal_portscheduler_resource_t pon_sched;
		a_uint32_t pon_qid;

		memset(&pon_sched, 0, sizeof(pon_sched));
		if(fal_port_scheduler_resource_get(EDMA_SWITCH_DEV_ID, PON_PORT_ID, &pon_sched) != SW_OK){
			edma_err("queue_stats: Unable to get scheduler resource for PON port %d\n", PON_PORT_ID);
		}
		else{
			for(j = 0; j < pon_sched.ucastq_num; j++){
				pon_qid = pon_sched.ucastq_start + j;

				if (fal_queue_counter_cleanup(EDMA_SWITCH_DEV_ID, pon_qid) != SW_OK) {
					edma_err("queue_stats: Warning: cleanup failed for PON qid %u\n", pon_qid);
					continue;
				}
			}

			edma_info("queue_stats: flushed queue stats for PON port\n");
		}
	}
#endif
	return count;
}

/*
 * edma_debugfs_flush_file_ops
 *	File operations for flush_stats debugfs file.
 */
static const struct file_operations edma_debugfs_flush_file_ops = {
	.write   = edma_debugfs_queue_stats_flush,
	.llseek  = default_llseek,
};

#if defined(NSS_DP_DDRQ_SUPPORT)
/*
 * EDMA_DEBUGFS_DDRQ_ACCU_STATS_ID_CNT
 *	Number of HW accumulate-stats context slots (0-7).
 */
#define EDMA_DEBUGFS_DDRQ_ACCU_STATS_ID_CNT	8

/*
 * ddrq_accu_stats_qid_gbl
 *	Queue ids (0-159) selected by the user via echo, one per HW accumulate
 *	stats context slot. Index into this array is the context id (0-7);
 *	ddrq_accu_stats_qid_gbl[context_id] holds the queue id bound to that
 *	context. SRAM and DDRQ accumulate stats for every bound queue are
 *	printed on cat. NSS_DP_DDRQ_INV_VAL means that context slot is unset [-1].
 */
static int32_t ddrq_accu_stats_qid_gbl[EDMA_DEBUGFS_DDRQ_ACCU_STATS_ID_CNT] = {
	[0 ... EDMA_DEBUGFS_DDRQ_ACCU_STATS_ID_CNT - 1] = NSS_DP_DDRQ_INV_VAL
};

/*
 * edma_debugfs_ddrq_accu_stats_write()
 *	Write callback — binds up to EDMA_DEBUGFS_DDRQ_ACCU_STATS_ID_CNT queue
 *	ids supplied by the user to the DDRQ accumulate stats HW context slots
 *	(context id = position in the supplied list) and starts accumulation
 *	for each.
 *
 *	Usage:   echo "<qid_0> <qid_1> ... <qid_7>" > /sys/kernel/debug/qca-nss-dp/queue_stats/ddrq_accu_stats
 *	Example: echo "10 18 26 34 42 50 58 66" > /sys/kernel/debug/qca-nss-dp/queue_stats/ddrq_accu_stats
 *
 *	After writing, run: cat ddrq_accu_stats  to see the SRAM and DDRQ accumulate stats.
 */
static ssize_t edma_debugfs_ddrq_accu_stats_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char buffer[128];
	char *cursor, *tok;
	uint32_t qid;
	int32_t new_qids[EDMA_DEBUGFS_DDRQ_ACCU_STATS_ID_CNT];
	int i, j, n;
	nss_dp_ddrq_obj_id_t ddrq_obj;
	nss_dp_ddrq_accu_stats_info_t stats_info;

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	buffer[count] = '\0';
	cursor = strstrip(buffer);

	n = 0;
	while ((tok = strsep(&cursor, " ")) != NULL){
		if (*tok == '\0')
			continue;

		if (n >= EDMA_DEBUGFS_DDRQ_ACCU_STATS_ID_CNT){
			edma_err("ddrq_accu_stats: too many queue ids, at most %d supported\n", EDMA_DEBUGFS_DDRQ_ACCU_STATS_ID_CNT);
			return -EINVAL;
		}

		if (kstrtou32(tok, 0, &qid)){
			edma_err("ddrq_accu_stats: invalid queue id '%s'\n", tok);
			return -EINVAL;
		}

		if (qid >= NSS_DP_DDRQ_MAX_CNT){
			edma_err("ddrq_accu_stats: invalid queue id %u, valid range 0-%d\n", qid, NSS_DP_DDRQ_MAX_CNT - 1);
			return -EINVAL;
		}

		for (j = 0; j < n; j++){
			if(new_qids[j] == qid){
				edma_err("ddrq_accu_stats: duplicate queue id %u\n", qid);
				return -EINVAL;
			}
		}

		new_qids[n++] = qid;
	}

	if (n == 0){
		edma_err("ddrq_accu_stats: no queue id supplied. Usage: echo \"<qid_0> ... <qid_7>\" > ddrq_accu_stats\n");
		return -EINVAL;
	}

	if (edma_debugfs_enable_queue_counters("ddrq_accu_stats"))
		return -EIO;

	for (i = 0; i < n; i++){
		qid = new_qids[i];

		if (fal_queue_counter_cleanup(EDMA_SWITCH_DEV_ID, qid) != SW_OK) {
			edma_err("ddrq_accu_stats: failed to reset SRAM queue stats for qid %u\n", qid);
			return -EIO;
		}

		memset(&ddrq_obj, 0, sizeof(ddrq_obj));
		ddrq_obj.cfg_type = NSS_DP_DDRQ_CFG_TYPE_QUEUE;
		ddrq_obj.cfg_id = qid;

		memset(&stats_info, 0, sizeof(stats_info));
		stats_info.stats_id = i;
		stats_info.stats_count = 1;
		stats_info.oper_type = NSS_DP_DDRQ_SHRD_STATS_OPER_TYPE_START;

		if (edma_ddrq_accu_stats_get(&ddrq_obj, &stats_info, NULL) != DDRQ_RET_SUCCESS){
			edma_err("ddrq_accu_stats: failed to start DDRQ accumulate stats for qid %u (stats_id %u)\n", qid, stats_info.stats_id);
			return -EIO;
		}
	}

	for (i = 0; i < n; i++)
		ddrq_accu_stats_qid_gbl[i] = new_qids[i];
	for (; i < EDMA_DEBUGFS_DDRQ_ACCU_STATS_ID_CNT; i++)
		ddrq_accu_stats_qid_gbl[i] = NSS_DP_DDRQ_INV_VAL;

	return count;
}

/*
 * edma_debugfs_ddrq_accu_stats_show()
 *	Show callback — prints SRAM Tx pkts/bytes and DDRQ accumulate Tx
 *	pkts/bytes for every queue id selected via edma_debugfs_ddrq_accu_stats_write(),
 *	one row per bound context id. Called when user does: cat ddrq_accu_stats
 */
static int edma_debugfs_ddrq_accu_stats_show(struct seq_file *m, void __attribute__((unused)) *p)
{
	nss_dp_ddrq_obj_id_t ddrq_obj;
	nss_dp_ddrq_accu_stats_info_t stats_info;
	nss_dp_ddrq_accu_stats_t ddrq_stats;
	fal_queue_stats_t queue_stats_info;
	uint32_t qid;
	int i;

	if(ddrq_accu_stats_qid_gbl[0] == NSS_DP_DDRQ_INV_VAL){
		seq_printf(m, "No queue selected.\n Usage: echo \"<qid_0> ... <qid_%d> (0-%d each)\" > /sys/kernel/debug/qca-nss-dp/queue_stats/ddrq_accu_stats\n",
			   EDMA_DEBUGFS_DDRQ_ACCU_STATS_ID_CNT - 1, NSS_DP_DDRQ_MAX_CNT - 1);
		return 0;
	}

	seq_printf(m, "\n*************** SRAM & DDRQ ACCUMULATE STATS ***************\n\n");

	seq_printf(m, "%-15s %-20s %-20s %-20s %-25s %-25s\n", "Context_ID", "SRAM_Queue_ID", "SRAM_Tx_Pkts", "SRAM_Tx_Bytes", "DDRQ_Accu_Tx_Pkts", "DDRQ_Accu_Tx_Bytes");
	seq_printf(m, "%-15s %-20s %-20s %-20s %-25s %-25s\n", "----------", "-------------", "------------", "-------------", "-----------------", "------------------");

	/*
	 * TODO : Make the queue stats read from locally maintained 64-bit counters (
	 * collected in the software based WQ).
	 */
	for(i = 0; i < EDMA_DEBUGFS_DDRQ_ACCU_STATS_ID_CNT; i++){
		if(ddrq_accu_stats_qid_gbl[i] == NSS_DP_DDRQ_INV_VAL)
			continue;

		qid = ddrq_accu_stats_qid_gbl[i];

		memset(&queue_stats_info, 0, sizeof(queue_stats_info));
		if (fal_queue_counter_get(EDMA_SWITCH_DEV_ID, qid, &queue_stats_info) != SW_OK){
			seq_printf(m, "Error: fal_queue_counter_get failed for qid %u\n", qid);
			continue;
		}

		memset(&ddrq_obj, 0, sizeof(ddrq_obj));
		ddrq_obj.cfg_type = NSS_DP_DDRQ_CFG_TYPE_QUEUE;
		ddrq_obj.cfg_id = qid;

		memset(&stats_info, 0, sizeof(stats_info));
		stats_info.stats_id = i;
		stats_info.stats_count = 1;
		stats_info.oper_type = NSS_DP_DDRQ_SHRD_STATS_OPER_TYPE_GET;

		memset(&ddrq_stats, 0, sizeof(ddrq_stats));
		if(edma_ddrq_accu_stats_get(&ddrq_obj, &stats_info, &ddrq_stats) != DDRQ_RET_SUCCESS){
			seq_printf(m, "Error: edma_ddrq_accu_stats_get failed for qid %u (stats_id %u)\n", qid, stats_info.stats_id);
			continue;
		}

		seq_printf(m, "%-15u %-20u %-20u %-20llu %-25u %-25llu\n", stats_info.stats_id, qid, queue_stats_info.tx_packets, queue_stats_info.tx_bytes, ddrq_stats.ddrq_accu_tx_pkts, ddrq_stats.ddrq_accu_tx_bytes);
	}

	seq_printf(m, "\n*************** SRAM & DDRQ ACCUMULATE STATS ***************\n\n");
	return 0;
}

/*
 * edma_debugfs_ddrq_accu_stats_open()
 *	open() callback for ddrq_accu_stats debugfs file.
 */
static int edma_debugfs_ddrq_accu_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_ddrq_accu_stats_show, inode->i_private);
}

/*
 * edma_debugfs_ddrq_accu_stats_file_ops
 *	File operations for ddrq_accu_stats debugfs file.
 */
static const struct file_operations edma_debugfs_ddrq_accu_stats_file_ops = {
	.open    = edma_debugfs_ddrq_accu_stats_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.write   = edma_debugfs_ddrq_accu_stats_write,
	.release = single_release,
};

/*
 * edma_debugfs_ddrq_occ_stats_show()
 *	Show callback — prints DDRQ per-queue occupancy statistics.
 *
 *	Usage: echo "<qid_0> ... <qid_7>" > ddrq_occ_stats to select up to 8 queues,
 *	       then cat to read stats for all selected queues.
 *	       echo "start"  > ddrq_occ_stats  to start the occupancy counter.
 *	       echo "stop"   > ddrq_occ_stats  to stop the occupancy counter.
 *	       echo "reset"  > ddrq_occ_stats  to reset occupancy counters for all
 *	       currently selected queues.
 */

/*
 * EDMA_DEBUGFS_DDRQ_OCC_STATS_QID_CNT
 *	Maximum number of queue ids accepted for DDRQ occupancy stats.
 */
#define EDMA_DEBUGFS_DDRQ_OCC_STATS_QID_CNT	8

/*
 * ddrq_occ_stats_qid_gbl
 *	Queue ids selected by the user for DDRQ occupancy stats.
 *	NSS_DP_DDRQ_INV_VAL means that entry is unset.
 */
static int32_t ddrq_occ_stats_qid_gbl[EDMA_DEBUGFS_DDRQ_OCC_STATS_QID_CNT] = {
	[0 ... EDMA_DEBUGFS_DDRQ_OCC_STATS_QID_CNT - 1] = NSS_DP_DDRQ_INV_VAL
};

/*
 * edma_debugfs_ddrq_occ_stats_show()
 *	Show DDRQ occupancy statistics for all selected queues.
 */
static int edma_debugfs_ddrq_occ_stats_show(struct seq_file *m, void __attribute__((unused)) *p)
{
	nss_dp_ddrq_occupancy_stats_t occ_stats;
	bool status;
	uint32_t qid;
	int i;

	if (ddrq_occ_stats_qid_gbl[0] == NSS_DP_DDRQ_INV_VAL) {
		seq_printf(m, "No queue selected.\n"
			   " Usage: echo \"<qid_0> ... <qid_%d> (0-%d each)\" > /sys/kernel/debug/qca-nss-dp/ddrq_info/ddrq_occ_stats\n",
			   EDMA_DEBUGFS_DDRQ_OCC_STATS_QID_CNT - 1,
			   NSS_DP_DDRQ_MAX_CNT - 1);
		return 0;
	}

	seq_printf(m, "\n*************** DDRQ OCCUPANCY STATS ***************\n\n");
	seq_printf(m, "%-10s %-20s %-20s %-20s %-20s %-20s\n",
		   "Queue_ID", "Threshold_Exceeded", "Current_Pkts",
		   "Current_Bytes", "Peak_Pkts", "Peak_Bytes");
	seq_printf(m, "%-10s %-20s %-20s %-20s %-20s %-20s\n",
		   "--------", "------------------", "------------",
		   "-------------", "---------", "----------");

	for (i = 0; i < EDMA_DEBUGFS_DDRQ_OCC_STATS_QID_CNT; i++) {
		if (ddrq_occ_stats_qid_gbl[i] == NSS_DP_DDRQ_INV_VAL)
			continue;

		qid = (uint32_t)ddrq_occ_stats_qid_gbl[i];

		memset(&occ_stats, 0, sizeof(occ_stats));
		if (edma_ddrq_occupancy_stats_get(qid, &occ_stats) != DDRQ_RET_SUCCESS) {
			seq_printf(m, "Error: failed to get DDRQ occupancy stats for qid %u\n", qid);
			continue;
		}

		if (edma_ddrq_occupancy_stats_status_get(qid, &status) != DDRQ_RET_SUCCESS) {
			seq_printf(m, "Error: failed to get DDRQ occupancy status for qid %u\n", qid);
			continue;
		}

		seq_printf(m, "%-10u %-20s %-20u %-20llu %-20u %-20llu\n",
			   qid, status ? "yes" : "no", occ_stats.pkts, occ_stats.bytes,
			   occ_stats.peak_pkts, occ_stats.peak_bytes);
	}

	seq_printf(m, "\n*************** DDRQ OCCUPANCY STATS ***************\n\n");

	return 0;
}

/*
 * edma_debugfs_ddrq_occ_stats_write()
 *	Write callback for ddrq_occ_stats.
 *
 *	Accepts:
 *	  "<qid_0> ... <qid_7>"  — select up to 8 queue ids
 *	  "start"  — start the occupancy counter
 *	  "stop"   — stop the occupancy counter
 *	  "reset"  — reset occupancy counters for all selected queue ids
 */
static ssize_t edma_debugfs_ddrq_occ_stats_write(struct file *file, const char __user *buf,
						  size_t count, loff_t *ppos)
{
	char buffer[128];
	char *cmd;
	char *cursor, *tok;
	int32_t new_qids[EDMA_DEBUGFS_DDRQ_OCC_STATS_QID_CNT];
	uint32_t qid;
	bool reset_failed;
	int i, j, n;

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	buffer[count] = 0;
	cmd = strstrip(buffer);

	if (!strcmp(cmd, "start")) {
		if (edma_ddrq_occupancy_stats_start() != DDRQ_RET_SUCCESS) {
			edma_err("ddrq_occ_stats: failed to start occupancy counter\n");
			return -EIO;
		}
		return count;
	}

	if (!strcmp(cmd, "stop")) {
		if (edma_ddrq_occupancy_stats_stop() != DDRQ_RET_SUCCESS) {
			edma_err("ddrq_occ_stats: failed to stop occupancy counter\n");
			return -EIO;
		}
		return count;
	}

	if (!strcmp(cmd, "reset")) {
		if (ddrq_occ_stats_qid_gbl[0] == NSS_DP_DDRQ_INV_VAL) {
			edma_err("ddrq_occ_stats: select queue id before reset\n");
			return -EINVAL;
		}

		reset_failed = false;
		for (i = 0; i < EDMA_DEBUGFS_DDRQ_OCC_STATS_QID_CNT; i++) {
			if (ddrq_occ_stats_qid_gbl[i] == NSS_DP_DDRQ_INV_VAL)
				continue;

			qid = (uint32_t)ddrq_occ_stats_qid_gbl[i];
			if (edma_ddrq_occupancy_stats_reset(qid) != DDRQ_RET_SUCCESS) {
				edma_err("ddrq_occ_stats: failed to reset occupancy counter for qid %u\n", qid);
				reset_failed = true;
			}
		}

		return reset_failed ? -EIO : count;
	}

	cursor = cmd;
	n = 0;
	while ((tok = strsep(&cursor, " ")) != NULL) {
		if (*tok == '\0')
			continue;

		if (n >= EDMA_DEBUGFS_DDRQ_OCC_STATS_QID_CNT) {
			edma_err("ddrq_occ_stats: too many queue ids, at most %d supported\n",
				 EDMA_DEBUGFS_DDRQ_OCC_STATS_QID_CNT);
			return -EINVAL;
		}

		if (kstrtou32(tok, 0, &qid)) {
			edma_err("ddrq_occ_stats: invalid queue id '%s'\n", tok);
			return -EINVAL;
		}

		if (qid >= NSS_DP_DDRQ_MAX_CNT) {
			edma_err("ddrq_occ_stats: invalid queue id %u, valid range 0-%d\n",
				 qid, NSS_DP_DDRQ_MAX_CNT - 1);
			return -EINVAL;
		}

		for (j = 0; j < n; j++) {
			if (new_qids[j] == qid) {
				edma_err("ddrq_occ_stats: duplicate queue id %u\n", qid);
				return -EINVAL;
			}
		}

		new_qids[n++] = (int32_t)qid;
	}

	if (n == 0) {
		edma_err("ddrq_occ_stats: invalid input %s. Provide queue ids (0-%d)\n",
			 cmd, NSS_DP_DDRQ_MAX_CNT - 1);
		return -EINVAL;
	}

	/*
	 * Reset the DDRQ occupancy statistics for the selected queue ids.
	 */
	for (i = 0; i < n; i++) {
		qid = (uint32_t)new_qids[i];
		if (edma_ddrq_occupancy_stats_reset(qid) != DDRQ_RET_SUCCESS) {
			edma_err("ddrq_occ_stats: failed to reset occupancy counter for qid %u\n", qid);
			return -EIO;
		}
	}

	for (i = 0; i < n; i++)
		ddrq_occ_stats_qid_gbl[i] = new_qids[i];
	for (; i < EDMA_DEBUGFS_DDRQ_OCC_STATS_QID_CNT; i++)
		ddrq_occ_stats_qid_gbl[i] = NSS_DP_DDRQ_INV_VAL;

	return count;
}

/*
 * edma_debugfs_ddrq_occ_stats_open()
 *	open() callback for ddrq_occ_stats debugfs file.
 */
static int edma_debugfs_ddrq_occ_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_ddrq_occ_stats_show, inode->i_private);
}

/*
 * edma_debugfs_ddrq_occ_stats_file_ops
 *	File operations for ddrq_occ_stats debugfs file.
 */
static const struct file_operations edma_debugfs_ddrq_occ_stats_file_ops = {
	.open    = edma_debugfs_ddrq_occ_stats_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.write   = edma_debugfs_ddrq_occ_stats_write,
	.release = single_release,
};

/*
 * ddrq_occ_threshold_qid_gbl
 *	Queue id selected by the user for DDRQ occupancy threshold get/set.
 */
static int32_t ddrq_occ_threshold_qid_gbl = NSS_DP_DDRQ_INV_VAL;

/*
 * edma_debugfs_ddrq_occ_threshold_show()
 *	Show DDRQ occupancy threshold for the selected queue.
 */
static int edma_debugfs_ddrq_occ_threshold_show(struct seq_file *m, void __attribute__((unused)) *p)
{
	nss_dp_ddrq_occupancy_threshold_t threshold;
	uint32_t qid;

	if (ddrq_occ_threshold_qid_gbl == NSS_DP_DDRQ_INV_VAL) {
		seq_printf(m, "No queue selected.\n"
			   " Usage: echo \"<qid> <threshold_val> <threshold_type(1=byte,0=pkt)>\""
			   " > /sys/kernel/debug/qca-nss-dp/ddrq_info/ddrq_occ_threshold\n");
		return 0;
	}

	qid = (uint32_t)ddrq_occ_threshold_qid_gbl;

	memset(&threshold, 0, sizeof(threshold));
	if (edma_ddrq_occupancy_stats_threshold_get(qid, &threshold) != DDRQ_RET_SUCCESS) {
		seq_printf(m, "Error: failed to get DDRQ occupancy threshold for qid %u\n", qid);
		return 0;
	}

	seq_printf(m, "\n*************** DDRQ OCCUPANCY THRESHOLD (Queue %u) ***************\n\n", qid);
	seq_printf(m, "%-25s : %lld\n", "Threshold_Value", threshold.threshold_val);
	seq_printf(m, "%-25s : %s\n", "Threshold_Type",
		   threshold.threshold_type ? "byte" : "pkt");
	seq_printf(m, "\n*************** DDRQ OCCUPANCY THRESHOLD (Queue %u) ***************\n\n", qid);

	return 0;
}

/*
 * edma_debugfs_ddrq_occ_threshold_write()
 *	Write callback for ddrq_occ_threshold.
 *
 *	Usage: echo "<qid> <threshold_val> <threshold_type(1=byte,0=pkt)>"
 *	             > /sys/kernel/debug/qca-nss-dp/ddrq_info/ddrq_occ_threshold
 */
static ssize_t edma_debugfs_ddrq_occ_threshold_write(struct file *file, const char __user *buf,
						      size_t count, loff_t *ppos)
{
	char buffer[64];
	uint32_t qid, threshold_type;
	int64_t threshold_val;
	nss_dp_ddrq_occupancy_threshold_t threshold;
	int ret;

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	buffer[count] = 0;

	ret = sscanf(buffer, "%u %lld %u", &qid, &threshold_val, &threshold_type);
	if (ret != 3) {
		edma_err("ddrq_occ_threshold: invalid input. Usage: echo \"<qid> <threshold_val> <1=byte|0=pkt>\"\n");
		return -EINVAL;
	}

	if (qid >= NSS_DP_DDRQ_MAX_CNT) {
		edma_err("ddrq_occ_threshold: invalid queue id %u, valid range 0-%d\n",
			 qid, NSS_DP_DDRQ_MAX_CNT - 1);
		return -EINVAL;
	}

	if (threshold_type > 1) {
		edma_err("ddrq_occ_threshold: invalid threshold_type %u, must be 1 (byte) or 0 (pkt)\n",
			 threshold_type);
		return -EINVAL;
	}

	if (threshold_val < 0) {
		edma_err("ddrq_occ_threshold: invalid threshold_val %lld, must be >= 0\n",
			 threshold_val);
		return -EINVAL;
	}

	threshold.threshold_val = threshold_val;
	threshold.threshold_type = (int8_t)threshold_type;

	if (edma_ddrq_occupancy_stats_threshold_set(qid, &threshold) != DDRQ_RET_SUCCESS) {
		edma_err("ddrq_occ_threshold: failed to set threshold for qid %u\n", qid);
		return -EIO;
	}

	ddrq_occ_threshold_qid_gbl = (int32_t)qid;

	return count;
}

/*
 * edma_debugfs_ddrq_occ_threshold_open()
 *	open() callback for ddrq_occ_threshold debugfs file.
 */
static int edma_debugfs_ddrq_occ_threshold_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_ddrq_occ_threshold_show, inode->i_private);
}

/*
 * edma_debugfs_ddrq_occ_threshold_file_ops
 *	File operations for ddrq_occ_threshold debugfs file.
 */
static const struct file_operations edma_debugfs_ddrq_occ_threshold_file_ops = {
	.open    = edma_debugfs_ddrq_occ_threshold_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.write   = edma_debugfs_ddrq_occ_threshold_write,
	.release = single_release,
};

/*
 * SRAMQ counter-monitor debugfs nodes.
 *
 * The SSDK exposes the following SRAMQ occupancy counter-monitor APIs:
 *   fal_qm_counter_monitor_en_set/get      — global enable/disable + cleanup
 *   fal_qm_counter_monitor_map_set/get     — bind a queue to a counter slot
 *   fal_qm_counter_monitor_ctrl_set/get    — per-counter threshold mode/value
 *   fal_qm_counter_monitor_stats_get       — per-counter occupancy statistics
 *   fal_qm_counter_monitor_status_get      — per-counter high-watermark status
 *
 * Unicast: up to EDMA_SRAMQ_UCAST_CNT_MAX (64) counter slots, each mapped to
 *          one queue id via fal_qm_counter_monitor_map_set.
 * Multicast: one counter per PPE port (up to EDMA_SRAMQ_MCAST_PORT_MAX = 9).
 */

/*
 * edma_debugfs_sramq_global_ctrl_show()
 *	Show SRAMQ global counter-monitor enable state.
 */
static int edma_debugfs_sramq_global_ctrl_show(struct seq_file *m, void __attribute__((unused)) *p)
{
	a_bool_t enable = A_FALSE;

	if (fal_qm_counter_monitor_en_get(EDMA_SWITCH_DEV_ID, &enable) != SW_OK) {
		seq_printf(m, "Error: fal_qm_counter_monitor_en_get failed\n");
		return 0;
	}

	seq_printf(m, "\n*************** SRAMQ GLOBAL COUNTER MONITOR CTRL ***************\n\n");
	seq_printf(m, "%-25s : %s\n", "Counter_Monitor_Enable", enable ? "enabled" : "disabled");
	seq_printf(m, "\n Usage: echo \"1\" > sramq_global_ctrl  (enable)\n");
	seq_printf(m, "        echo \"0\" > sramq_global_ctrl  (disable)\n");
	seq_printf(m, "        echo \"cleanup\" > sramq_global_ctrl  (reset all counters)\n");
	seq_printf(m, "\n*************** SRAMQ GLOBAL COUNTER MONITOR CTRL ***************\n\n");

	return 0;
}

/*
 * edma_debugfs_sramq_global_ctrl_write()
 *	Write callback for sramq_global_ctrl.
 *
 *	Usage:
 *	  echo "1"       > sramq_global_ctrl   — enable SRAMQ counter monitor
 *	  echo "0"       > sramq_global_ctrl   — disable SRAMQ counter monitor
 *	  echo "cleanup" > sramq_global_ctrl   — reset all SRAMQ counters
 */
static ssize_t edma_debugfs_sramq_global_ctrl_write(struct file *file, const char __user *buf,
						     size_t count, loff_t *ppos)
{
	char buffer[32];
	char *cmd;
	uint32_t enable_val;

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	buffer[count] = 0;
	cmd = strstrip(buffer);

	if (!strcmp(cmd, "cleanup")) {
		if (fal_qm_counter_monitor_stats_cleanup(EDMA_SWITCH_DEV_ID) != SW_OK) {
			edma_err("sramq_global_ctrl: fal_qm_counter_monitor_stats_cleanup failed\n");
			return -EIO;
		}
		return count;
	}

	if (kstrtou32(cmd, 0, &enable_val) || enable_val > 1) {
		edma_err("sramq_global_ctrl: invalid input %s. Use 0, 1, or cleanup\n", cmd);
		return -EINVAL;
	}

	if (fal_qm_counter_monitor_en_set(EDMA_SWITCH_DEV_ID,
					  enable_val ? A_TRUE : A_FALSE) != SW_OK) {
		edma_err("sramq_global_ctrl: fal_qm_counter_monitor_en_set failed\n");
		return -EIO;
	}

	return count;
}

/*
 * edma_debugfs_sramq_global_ctrl_open()
 *	open() callback for sramq_global_ctrl debugfs file.
 */
static int edma_debugfs_sramq_global_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_sramq_global_ctrl_show, inode->i_private);
}

/*
 * edma_debugfs_sramq_global_ctrl_file_ops
 *	File operations for sramq_global_ctrl debugfs file.
 */
static const struct file_operations edma_debugfs_sramq_global_ctrl_file_ops = {
	.open    = edma_debugfs_sramq_global_ctrl_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.write   = edma_debugfs_sramq_global_ctrl_write,
	.release = single_release,
};

/*
 * edma_debugfs_sramq_map_show()
 *	Show SRAMQ counter-monitor queue-to-counter mapping for all unicast
 *	counter slots and all multicast port slots.
 */
static int edma_debugfs_sramq_map_show(struct seq_file *m, void __attribute__((unused)) *p)
{
	fal_qm_monitor_map_t monitor_map;
	uint32_t i;

	seq_printf(m, "\n*************** SRAMQ COUNTER MONITOR MAP ***************\n\n");

	seq_printf(m, "UNICAST (queue_id -> cnt_id):\n");
	seq_printf(m, "%-15s %-15s %-10s\n", "Queue_ID", "Cnt_ID", "Enabled");
	seq_printf(m, "%-15s %-15s %-10s\n", "--------", "------", "-------");

	for (i = 0; i < EDMA_SRAMQ_UCAST_CNT_MAX; i++) {
		memset(&monitor_map, 0, sizeof(monitor_map));
		if (fal_qm_counter_monitor_map_get(EDMA_SWITCH_DEV_ID, FAL_SRAM_UNIQ,
						   i, &monitor_map) != SW_OK) {
			seq_printf(m, "%-15u Error: map_get failed\n", i);
			continue;
		}
		if (!monitor_map.cnt_en)
			continue;
		seq_printf(m, "%-15u %-15u %-10s\n", i, monitor_map.cnt_id,
			   monitor_map.cnt_en ? "yes" : "no");
	}

	seq_printf(m, "\nMULTICAST (port_id -> queue_offset):\n");
	seq_printf(m, "%-15s %-15s %-10s\n", "Port_ID", "Queue_Offset", "Enabled");
	seq_printf(m, "%-15s %-15s %-10s\n", "-------", "------------", "-------");

	for (i = 0; i < EDMA_SRAMQ_MCAST_PORT_MAX; i++) {
		memset(&monitor_map, 0, sizeof(monitor_map));
		if (fal_qm_counter_monitor_map_get(EDMA_SWITCH_DEV_ID, FAL_SRAM_MULTIQ,
						   i, &monitor_map) != SW_OK) {
			seq_printf(m, "%-15u Error: map_get failed\n", i);
			continue;
		}
		seq_printf(m, "%-15u %-15u %-10s\n", i, monitor_map.cnt_id,
			   monitor_map.cnt_en ? "yes" : "no");
	}

	seq_printf(m, "\n Usage: echo \"<ucast|mcast> <map_id> <cnt_id> <0|1>\""
		   " > /sys/kernel/debug/qca-nss-dp/sramq_info/sramq_map\n");
	seq_printf(m, "        ucast: map_id=queue_id (0-%d), cnt_id=counter_slot (0-%d)\n",
		   NSS_DP_DDRQ_MAX_CNT - 1, EDMA_SRAMQ_UCAST_CNT_MAX - 1);
	seq_printf(m, "        mcast: map_id=port_id (0-%d), cnt_id=queue_offset\n",
		   EDMA_SRAMQ_MCAST_PORT_MAX - 1);
	seq_printf(m, "\n*************** SRAMQ COUNTER MONITOR MAP ***************\n\n");

	return 0;
}

/*
 * edma_debugfs_sramq_map_write()
 *	Write callback for sramq_map.
 *
 *	Usage: echo "<ucast|mcast> <map_id> <cnt_id> <0|1>"
 *	             > /sys/kernel/debug/qca-nss-dp/sramq_info/sramq_map
 *
 *	For unicast: map_id = queue_id, cnt_id = counter slot (0-63)
 *	For multicast: map_id = port_id, cnt_id = queue offset
 */
static ssize_t edma_debugfs_sramq_map_write(struct file *file, const char __user *buf,
					    size_t count, loff_t *ppos)
{
	char buffer[64];
	char type_str[8];
	uint32_t map_id, cnt_id, cnt_en;
	fal_qm_queue_type_t queue_type;
	fal_qm_monitor_map_t monitor_map;
	int ret;

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	buffer[count] = 0;

	ret = sscanf(buffer, "%7s %u %u %u", type_str, &map_id, &cnt_id, &cnt_en);
	if (ret != 4) {
		edma_err("sramq_map: invalid input. Usage: echo \"<ucast|mcast> <map_id> <cnt_id> <0|1>\"\n");
		return -EINVAL;
	}

	if (!strcmp(type_str, "ucast")) {
		queue_type = FAL_SRAM_UNIQ;
		if (cnt_id >= EDMA_SRAMQ_UCAST_CNT_MAX) {
			edma_err("sramq_map: invalid cnt_id %u for ucast, valid range 0-%d\n",
				 cnt_id, EDMA_SRAMQ_UCAST_CNT_MAX - 1);
			return -EINVAL;
		}
	} else if (!strcmp(type_str, "mcast")) {
		queue_type = FAL_SRAM_MULTIQ;
		if (map_id >= EDMA_SRAMQ_MCAST_PORT_MAX) {
			edma_err("sramq_map: invalid port_id %u for mcast, valid range 0-%d\n",
				 map_id, EDMA_SRAMQ_MCAST_PORT_MAX - 1);
			return -EINVAL;
		}
	} else {
		edma_err("sramq_map: invalid type %s, must be ucast or mcast\n", type_str);
		return -EINVAL;
	}

	if (cnt_en > 1) {
		edma_err("sramq_map: invalid enable value %u, must be 0 or 1\n", cnt_en);
		return -EINVAL;
	}

	monitor_map.cnt_en = cnt_en ? A_TRUE : A_FALSE;
	monitor_map.cnt_id = cnt_id;

	if (fal_qm_counter_monitor_map_set(EDMA_SWITCH_DEV_ID, queue_type,
					   map_id, &monitor_map) != SW_OK) {
		edma_err("sramq_map: fal_qm_counter_monitor_map_set failed for %s map_id %u cnt_id %u\n",
			 type_str, map_id, cnt_id);
		return -EIO;
	}

	return count;
}

/*
 * edma_debugfs_sramq_map_open()
 *	open() callback for sramq_map debugfs file.
 */
static int edma_debugfs_sramq_map_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_sramq_map_show, inode->i_private);
}

/*
 * edma_debugfs_sramq_map_file_ops
 *	File operations for sramq_map debugfs file.
 */
static const struct file_operations edma_debugfs_sramq_map_file_ops = {
	.open    = edma_debugfs_sramq_map_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.write   = edma_debugfs_sramq_map_write,
	.release = single_release,
};

/*
 * edma_debugfs_sramq_ctrl_show()
 *	Show SRAMQ per-counter threshold mode and value for all unicast and
 *	multicast counter slots.
 */
static int edma_debugfs_sramq_ctrl_show(struct seq_file *m, void __attribute__((unused)) *p)
{
	fal_qm_monitor_ctrl_t monitor_ctrl;
	uint32_t i;

	seq_printf(m, "\n*************** SRAMQ COUNTER MONITOR CTRL ***************\n\n");

	seq_printf(m, "UNICAST counters (cnt_id 0-%d):\n", EDMA_SRAMQ_UCAST_CNT_MAX - 1);
	seq_printf(m, "%-10s %-15s %-15s\n", "Cnt_ID", "Threshold_Mode", "Threshold_Val");
	seq_printf(m, "%-10s %-15s %-15s\n", "------", "--------------", "-------------");

	for (i = 0; i < EDMA_SRAMQ_UCAST_CNT_MAX; i++) {
		memset(&monitor_ctrl, 0, sizeof(monitor_ctrl));
		if (fal_qm_counter_monitor_ctrl_get(EDMA_SWITCH_DEV_ID, FAL_SRAM_UNIQ,
						    i, &monitor_ctrl) != SW_OK) {
			seq_printf(m, "%-10u Error: ctrl_get failed\n", i);
			continue;
		}
		seq_printf(m, "%-10u %-15s %-15u\n", i,
			   monitor_ctrl.cnt_threshold_mode ? "pkt" : "byte",
			   monitor_ctrl.cnt_threshold);
	}

	seq_printf(m, "\nMULTICAST counters (port_id 0-%d):\n", EDMA_SRAMQ_MCAST_PORT_MAX - 1);
	seq_printf(m, "%-10s %-15s %-15s\n", "Port_ID", "Threshold_Mode", "Threshold_Val");
	seq_printf(m, "%-10s %-15s %-15s\n", "-------", "--------------", "-------------");

	for (i = 0; i < EDMA_SRAMQ_MCAST_PORT_MAX; i++) {
		memset(&monitor_ctrl, 0, sizeof(monitor_ctrl));
		if (fal_qm_counter_monitor_ctrl_get(EDMA_SWITCH_DEV_ID, FAL_SRAM_MULTIQ,
						    i, &monitor_ctrl) != SW_OK) {
			seq_printf(m, "%-10u Error: ctrl_get failed\n", i);
			continue;
		}
		seq_printf(m, "%-10u %-15s %-15u\n", i,
			   monitor_ctrl.cnt_threshold_mode ? "pkt" : "byte",
			   monitor_ctrl.cnt_threshold);
	}

	seq_printf(m, "\n Usage: echo \"<ucast|mcast> <cnt_id> <0=byte|1=pkt> <threshold_val>\""
		   " > /sys/kernel/debug/qca-nss-dp/sramq_info/sramq_ctrl\n");
	seq_printf(m, "\n*************** SRAMQ COUNTER MONITOR CTRL ***************\n\n");

	return 0;
}

/*
 * edma_debugfs_sramq_ctrl_write()
 *	Write callback for sramq_ctrl.
 *
 *	Usage: echo "<ucast|mcast> <cnt_id> <0=byte|1=pkt> <threshold_val>"
 *	             > /sys/kernel/debug/qca-nss-dp/sramq_info/sramq_ctrl
 */
static ssize_t edma_debugfs_sramq_ctrl_write(struct file *file, const char __user *buf,
					     size_t count, loff_t *ppos)
{
	char buffer[64];
	char type_str[8];
	uint32_t cnt_id, threshold_mode, threshold_val;
	fal_qm_queue_type_t queue_type;
	fal_qm_monitor_ctrl_t monitor_ctrl;
	int ret;

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	buffer[count] = 0;

	ret = sscanf(buffer, "%7s %u %u %u", type_str, &cnt_id, &threshold_mode, &threshold_val);
	if (ret != 4) {
		edma_err("sramq_ctrl: invalid input. Usage: echo \"<ucast|mcast> <cnt_id> <0=byte|1=pkt> <threshold_val>\"\n");
		return -EINVAL;
	}

	if (!strcmp(type_str, "ucast")) {
		queue_type = FAL_SRAM_UNIQ;
		if (cnt_id >= EDMA_SRAMQ_UCAST_CNT_MAX) {
			edma_err("sramq_ctrl: invalid cnt_id %u for ucast, valid range 0-%d\n",
				 cnt_id, EDMA_SRAMQ_UCAST_CNT_MAX - 1);
			return -EINVAL;
		}
	} else if (!strcmp(type_str, "mcast")) {
		queue_type = FAL_SRAM_MULTIQ;
		if (cnt_id >= EDMA_SRAMQ_MCAST_PORT_MAX) {
			edma_err("sramq_ctrl: invalid port_id %u for mcast, valid range 0-%d\n",
				 cnt_id, EDMA_SRAMQ_MCAST_PORT_MAX - 1);
			return -EINVAL;
		}
	} else {
		edma_err("sramq_ctrl: invalid type %s, must be ucast or mcast\n", type_str);
		return -EINVAL;
	}

	if (threshold_mode > 1) {
		edma_err("sramq_ctrl: invalid threshold_mode %u, must be 0 (byte) or 1 (pkt)\n",
			 threshold_mode);
		return -EINVAL;
	}

	monitor_ctrl.cnt_threshold_mode = threshold_mode ? A_TRUE : A_FALSE;
	monitor_ctrl.cnt_threshold = threshold_val;

	if (fal_qm_counter_monitor_ctrl_set(EDMA_SWITCH_DEV_ID, queue_type,
					    cnt_id, &monitor_ctrl) != SW_OK) {
		edma_err("sramq_ctrl: fal_qm_counter_monitor_ctrl_set failed for %s cnt_id %u\n",
			 type_str, cnt_id);
		return -EIO;
	}

	return count;
}

/*
 * edma_debugfs_sramq_ctrl_open()
 *	open() callback for sramq_ctrl debugfs file.
 */
static int edma_debugfs_sramq_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_sramq_ctrl_show, inode->i_private);
}

/*
 * edma_debugfs_sramq_ctrl_file_ops
 *	File operations for sramq_ctrl debugfs file.
 */
static const struct file_operations edma_debugfs_sramq_ctrl_file_ops = {
	.open    = edma_debugfs_sramq_ctrl_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.write   = edma_debugfs_sramq_ctrl_write,
	.release = single_release,
};

/*
 * edma_debugfs_sramq_stats_show()
 *	Show SRAMQ per-counter occupancy statistics for all unicast and
 *	multicast counter slots.
 *
 *	Prints: current packets, current bytes, peak packets, peak bytes.
 */
static int edma_debugfs_sramq_stats_show(struct seq_file *m, void __attribute__((unused)) *p)
{
	fal_qm_monitor_stats_t monitor_stats;
	uint32_t i;

	seq_printf(m, "\n*************** SRAMQ COUNTER MONITOR STATS ***************\n\n");

	seq_printf(m, "UNICAST counters (cnt_id 0-%d):\n", EDMA_SRAMQ_UCAST_CNT_MAX - 1);
	seq_printf(m, "%-10s %-15s %-15s %-15s %-15s\n",
		   "Cnt_ID", "Curr_Pkts", "Curr_Bytes", "Peak_Pkts", "Peak_Bytes");
	seq_printf(m, "%-10s %-15s %-15s %-15s %-15s\n",
		   "------", "---------", "----------", "---------", "----------");

	for (i = 0; i < EDMA_SRAMQ_UCAST_CNT_MAX; i++) {
		memset(&monitor_stats, 0, sizeof(monitor_stats));
		if (fal_qm_counter_monitor_stats_get(EDMA_SWITCH_DEV_ID, FAL_SRAM_UNIQ,
						     i, &monitor_stats) != SW_OK) {
			seq_printf(m, "%-10u Error: stats_get failed\n", i);
			continue;
		}
		seq_printf(m, "%-10u %-15u %-15u %-15u %-15u\n", i,
			   monitor_stats.packets, monitor_stats.bytes,
			   monitor_stats.peak_packets, monitor_stats.peak_bytes);
	}

	seq_printf(m, "\nMULTICAST counters (port_id 0-%d):\n", EDMA_SRAMQ_MCAST_PORT_MAX - 1);
	seq_printf(m, "%-10s %-15s %-15s %-15s %-15s\n",
		   "Port_ID", "Curr_Pkts", "Curr_Bytes", "Peak_Pkts", "Peak_Bytes");
	seq_printf(m, "%-10s %-15s %-15s %-15s %-15s\n",
		   "-------", "---------", "----------", "---------", "----------");

	for (i = 0; i < EDMA_SRAMQ_MCAST_PORT_MAX; i++) {
		memset(&monitor_stats, 0, sizeof(monitor_stats));
		if (fal_qm_counter_monitor_stats_get(EDMA_SWITCH_DEV_ID, FAL_SRAM_MULTIQ,
						     i, &monitor_stats) != SW_OK) {
			seq_printf(m, "%-10u Error: stats_get failed\n", i);
			continue;
		}
		seq_printf(m, "%-10u %-15u %-15u %-15u %-15u\n", i,
			   monitor_stats.packets, monitor_stats.bytes,
			   monitor_stats.peak_packets, monitor_stats.peak_bytes);
	}

	seq_printf(m, "\n*************** SRAMQ COUNTER MONITOR STATS ***************\n\n");

	return 0;
}

/*
 * edma_debugfs_sramq_stats_open()
 *	open() callback for sramq_stats debugfs file.
 */
static int edma_debugfs_sramq_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_sramq_stats_show, inode->i_private);
}

/*
 * edma_debugfs_sramq_stats_file_ops
 *	File operations for sramq_stats debugfs file.
 */
static const struct file_operations edma_debugfs_sramq_stats_file_ops = {
	.open    = edma_debugfs_sramq_stats_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

/*
 * edma_debugfs_sramq_status_show()
 *	Show SRAMQ per-counter high-watermark (threshold-exceeded) status for
 *	all unicast and multicast counter slots.
 */
static int edma_debugfs_sramq_status_show(struct seq_file *m, void __attribute__((unused)) *p)
{
	a_bool_t status;
	uint32_t i;

	seq_printf(m, "\n*************** SRAMQ COUNTER MONITOR STATUS ***************\n\n");

	seq_printf(m, "UNICAST counters (cnt_id 0-%d):\n", EDMA_SRAMQ_UCAST_CNT_MAX - 1);
	seq_printf(m, "%-10s %-20s\n", "Cnt_ID", "Threshold_Exceeded");
	seq_printf(m, "%-10s %-20s\n", "------", "------------------");

	for (i = 0; i < EDMA_SRAMQ_UCAST_CNT_MAX; i++) {
		status = A_FALSE;
		if (fal_qm_counter_monitor_status_get(EDMA_SWITCH_DEV_ID, FAL_SRAM_UNIQ,
						      i, &status) != SW_OK) {
			seq_printf(m, "%-10u Error: status_get failed\n", i);
			continue;
		}
		seq_printf(m, "%-10u %-20s\n", i, status ? "yes" : "no");
	}

	seq_printf(m, "\nMULTICAST counters (port_id 0-%d):\n", EDMA_SRAMQ_MCAST_PORT_MAX - 1);
	seq_printf(m, "%-10s %-20s\n", "Port_ID", "Threshold_Exceeded");
	seq_printf(m, "%-10s %-20s\n", "-------", "------------------");

	for (i = 0; i < EDMA_SRAMQ_MCAST_PORT_MAX; i++) {
		status = A_FALSE;
		if (fal_qm_counter_monitor_status_get(EDMA_SWITCH_DEV_ID, FAL_SRAM_MULTIQ,
						      i, &status) != SW_OK) {
			seq_printf(m, "%-10u Error: status_get failed\n", i);
			continue;
		}
		seq_printf(m, "%-10u %-20s\n", i, status ? "yes" : "no");
	}

	seq_printf(m, "\n*************** SRAMQ COUNTER MONITOR STATUS ***************\n\n");

	return 0;
}

/*
 * edma_debugfs_sramq_status_open()
 *	open() callback for sramq_status debugfs file.
 */
static int edma_debugfs_sramq_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_sramq_status_show, inode->i_private);
}

/*
 * edma_debugfs_sramq_status_file_ops
 *	File operations for sramq_status debugfs file.
 */
static const struct file_operations edma_debugfs_sramq_status_file_ops = {
	.open    = edma_debugfs_sramq_status_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

#endif /* NSS_DP_DDRQ_SUPPORT */

/*
 * edma_debugfs_init()
 *	EDMA debugfs init API
 */
int edma_debugfs_init(void)
{
	edma_gbl_ctx->root_dentry = debugfs_create_dir("qca-nss-dp", NULL);
	if (!edma_gbl_ctx->root_dentry) {
		edma_err("Unable to create debugfs qca-nss-dp directory in debugfs\n");
		return -1;
	}

	edma_gbl_ctx->stats_dentry = debugfs_create_dir("stats", edma_gbl_ctx->root_dentry);
	if (!edma_gbl_ctx->stats_dentry) {
		edma_err("Unable to create debugfs stats directory in debugfs\n");
		goto debugfs_dir_failed;
	}

	edma_gbl_ctx->bp_stats_dentry = debugfs_create_dir("bp_stats", edma_gbl_ctx->stats_dentry);
	if (!edma_gbl_ctx->bp_stats_dentry) {
		edma_err("Unable to create debugfs bp stats directory in debugfs\n");
		goto debugfs_dir_failed;
	}

	edma_gbl_ctx->queue_stats_dentry = debugfs_create_dir("queue_stats", edma_gbl_ctx->root_dentry);
	if(!edma_gbl_ctx->queue_stats_dentry){
		edma_err("Unable to create debugfs queue stats directory in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("port_to_bitmap", S_IRUGO, edma_gbl_ctx->queue_stats_dentry,
			NULL, &edma_port_bitmap_info_file_ops)) {
		edma_err("Unable to create port_to_bitmap file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("port_queue_stats", S_IRUGO | S_IWUGO, edma_gbl_ctx->queue_stats_dentry,
			NULL, &edma_debugfs_port_stats_file_ops)) {
		edma_err("Unable to create port_stats file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("flush_stats", S_IWUGO, edma_gbl_ctx->queue_stats_dentry,
			NULL, &edma_debugfs_flush_file_ops)) {
		edma_err("Unable to create flush_stats file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

#if defined(NSS_DP_DDRQ_SUPPORT)
	if (!debugfs_create_file("ddrq_accu_stats", S_IRUGO | S_IWUGO, edma_gbl_ctx->queue_stats_dentry,
			NULL, &edma_debugfs_ddrq_accu_stats_file_ops)) {
		edma_err("Unable to create ddrq_accu_stats file entry in debugfs\n");
		goto debugfs_dir_failed;
	}
#endif

	if (!debugfs_create_file("bp_counter_clk_cycle", S_IRUGO | S_IWUGO, edma_gbl_ctx->bp_stats_dentry,
				  NULL, &edma_debugfs_bp_counter_clk_cycle_file_ops)) {
		edma_err("Unable to create bp counter clk cycle file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("bp_counter_clean_bitmap", S_IRUGO | S_IWUGO, edma_gbl_ctx->bp_stats_dentry,
				  NULL, &edma_debugfs_bp_counter_clean_bitmap_file_ops)) {
		edma_err("Unable to create bp counter clean bitmap file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("bp_counters_enable", S_IRUGO | S_IWUGO, edma_gbl_ctx->bp_stats_dentry,
				  NULL, &edma_debugfs_bp_counters_enable_file_ops)) {
		edma_err("Unable to create bp counters enable file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("bp_counters", S_IRUGO, edma_gbl_ctx->bp_stats_dentry,
				  NULL, &edma_debugfs_bp_counters_file_ops)) {
		edma_err("Unable to create bp counters file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("bp_all_counters_clean", S_IRUGO | S_IWUGO, edma_gbl_ctx->bp_stats_dentry,
				  NULL, &edma_debugfs_bp_all_counters_clean_file_ops)) {
		edma_err("Unable to create bp all counters clean file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("bp_cnt_total", S_IRUGO, edma_gbl_ctx->bp_stats_dentry,
				  NULL, &edma_debugfs_bp_cnt_total_file_ops)) {
		edma_err("Unable to create bp cnt total file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("rx_ring_stats", S_IRUGO, edma_gbl_ctx->stats_dentry,
			NULL, &edma_debugfs_rx_rings_file_ops)) {
		edma_err("Unable to create Rx rings statistics file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("tx_ring_stats", S_IRUGO, edma_gbl_ctx->stats_dentry,
			NULL, &edma_debugfs_tx_rings_file_ops)) {
		edma_err("Unable to create Tx rings statistics file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("clear_ring_stats", S_IRUGO, edma_gbl_ctx->stats_dentry,
			NULL, &edma_debugfs_clear_ring_stats_ops)) {
		edma_err("Unable to create clear rings statistics file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	/*
	 * Allocate memory for EDMA miscellaneous stats
	 */
	if (edma_misc_stats_alloc() < 0) {
		edma_err("Unable to allocate miscellaneous percpu stats\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("misc_stats", S_IRUGO, edma_gbl_ctx->stats_dentry,
			NULL, &edma_debugfs_misc_file_ops)) {
		edma_err("Unable to create EDMA miscellaneous statistics file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

#if defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
	if (!debugfs_create_file("loopback_stats", S_IRUGO, edma_gbl_ctx->stats_dentry,
			NULL, &edma_debugfs_loopback_file_ops)) {
		edma_err("Unable to create EDMA loopback statistics file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

#endif

#ifdef NSS_DP_MHT_SW_PORT_MAP
	if (!debugfs_create_file("mht_tx_fcgrp", S_IRUGO, edma_gbl_ctx->root_dentry,
			NULL, &edma_debugfs_mht_tx_fcgrp_file_ops)) {
		edma_err("Unable to create EDMA tx fcgrp on MHT ports file entry in debugfs\n");
		goto debugfs_dir_failed;
	}
#endif

#if defined(NSS_DP_HW_GRO)
	if (!debugfs_create_file("hw_gro_config", S_IRUGO, edma_gbl_ctx->stats_dentry,
			NULL, &edma_debugfs_hw_gro_file_ops)) {
		edma_err("Unable to create EDMA loopback statistics file entry in debugfs\n");
		goto debugfs_dir_failed;
	}
#endif

#if defined(NSS_DP_DDRQ_SUPPORT)
	edma_gbl_ctx->ddrq_info_dentry = debugfs_create_dir("ddrq_info", edma_gbl_ctx->root_dentry);
	if (!edma_gbl_ctx->ddrq_info_dentry) {
		edma_err("Unable to create EDMA DDRQ information directory in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("edma_ddrq_port_en_bitmask", S_IRUGO, edma_gbl_ctx->ddrq_info_dentry,
			NULL, &edma_debugfs_ddrq_port_en_bitmask_file_ops)) {
		edma_err("Unable to create EDMA DDRQ port enable bitmask file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("edma_ddrq_blk_num", S_IRUGO, edma_gbl_ctx->ddrq_info_dentry,
			NULL, &edma_debugfs_ddrq_blk_num_file_ops)) {
		edma_err("Unable to create EDMA DDRQ block number file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("edma_ddrq_blk_size", S_IRUGO, edma_gbl_ctx->ddrq_info_dentry,
			NULL, &edma_debugfs_ddrq_blk_size_file_ops)) {
		edma_err("Unable to create EDMA DDRQ block size file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file(EDMA_DDRQ_OCC_STATS_NODE_NAME, S_IRUGO | S_IWUGO,
			edma_gbl_ctx->ddrq_info_dentry,
			NULL, &edma_debugfs_ddrq_occ_stats_file_ops)) {
		edma_err("Unable to create EDMA DDRQ occupancy stats file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file(EDMA_DDRQ_OCC_THRESHOLD_NODE_NAME, S_IRUGO | S_IWUGO,
			edma_gbl_ctx->ddrq_info_dentry,
			NULL, &edma_debugfs_ddrq_occ_threshold_file_ops)) {
		edma_err("Unable to create EDMA DDRQ occupancy threshold file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	edma_gbl_ctx->sramq_info_dentry = debugfs_create_dir(EDMA_SRAMQ_INFO_DIR_NAME,
							     edma_gbl_ctx->root_dentry);
	if (!edma_gbl_ctx->sramq_info_dentry) {
		edma_err("Unable to create EDMA SRAMQ information directory in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file(EDMA_SRAMQ_GLOBAL_CTRL_NODE_NAME, S_IRUGO | S_IWUGO,
			edma_gbl_ctx->sramq_info_dentry,
			NULL, &edma_debugfs_sramq_global_ctrl_file_ops)) {
		edma_err("Unable to create EDMA SRAMQ global ctrl file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file(EDMA_SRAMQ_MAP_NODE_NAME, S_IRUGO | S_IWUGO,
			edma_gbl_ctx->sramq_info_dentry,
			NULL, &edma_debugfs_sramq_map_file_ops)) {
		edma_err("Unable to create EDMA SRAMQ map file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file(EDMA_SRAMQ_CTRL_NODE_NAME, S_IRUGO | S_IWUGO,
			edma_gbl_ctx->sramq_info_dentry,
			NULL, &edma_debugfs_sramq_ctrl_file_ops)) {
		edma_err("Unable to create EDMA SRAMQ ctrl file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file(EDMA_SRAMQ_STATS_NODE_NAME, S_IRUGO,
			edma_gbl_ctx->sramq_info_dentry,
			NULL, &edma_debugfs_sramq_stats_file_ops)) {
		edma_err("Unable to create EDMA SRAMQ stats file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file(EDMA_SRAMQ_STATUS_NODE_NAME, S_IRUGO,
			edma_gbl_ctx->sramq_info_dentry,
			NULL, &edma_debugfs_sramq_status_file_ops)) {
		edma_err("Unable to create EDMA SRAMQ status file entry in debugfs\n");
		goto debugfs_dir_failed;
	}
#endif
	return 0;

debugfs_dir_failed:
	debugfs_remove_recursive(edma_gbl_ctx->root_dentry);
	edma_gbl_ctx->root_dentry = NULL;
	edma_gbl_ctx->stats_dentry = NULL;
	edma_gbl_ctx->bp_stats_dentry = NULL;
	edma_gbl_ctx->queue_stats_dentry = NULL;
#if defined(NSS_DP_DDRQ_SUPPORT)
	edma_gbl_ctx->ddrq_info_dentry = NULL;
	edma_gbl_ctx->sramq_info_dentry = NULL;
#endif
	return -1;
}

/*
 * edma_debugfs_exit()
 *	EDMA debugfs exit API
 */
void edma_debugfs_exit(void)
{
	/*
	 * Free EDMA miscellaneous stats memory
	 */
	edma_misc_stats_free();

	if (edma_gbl_ctx->root_dentry) {
		debugfs_remove_recursive(edma_gbl_ctx->root_dentry);
		edma_gbl_ctx->root_dentry = NULL;
		edma_gbl_ctx->stats_dentry = NULL;
		edma_gbl_ctx->bp_stats_dentry = NULL;
		edma_gbl_ctx->queue_stats_dentry = NULL;
#if defined(NSS_DP_DDRQ_SUPPORT)
		edma_gbl_ctx->ddrq_info_dentry = NULL;
		edma_gbl_ctx->sramq_info_dentry = NULL;
#endif
	}
}
