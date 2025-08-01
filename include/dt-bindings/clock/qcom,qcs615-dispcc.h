/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */
/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _DT_BINDINGS_CLK_QCOM_DISP_CC_QCS615_H
#define _DT_BINDINGS_CLK_QCOM_DISP_CC_QCS615_H

/* DISP_CC clocks */
#define DISP_CC_MDSS_AHB_CLK					0
#define DISP_CC_MDSS_AHB_CLK_SRC				1
#define DISP_CC_MDSS_BYTE0_CLK					2
#define DISP_CC_MDSS_BYTE0_CLK_SRC				3
#define DISP_CC_MDSS_BYTE0_DIV_CLK_SRC				4
#define DISP_CC_MDSS_BYTE0_INTF_CLK				5
#define DISP_CC_MDSS_DP_AUX_CLK					6
#define DISP_CC_MDSS_DP_AUX_CLK_SRC				7
#define DISP_CC_MDSS_DP_CRYPTO_CLK				8
#define DISP_CC_MDSS_DP_CRYPTO_CLK_SRC				9
#define DISP_CC_MDSS_DP_LINK_CLK				10
#define DISP_CC_MDSS_DP_LINK_CLK_SRC				11
#define DISP_CC_MDSS_DP_LINK_DIV_CLK_SRC			12
#define DISP_CC_MDSS_DP_LINK_INTF_CLK				13
#define DISP_CC_MDSS_DP_PIXEL1_CLK				14
#define DISP_CC_MDSS_DP_PIXEL1_CLK_SRC				15
#define DISP_CC_MDSS_DP_PIXEL_CLK				16
#define DISP_CC_MDSS_DP_PIXEL_CLK_SRC				17
#define DISP_CC_MDSS_ESC0_CLK					18
#define DISP_CC_MDSS_ESC0_CLK_SRC				19
#define DISP_CC_MDSS_MDP_CLK					20
#define DISP_CC_MDSS_MDP_CLK_SRC				21
#define DISP_CC_MDSS_MDP_LUT_CLK				22
#define DISP_CC_MDSS_NON_GDSC_AHB_CLK				23
#define DISP_CC_MDSS_PCLK0_CLK					24
#define DISP_CC_MDSS_PCLK0_CLK_SRC				25
#define DISP_CC_MDSS_ROT_CLK					26
#define DISP_CC_MDSS_ROT_CLK_SRC				27
#define DISP_CC_MDSS_RSCC_AHB_CLK				28
#define DISP_CC_MDSS_RSCC_VSYNC_CLK				29
#define DISP_CC_MDSS_VSYNC_CLK					30
#define DISP_CC_MDSS_VSYNC_CLK_SRC				31
#define DISP_CC_PLL0						32
#define DISP_CC_XO_CLK						33

/* DISP_CC power domains */
#define MDSS_CORE_GDSC						0

/* DISP_CC resets */
#define DISP_CC_MDSS_CORE_BCR					0
#define DISP_CC_MDSS_RSCC_BCR					1

#endif
