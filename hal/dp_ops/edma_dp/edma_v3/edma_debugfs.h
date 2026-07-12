/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __EDMA_DEBUGFS_H__
#define __EDMA_DEBUGFS_H__

#define EDMA_STATS_BANNER_MAX_LEN	80
#define EDMA_RX_RING_STATS_NODE_NAME	"EDMA_RX"
#define EDMA_TX_RING_STATS_NODE_NAME	"EDMA_TX"
#define EDMA_MISC_STATS_NODE_NAME	"EDMA_MISC"
#define EDMA_RX_RING_PPEDS_STATS_NODE_NAME	"EDMA_RX_PPEDS"
#define EDMA_TX_RING_PPEDS_STATS_NODE_NAME	"EDMA_TX_PPEDS"

/* DDRQ occupancy stats debugfs node names */
#define EDMA_DDRQ_OCC_STATS_NODE_NAME		"ddrq_occ_stats"
#define EDMA_DDRQ_OCC_THRESHOLD_NODE_NAME	"ddrq_occ_threshold"

/* SRAMQ counter-monitor debugfs node names */
#define EDMA_SRAMQ_INFO_DIR_NAME		"sramq_info"
#define EDMA_SRAMQ_GLOBAL_CTRL_NODE_NAME	"sramq_global_ctrl"
#define EDMA_SRAMQ_MAP_NODE_NAME		"sramq_map"
#define EDMA_SRAMQ_CTRL_NODE_NAME		"sramq_ctrl"
#define EDMA_SRAMQ_STATS_NODE_NAME		"sramq_stats"
#define EDMA_SRAMQ_STATUS_NODE_NAME		"sramq_status"

/* SRAMQ unicast counter limits */
#define EDMA_SRAMQ_UCAST_CNT_MAX		64
#define EDMA_SRAMQ_MCAST_PORT_MAX		9

int edma_debugfs_init(void);
void edma_debugfs_exit(void);

#endif	// __EDMA_DEBUGFS_H__
