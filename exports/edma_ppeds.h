/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __NSS_DP_PPEDS_H__
#define __NSS_DP_PPEDS_H__

/**
 * edma_ppeds_rx_fill_elem
 *	PPE-DS Rx fill buffer info
 */
struct edma_ppeds_rx_fill_elem {
	uint32_t opaque_lo;		/**< Low 32-bit opaque content */
	uint32_t opaque_hi;		/**< High 32-bit opaque content */
	dma_addr_t buff_addr;		/**< Buffer's physical address */
};

/**
 * edma_ppeds_tx_cmpl_elem
 *	PPE-DS Tx complete buffer info
 */
struct edma_ppeds_tx_cmpl_elem {
	unsigned long cookie;		/**< Cookie information to be filled in the opaque field */
};

/**
 * edma_ppeds_handle
 *	PPE-DS EDMA handle info
 */
typedef struct edma_ppeds_handle {
	dma_addr_t ppe2tcl_ba;		/**< PPE2TCL ring's base address */
	dma_addr_t reo2ppe_ba;		/**< REO2PPE ring's base address */
	uint32_t ppe2tcl_num_desc;	/**< PPE2TCL descriptor count */
	uint32_t reo2ppe_num_desc;	/**< REO2PPE descriptor count */
	uint32_t eth_rxfill_low_thr;	/**< RxFill ring's low threshold interrupt value */
	uint32_t eth_txcomp_budget;	/**< Tx complete's budget */
	struct edma_ppeds_rx_fill_elem *rx_fill_arr;	/**< RxFill buffer array */
	struct edma_ppeds_tx_cmpl_elem *tx_cmpl_arr;	/**< TxComplete buffer array */
	char priv[] __aligned(NETDEV_ALIGN);	/**< Private area */
} edma_ppeds_handle_t;

/*
 * edma_ppeds_ops
 *	EDMA PPE-DS operations info
 */
struct edma_ppeds_ops {
	void (*rx)(edma_ppeds_handle_t *, uint16_t hw_prod_idx);
				/**< PPE-DS EDMA Rx callback */
	uint32_t (*rx_fill)(edma_ppeds_handle_t *, uint32_t num_buff_req, uint32_t buff_size, uint32_t headroom);
				/**< PPE-DS EDMA Rx fill callback */
	void (*rx_release)(edma_ppeds_handle_t *, uint64_t rx_opaque);
				/**< PPE-DS EDMA Tx descriptor free callback */
	void (*tx_cmpl)(edma_ppeds_handle_t *, uint16_t cons_idx);
				/**< PPE-DS EDMA Tx complete callback */
};

/**
 * edma_ppeds_get_rx_fill_arr
 *	Wrapper to get the RxFill array from PPE-DS EDMA handle
 *
 * @datatypes
 * edma_ppeds_rx_fill_elem
 * edma_ppeds_handle_t
 *
 * @param[in] ppeds_handle Pointer to the EDMA PPE-DS handle structure
 *
 * @return
 * RxFill array pointer
 */
static inline struct edma_ppeds_rx_fill_elem *edma_ppeds_get_rx_fill_arr(edma_ppeds_handle_t *ppeds_handle)
{
	return ppeds_handle->rx_fill_arr;
}

/**
 * edma_ppeds_get_tx_cmpl_arr
 *	Wrapper to get the TxComplete array from PPE-DS EDMA handle
 *
 * @datatypes
 * edma_ppeds_tx_cmpl_elem
 * edma_ppeds_handle_t
 *
 * @param[in] ppeds_handle Pointer to the EDMA PPE-DS handle structure
 *
 * @return
 * TxComplete array pointer
 */
static inline struct edma_ppeds_tx_cmpl_elem *edma_ppeds_get_tx_cmpl_arr(edma_ppeds_handle_t *ppeds_handle)
{
	return ppeds_handle->tx_cmpl_arr;
}

/*
 * edma_ppeds_priv
 *	Wrapper to get the private area's pointer from the PPE-DS EDMA handle
 *
 * @datatypes
 * edma_ppeds_handle_t
 *
 * @param[in] ppeds_handle Pointer to the EDMA PPE-DS handle structure
 *
 * @return
 * Pointer of the PPE-DS EMDA handle's private area
 */
static inline void *edma_ppeds_priv(edma_ppeds_handle_t *ppeds_handle)
{
	return ((void *)&ppeds_handle->priv);
}

/**
 * edma_ppeds_inst_alloc
 *	PPE-DS EDMA instance allocation API
 *
 * @datatypes
 * edma_ppeds_handle_t
 * edma_ppeds_ops
 *
 * @param[in] ops PPE-DS EDMA callback operations
 * @param[in] priv_size Requested allocation size
 *
 * @return
 * PPE-DS EDMA handle pointer
 */
edma_ppeds_handle_t *edma_ppeds_inst_alloc(const struct edma_ppeds_ops *ops, size_t priv_size);

/**
 * edma_ppeds_inst_register
 *	PPE-DS EDMA instance registration API
 *
 * @datatypes
 * edma_ppeds_handle_t
 *
 * @param[in] ppeds_handle PPE-DS EDMA handle pointer
 *
 * @return
 * true on success, false on failure
 */
bool edma_ppeds_inst_register(edma_ppeds_handle_t *ppeds_handle);

/**
 * edma_ppeds_set_rx_cons_idx
 *	PPE-DS EDMA Rx ring's consumer index set API
 *
 * @datatypes
 * edma_ppeds_handle_t
 *
 * @param[in] ppeds_handle PPE-DS EDMA handle pointer
 * @param[in] rx_cons_idx consumer index to set
 */
void edma_ppeds_set_rx_cons_idx(edma_ppeds_handle_t *ppeds_handle, uint16_t rx_cons_idx);

/**
 * edma_ppeds_set_tx_prod_idx
 *	PPE-DS EDMA Tx ring's producer index set API
 *
 * @datatypes
 * edma_ppeds_handle_t
 *
 * @param[in] ppeds_handle PPE-DS EDMA handle pointer
 * @param[in] txw_prod_idx producer index to set
 */
void edma_ppeds_set_tx_prod_idx(edma_ppeds_handle_t *ppeds_handle, uint16_t txw_prod_idx);

/**
 * edma_ppeds_get_tx_cons_idx
 *	PPE-DS EDMA Tx ring's consumer index get API
 *
 * @datatypes
 * edma_ppeds_handle_t
 *
 * @param[in] ppeds_handle PPE-DS EDMA handle pointer
 *
 * @return
 * Tx ring's consumer index
 */
uint16_t edma_ppeds_get_tx_cons_idx(edma_ppeds_handle_t *ppeds_handle);

/**
 * edma_ppeds_get_rx_prod_idx
 *	PPE-DS EDMA Rx ring's producer index get API
 *
 * @datatypes
 * edma_ppeds_handle_t
 *
 * @param[in] ppeds_handle PPE-DS EDMA handle pointer
 *
 * @return
 * Rx ring's producer index
 */
uint16_t edma_ppeds_get_rx_prod_idx(edma_ppeds_handle_t *ppeds_handle);

/**
 * edma_ppeds_inst_start
 *	PPE-DS EDMA instance start API
 *
 * @datatypes
 * edma_ppeds_handle_t
 *
 * @param[in] ppeds_handle PPE-DS EMDA handle pointer
 * @param[in] intr_enable Flag to state whether EDMA ring's interrupts needs to be enabled or not
 *
 * @return
 * 0 on success, otherwise failure
 */
int edma_ppeds_inst_start(edma_ppeds_handle_t *ppeds_handle,  uint8_t intr_enable);

/**
 * edma_ppeds_inst_refill
 *	API to fill PPE-DS EDMA RxFill ring
 *
 * @datatypes
 * edma_ppeds_handle_t
 *
 * @param[in] ppeds_handle PPE-DS EDMA handle pointer
 * @param[in] count packet count to fill
 */
void edma_ppeds_inst_refill(edma_ppeds_handle_t *ppeds_handle, int count);

/**
 * edma_ppeds_inst_stop
 *	PPE-DS EDMA instance stop API
 *
 * @datatypes
 * edma_ppeds_handle_t
 *
 * @param[in] ppeds_handle PPE-DS EDMA handle pointer
 * @param[in] intr_enable Flag to state whether EDMA ring's interrupts needs to be enabled or not
 */
void edma_ppeds_inst_stop(edma_ppeds_handle_t *ppeds_handle, uint8_t intr_enable);

/**
 * edma_ppeds_inst_del
 *	PPE-DS EDMA instance delete API
 *
 * @datatypes
 * edma_ppeds_handle_t
 *
 * @param[in] ppeds_handle PPE-DS EDMA handle pointer
 */
void edma_ppeds_inst_del(edma_ppeds_handle_t *ppeds_handle);

/**
 * edma_ppeds_get_ppe_queues
 *	PPE-DS EDMA get ppe queues API
 *
 * @datatypes
 * edma_ppeds_handle_t
 *
 * @param[in] ppeds_handle PPE-DS EDMA handle pointer
 * @param[in] ppe_queue_start Starting PPE queue number
 * @param[in] num PPE queue's count
 *
 * @return
 * True, when success.
 */
bool edma_ppeds_get_ppe_queues(edma_ppeds_handle_t *ppeds_handle, uint32_t *ppe_queue_start, uint32_t *num);

/**
 * edma_ppeds_drain_rxfill_ring
 *	PPE-DS EDMA API to drain the Rxfill ring
 *
 * @datatypes
 * edma_ppeds_handle_t
 *
 * @param[in] ppeds_handle PPE-DS EDMA handle pointer
 */
void edma_ppeds_drain_rxfill_ring(edma_ppeds_handle_t *ppeds_handle);

/**
 * edma_ppeds_drain_tx_cmpl_ring
 *	PPE-DS EDMA API to drain the Tx complete ring
 *
 * @datatypes
 * edma_ppeds_handle_t
 *
 * @param[in] ppeds_handle PPE-DS EDMA handle pointer
 */
void edma_ppeds_drain_tx_cmpl_ring(edma_ppeds_handle_t *ppeds_handle);

#endif	/** __NSS_DP_PPEDS_H__*/
