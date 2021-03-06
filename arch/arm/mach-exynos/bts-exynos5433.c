/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>

#include <plat/devs.h>

#include <mach/map.h>
#include <mach/bts.h>
#include <mach/regs-bts.h>
#include <mach/regs-pmu.h>

#define EXYNOS5430_PA_DREX0		0x10400000
#define EXYNOS5430_PA_DREX1		0x10440000

void __iomem *drex0_va_base;
void __iomem *drex1_va_base;

enum bts_index {
	BTS_IDX_DECONM0 = 0,
	BTS_IDX_DECONM1,
	BTS_IDX_DECONM2,
	BTS_IDX_DECONM3,
	BTS_IDX_DECONM4,
	BTS_IDX_DECONTV_M0,
	BTS_IDX_DECONTV_M1,
	BTS_IDX_DECONTV_M2,
	BTS_IDX_DECONTV_M3,
	BTS_IDX_FIMC_LITE_A,
	BTS_IDX_FIMC_LITE_B,
	BTS_IDX_FIMC_LITE_D,
	BTS_IDX_3AA0,
	BTS_IDX_3AA1,
	BTS_IDX_GSCL0,
	BTS_IDX_GSCL1,
	BTS_IDX_GSCL2,
	BTS_IDX_MFC0,
	BTS_IDX_MFC1,
	BTS_IDX_G3D0,
	BTS_IDX_G3D1,
	BTS_IDX_MSCL0,
	BTS_IDX_MSCL1,
	BTS_IDX_JPEG,
	BTS_IDX_FIMC_FD,
	BTS_IDX_ISPCPU,
	BTS_IDX_FIMC_ISP,
	BTS_IDX_FIMC_DRC,
	BTS_IDX_FIMC_SCLC,
	BTS_IDX_FIMC_DIS0,
	BTS_IDX_FIMC_DIS1,
	BTS_IDX_FIMC_SCLP,
	BTS_IDX_FIMC_3DNR,
	BTS_IDX_G2D,
	BTS_IDX_HEVC0,
	BTS_IDX_HEVC1,
	BTS_IDX_NUM,
};

enum bts_id {
	BTS_DECONM0 = (1 << BTS_IDX_DECONM0),
	BTS_DECONM1 = (1 << BTS_IDX_DECONM1),
	BTS_DECONM2 = (1 << BTS_IDX_DECONM2),
	BTS_DECONM3 = (1 << BTS_IDX_DECONM3),
	BTS_DECONM4 = (1 << BTS_IDX_DECONM4),
	BTS_DECONTV_M0 = (1 << BTS_IDX_DECONTV_M0),
	BTS_DECONTV_M1 = (1 << BTS_IDX_DECONTV_M1),
	BTS_DECONTV_M2 = (1 << BTS_IDX_DECONTV_M2),
	BTS_DECONTV_M3 = (1 << BTS_IDX_DECONTV_M3),
	BTS_FIMC_LITE_A = (1 << BTS_IDX_FIMC_LITE_A),
	BTS_FIMC_LITE_B = (1 << BTS_IDX_FIMC_LITE_B),
	BTS_FIMC_LITE_D = (1 << BTS_IDX_FIMC_LITE_D),
	BTS_3AA0 = (1 << BTS_IDX_3AA0),
	BTS_3AA1 = (1 << BTS_IDX_3AA1),
	BTS_GSCL0 = (1 << BTS_IDX_GSCL0),
	BTS_GSCL1 = (1 << BTS_IDX_GSCL1),
	BTS_GSCL2 = (1 << BTS_IDX_GSCL2),
	BTS_MFC0 = (1 << BTS_IDX_MFC0),
	BTS_MFC1 = (1 << BTS_IDX_MFC1),
	BTS_G3D0 = (1 << BTS_IDX_G3D0),
	BTS_G3D1 = (1 << BTS_IDX_G3D1),
};

enum exynos_bts_scenario {
	BS_DEFAULT,
	BS_MFC_UD_ENCODING_ENABLE,
	BS_MFC_UD_ENCODING_DISABLE,
	BS_MFC_UD_DECODING_ENABLE,
	BS_MFC_UD_DECODING_DISABLE,
	BS_CAM_SCEN_ENABLE,
	BS_CAM_SCEN_DISABLE,
	BS_GSCL_OTF_ENABLE,
	BS_GSCL_OTF_DISABLE,
	BS_G3D_SCENARIO_ENABLE,
	BS_G3D_SCENARIO_DISABLE,
	BS_DISABLE,
	BS_MAX,
};

#define DEFAULT_BTS_ID_COUNT (1 << BTS_IDX_MSCL0)
#define MAKE_BTS_ID(x) (DEFAULT_BTS_ID_COUNT + \
		(DEFAULT_BTS_ID_COUNT * (x)))

struct bts_table {
	struct bts_set_table *table_list;
	unsigned int table_num;
};

struct bts_info {
	unsigned int id;
	const char *name;
	unsigned int pa_base;
	void __iomem *va_base;
	struct bts_table table[BS_MAX];
	const char *pd_name;
	const char *clk_name;
	struct clk *clk;
	bool on;
	enum exynos_bts_scenario scen;
	struct list_head list;
	struct list_head scen_list;
	bool enable;
};

struct bts_set_table {
	unsigned int reg;
	unsigned int val;
};

struct bts_scen_status {
	bool cam;
	bool ud_scen;
	bool g3d_scen;
};

struct bts_scenario {
	const char *name;
	unsigned int ip;
	enum exynos_bts_scenario id;
};

struct bts_scen_status pr_state = {
	.cam = false,
	.ud_scen = false,
	.g3d_scen = false,
};

#define update_cam(a) (pr_state.cam = a)
#define update_ud_scen(a) (pr_state.ud_scen= a)
#define update_g3d_scen(a) (pr_state.g3d_scen= a)

#define BTS_DECON (BTS_DECONM0 | BTS_DECONM1 |			\
			BTS_DECONM2 | BTS_DECONM3 | BTS_DECONM4)
#define BTS_DECONTV (BTS_DECONTV_M0 | BTS_DECONTV_M1 |		\
			BTS_DECONTV_M2 | BTS_DECONTV_M3)
#define BTS_DISPLAY (BTS_DECON | BTS_DECONTV)
#define BTS_CAM (BTS_FIMC_LITE_A | BTS_FIMC_LITE_B |		\
			BTS_FIMC_LITE_D  | BTS_3AA0 | BTS_3AA1)
#define BTS_MFC (BTS_MFC0 | BTS_MFC1)
#define BTS_GSCL (BTS_GSCL0 | BTS_GSCL1 | BTS_GSCL2)
#define BTS_G3D (BTS_G3D0 | BTS_G3D1)

#define is_bts_scen_ip(a) (a & (BTS_DECON | BTS_DECONTV | BTS_MFC | BTS_G3D))

#define BTS_TABLE(num)					\
static struct bts_set_table axiqos_##num##_table[] = {	\
	{READ_QOS_CONTROL, 0x0},			\
	{WRITE_QOS_CONTROL, 0x0},			\
	{READ_CHANNEL_PRIORITY, num},			\
	{READ_TOKEN_MAX_VALUE, 0xffdf},			\
	{READ_BW_UPPER_BOUNDARY, 0x18},			\
	{READ_BW_LOWER_BOUNDARY, 0x1},			\
	{READ_INITIAL_TOKEN_VALUE, 0x8},		\
	{WRITE_CHANNEL_PRIORITY, num},			\
	{WRITE_TOKEN_MAX_VALUE, 0xffdf},		\
	{WRITE_BW_UPPER_BOUNDARY, 0x18},		\
	{WRITE_BW_LOWER_BOUNDARY, 0x1},			\
	{WRITE_INITIAL_TOKEN_VALUE, 0x8},		\
	{READ_QOS_CONTROL, 0x1},			\
	{WRITE_QOS_CONTROL, 0x1}			\
}

#define MO_3AA1				(0x2)
#define MO_DECON			(0xa)
#define MO_DECON_TV			(0xa)

BTS_TABLE(0xcccc);
BTS_TABLE(0xdddd);
BTS_TABLE(0xeeee);
BTS_TABLE(0xffff);


static struct bts_set_table axiqos_uhd_mfc_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{WRITE_QOS_CONTROL, 0x0},
	{READ_CHANNEL_PRIORITY, 0x8888},
	{READ_TOKEN_MAX_VALUE, 0xffdf},
	{READ_BW_UPPER_BOUNDARY, 0x18},
	{READ_BW_LOWER_BOUNDARY, 0x1},
	{READ_INITIAL_TOKEN_VALUE, 0x8},
	{WRITE_CHANNEL_PRIORITY, 0x9999},
	{WRITE_TOKEN_MAX_VALUE, 0xffdf},
	{WRITE_BW_UPPER_BOUNDARY, 0x18},
	{WRITE_BW_LOWER_BOUNDARY, 0x1},
	{WRITE_INITIAL_TOKEN_VALUE, 0x8},
	{READ_QOS_CONTROL, 0x1},
	{WRITE_QOS_CONTROL, 0x1},
};

static struct bts_set_table disable_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{WRITE_QOS_CONTROL, 0x0},
};

static struct bts_set_table mo_3aa1_static_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{WRITE_QOS_CONTROL, 0x0},
	{READ_CHANNEL_PRIORITY, 0x8888},
	{READ_ISSUE_CAPABILITY_UPPER_BOUNDARY, 0x7f - MO_3AA1},
	{READ_ISSUE_CAPABILITY_LOWER_BOUNDARY, MO_3AA1 - 1},
	{READ_FLEXIBLE_BLOCKING_CONTROL, 0x0},
	{READ_FLEXIBLE_BLOCKING_POLARITY, 0x3},
	{WRITE_CHANNEL_PRIORITY, 0x8888},
	{WRITE_ISSUE_CAPABILITY_UPPER_BOUNDARY, 0x7f - MO_3AA1},
	{WRITE_ISSUE_CAPABILITY_LOWER_BOUNDARY, MO_3AA1 - 1},
	{WRITE_FLEXIBLE_BLOCKING_CONTROL, 0x0},
	{WRITE_FLEXIBLE_BLOCKING_POLARITY, 0x3},
	{WRITE_QOS_MODE, 0x2},
	{READ_QOS_MODE, 0x2},
	{WRITE_QOS_CONTROL, 0x3},
	{READ_QOS_CONTROL, 0x3},
};

static struct bts_set_table mo_decon_static_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{WRITE_QOS_CONTROL, 0x0},
	{READ_CHANNEL_PRIORITY, 0xdddd},
	{READ_ISSUE_CAPABILITY_UPPER_BOUNDARY, 0x7f - MO_DECON},
	{READ_ISSUE_CAPABILITY_LOWER_BOUNDARY, MO_DECON - 1},
	{READ_FLEXIBLE_BLOCKING_CONTROL, 0x0},
	{READ_FLEXIBLE_BLOCKING_POLARITY, 0x3},
	{WRITE_CHANNEL_PRIORITY, 0xdddd},
	{WRITE_ISSUE_CAPABILITY_UPPER_BOUNDARY, 0x7f - MO_DECON},
	{WRITE_ISSUE_CAPABILITY_LOWER_BOUNDARY, MO_DECON - 1},
	{WRITE_FLEXIBLE_BLOCKING_CONTROL, 0x0},
	{WRITE_FLEXIBLE_BLOCKING_POLARITY, 0x3},
	{WRITE_QOS_MODE, 0x2},
	{READ_QOS_MODE, 0x2},
	{WRITE_QOS_CONTROL, 0x3},
	{READ_QOS_CONTROL, 0x3},
};

static struct bts_set_table mo_decon_tv_static_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{WRITE_QOS_CONTROL, 0x0},
	{READ_CHANNEL_PRIORITY, 0xdddd},
	{READ_ISSUE_CAPABILITY_UPPER_BOUNDARY, 0x7f - MO_DECON_TV},
	{READ_ISSUE_CAPABILITY_LOWER_BOUNDARY, MO_DECON_TV - 1},
	{READ_FLEXIBLE_BLOCKING_CONTROL, 0x0},
	{READ_FLEXIBLE_BLOCKING_POLARITY, 0x3},
	{WRITE_CHANNEL_PRIORITY, 0xdddd},
	{WRITE_ISSUE_CAPABILITY_UPPER_BOUNDARY, 0x7f - MO_DECON_TV},
	{WRITE_ISSUE_CAPABILITY_LOWER_BOUNDARY, MO_DECON_TV - 1},
	{WRITE_FLEXIBLE_BLOCKING_CONTROL, 0x0},
	{WRITE_FLEXIBLE_BLOCKING_POLARITY, 0x3},
	{WRITE_QOS_MODE, 0x2},
	{READ_QOS_MODE, 0x2},
	{WRITE_QOS_CONTROL, 0x3},
	{READ_QOS_CONTROL, 0x3},
};

static struct bts_set_table axiqos_g3d_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{READ_CHANNEL_PRIORITY, 0xaaaa},
	{READ_TOKEN_MAX_VALUE, 0xffdf},
	{READ_BW_UPPER_BOUNDARY, 0x18},
	{READ_BW_LOWER_BOUNDARY, 0x1},
	{READ_INITIAL_TOKEN_VALUE, 0x8},
	{READ_QOS_CONTROL, 0x1},
};

#ifdef BTS_DBGGEN
#define BTS_DBG(x...) pr_err(x)
#else
#define BTS_DBG(x...) do {} while (0)
#endif

#ifdef BTS_DBGGEN1
#define BTS_DBG1(x...) pr_err(x)
#else
#define BTS_DBG1(x...) do {} while (0)
#endif

static struct bts_info exynos5_bts[BTS_IDX_NUM] = {
	[BTS_IDX_DECONM0] = {
		.id = BTS_DECONM0,
		.name = "decon0",
		.pa_base = EXYNOS5430_PA_BTS_DECONM0,
		.pd_name = "spd-decon",
		.clk_name = "gate_bts_deconm0",
		.table[BS_DEFAULT].table_list = axiqos_0xdddd_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.table[BS_MFC_UD_ENCODING_ENABLE].table_list = mo_decon_static_table,
		.table[BS_MFC_UD_ENCODING_ENABLE].table_num = ARRAY_SIZE(mo_decon_static_table),
		.table[BS_MFC_UD_ENCODING_DISABLE].table_list = axiqos_0xdddd_table,
		.table[BS_MFC_UD_ENCODING_DISABLE].table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.table[BS_CAM_SCEN_ENABLE].table_list = mo_decon_static_table,
		.table[BS_CAM_SCEN_ENABLE].table_num = ARRAY_SIZE(mo_decon_static_table),
		.table[BS_CAM_SCEN_DISABLE].table_list = axiqos_0xdddd_table,
		.table[BS_CAM_SCEN_DISABLE].table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_DECONM1] = {
		.id = BTS_DECONM1,
		.name = "decon1",
		.pa_base = EXYNOS5430_PA_BTS_DECONM1,
		.pd_name = "spd-decon",
		.clk_name = "gate_bts_deconm1",
		.table[BS_DEFAULT].table_list = axiqos_0xdddd_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.table[BS_MFC_UD_ENCODING_ENABLE].table_list = mo_decon_static_table,
		.table[BS_MFC_UD_ENCODING_ENABLE].table_num = ARRAY_SIZE(mo_decon_static_table),
		.table[BS_MFC_UD_ENCODING_DISABLE].table_list = axiqos_0xdddd_table,
		.table[BS_MFC_UD_ENCODING_DISABLE].table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.table[BS_CAM_SCEN_ENABLE].table_list = mo_decon_static_table,
		.table[BS_CAM_SCEN_ENABLE].table_num = ARRAY_SIZE(mo_decon_static_table),
		.table[BS_CAM_SCEN_DISABLE].table_list = axiqos_0xdddd_table,
		.table[BS_CAM_SCEN_DISABLE].table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_DECONM2] = {
		.id = BTS_DECONM2,
		.name = "decon2",
		.pa_base = EXYNOS5430_PA_BTS_DECONM2,
		.pd_name = "spd-decon",
		.clk_name = "gate_bts_deconm2",
		.table[BS_DEFAULT].table_list = axiqos_0xdddd_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.table[BS_MFC_UD_ENCODING_ENABLE].table_list = mo_decon_static_table,
		.table[BS_MFC_UD_ENCODING_ENABLE].table_num = ARRAY_SIZE(mo_decon_static_table),
		.table[BS_MFC_UD_ENCODING_DISABLE].table_list = axiqos_0xdddd_table,
		.table[BS_MFC_UD_ENCODING_DISABLE].table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.table[BS_CAM_SCEN_ENABLE].table_list = mo_decon_static_table,
		.table[BS_CAM_SCEN_ENABLE].table_num = ARRAY_SIZE(mo_decon_static_table),
		.table[BS_CAM_SCEN_DISABLE].table_list = axiqos_0xdddd_table,
		.table[BS_CAM_SCEN_DISABLE].table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_DECONM3] = {
		.id = BTS_DECONM3,
		.name = "decon3",
		.pa_base = EXYNOS5430_PA_BTS_DECONM3,
		.pd_name = "spd-decon",
		.clk_name = "gate_bts_deconm3",
		.table[BS_DEFAULT].table_list = axiqos_0xdddd_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.table[BS_MFC_UD_ENCODING_ENABLE].table_list = mo_decon_static_table,
		.table[BS_MFC_UD_ENCODING_ENABLE].table_num = ARRAY_SIZE(mo_decon_static_table),
		.table[BS_MFC_UD_ENCODING_DISABLE].table_list = axiqos_0xdddd_table,
		.table[BS_MFC_UD_ENCODING_DISABLE].table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.table[BS_CAM_SCEN_ENABLE].table_list = mo_decon_static_table,
		.table[BS_CAM_SCEN_ENABLE].table_num = ARRAY_SIZE(mo_decon_static_table),
		.table[BS_CAM_SCEN_DISABLE].table_list = axiqos_0xdddd_table,
		.table[BS_CAM_SCEN_DISABLE].table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_DECONM4] = {
		.id = BTS_DECONM4,
		.name = "decon4",
		.pa_base = EXYNOS5430_PA_BTS_DECONM4,
		.pd_name = "spd-decon",
		.clk_name = "gate_bts_deconm4",
		.table[BS_DEFAULT].table_list = axiqos_0xdddd_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.table[BS_MFC_UD_ENCODING_ENABLE].table_list = mo_decon_static_table,
		.table[BS_MFC_UD_ENCODING_ENABLE].table_num = ARRAY_SIZE(mo_decon_static_table),
		.table[BS_MFC_UD_ENCODING_DISABLE].table_list = axiqos_0xdddd_table,
		.table[BS_MFC_UD_ENCODING_DISABLE].table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.table[BS_CAM_SCEN_ENABLE].table_list = mo_decon_static_table,
		.table[BS_CAM_SCEN_ENABLE].table_num = ARRAY_SIZE(mo_decon_static_table),
		.table[BS_CAM_SCEN_DISABLE].table_list = axiqos_0xdddd_table,
		.table[BS_CAM_SCEN_DISABLE].table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_DECONTV_M0] = {
		.id = BTS_DECONTV_M0,
		.name = "decontv_m0",
		.pa_base = EXYNOS5430_PA_BTS_DECONTV_M0,
		.pd_name = "spd-decon-tv",
		.clk_name = "gate_bts_decontv_m0",
		.table[BS_DEFAULT].table_list = axiqos_0xcccc_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.table[BS_MFC_UD_ENCODING_ENABLE].table_list = mo_decon_tv_static_table,
		.table[BS_MFC_UD_ENCODING_ENABLE].table_num = ARRAY_SIZE(mo_decon_tv_static_table),
		.table[BS_MFC_UD_ENCODING_DISABLE].table_list = axiqos_0xcccc_table,
		.table[BS_MFC_UD_ENCODING_DISABLE].table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.table[BS_CAM_SCEN_ENABLE].table_list = mo_decon_tv_static_table,
		.table[BS_CAM_SCEN_ENABLE].table_num = ARRAY_SIZE(mo_decon_tv_static_table),
		.table[BS_CAM_SCEN_DISABLE].table_list = axiqos_0xcccc_table,
		.table[BS_CAM_SCEN_DISABLE].table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_DECONTV_M1] = {
		.id = BTS_DECONTV_M1,
		.name = "decontv_m1",
		.pa_base = EXYNOS5430_PA_BTS_DECONTV_M1,
		.pd_name = "spd-decon-tv",
		.clk_name = "gate_bts_decontv_m1",
		.table[BS_DEFAULT].table_list = axiqos_0xcccc_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.table[BS_MFC_UD_ENCODING_ENABLE].table_list = mo_decon_tv_static_table,
		.table[BS_MFC_UD_ENCODING_ENABLE].table_num = ARRAY_SIZE(mo_decon_tv_static_table),
		.table[BS_MFC_UD_ENCODING_DISABLE].table_list = axiqos_0xcccc_table,
		.table[BS_MFC_UD_ENCODING_DISABLE].table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.table[BS_CAM_SCEN_ENABLE].table_list = mo_decon_tv_static_table,
		.table[BS_CAM_SCEN_ENABLE].table_num = ARRAY_SIZE(mo_decon_tv_static_table),
		.table[BS_CAM_SCEN_DISABLE].table_list = axiqos_0xcccc_table,
		.table[BS_CAM_SCEN_DISABLE].table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_DECONTV_M2] = {
		.id = BTS_DECONTV_M2,
		.name = "decontv_m2",
		.pa_base = EXYNOS5430_PA_BTS_DECONTV_M2,
		.pd_name = "spd-decon-tv",
		.clk_name = "gate_bts_decontv_m2",
		.table[BS_DEFAULT].table_list = axiqos_0xcccc_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.table[BS_MFC_UD_ENCODING_ENABLE].table_list = mo_decon_tv_static_table,
		.table[BS_MFC_UD_ENCODING_ENABLE].table_num = ARRAY_SIZE(mo_decon_tv_static_table),
		.table[BS_MFC_UD_ENCODING_DISABLE].table_list = axiqos_0xcccc_table,
		.table[BS_MFC_UD_ENCODING_DISABLE].table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.table[BS_CAM_SCEN_ENABLE].table_list = mo_decon_tv_static_table,
		.table[BS_CAM_SCEN_ENABLE].table_num = ARRAY_SIZE(mo_decon_tv_static_table),
		.table[BS_CAM_SCEN_DISABLE].table_list = axiqos_0xcccc_table,
		.table[BS_CAM_SCEN_DISABLE].table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_DECONTV_M3] = {
		.id = BTS_DECONTV_M3,
		.name = "decontv_m3",
		.pa_base = EXYNOS5430_PA_BTS_DECONTV_M3,
		.pd_name = "spd-decon-tv",
		.clk_name = "gate_bts_decontv_m3",
		.table[BS_DEFAULT].table_list = axiqos_0xcccc_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.table[BS_MFC_UD_ENCODING_ENABLE].table_list = mo_decon_tv_static_table,
		.table[BS_MFC_UD_ENCODING_ENABLE].table_num = ARRAY_SIZE(mo_decon_tv_static_table),
		.table[BS_MFC_UD_ENCODING_DISABLE].table_list = axiqos_0xcccc_table,
		.table[BS_MFC_UD_ENCODING_DISABLE].table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.table[BS_CAM_SCEN_ENABLE].table_list = mo_decon_tv_static_table,
		.table[BS_CAM_SCEN_ENABLE].table_num = ARRAY_SIZE(mo_decon_tv_static_table),
		.table[BS_CAM_SCEN_DISABLE].table_list = axiqos_0xcccc_table,
		.table[BS_CAM_SCEN_DISABLE].table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_FIMC_LITE_A] = {
		.id = BTS_FIMC_LITE_A,
		.name = "fimc_lite_a",
		.pa_base = EXYNOS5430_PA_BTS_FIMC_LITE0,
		.pd_name = "pd-cam0",
		.clk_name = "gate_bts_lite_a",
		.table[BS_CAM_SCEN_ENABLE].table_list = axiqos_0xeeee_table,
		.table[BS_CAM_SCEN_ENABLE].table_num = ARRAY_SIZE(axiqos_0xeeee_table),
		.table[BS_CAM_SCEN_DISABLE].table_list = disable_table,
		.table[BS_CAM_SCEN_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_FIMC_LITE_B] = {
		.id = BTS_FIMC_LITE_B,
		.name = "fimc_lite_b",
		.pa_base = EXYNOS5430_PA_BTS_FIMC_LITE1,
		.pd_name = "pd-cam0",
		.clk_name = "gate_bts_lite_b",
		.table[BS_CAM_SCEN_ENABLE].table_list = axiqos_0xeeee_table,
		.table[BS_CAM_SCEN_ENABLE].table_num = ARRAY_SIZE(axiqos_0xeeee_table),
		.table[BS_CAM_SCEN_DISABLE].table_list = disable_table,
		.table[BS_CAM_SCEN_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_FIMC_LITE_D] = {
		.id = BTS_FIMC_LITE_D,
		.name = "fimc_lite_d",
		.pa_base = EXYNOS5430_PA_BTS_FIMC_LITE3,
		.pd_name = "pd-cam0",
		.clk_name = "gate_bts_lite_d",
		.table[BS_CAM_SCEN_ENABLE].table_list = axiqos_0xeeee_table,
		.table[BS_CAM_SCEN_ENABLE].table_num = ARRAY_SIZE(axiqos_0xeeee_table),
		.table[BS_CAM_SCEN_DISABLE].table_list = disable_table,
		.table[BS_CAM_SCEN_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_3AA0] = {
		.id = BTS_3AA0,
		.name = "3aa0",
		.pa_base = EXYNOS5430_PA_BTS_3AA0,
		.pd_name = "pd-cam0",
		.clk_name = "gate_bts_3aa0",
		.table[BS_CAM_SCEN_ENABLE].table_list = axiqos_0xeeee_table,
		.table[BS_CAM_SCEN_ENABLE].table_num = ARRAY_SIZE(axiqos_0xeeee_table),
		.table[BS_CAM_SCEN_DISABLE].table_list = disable_table,
		.table[BS_CAM_SCEN_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_3AA1] = {
		.id = BTS_3AA1,
		.name = "3aa1",
		.pa_base = EXYNOS5430_PA_BTS_3AA1,
		.pd_name = "pd-cam0",
		.clk_name = "gate_bts_3aa1",
		.table[BS_CAM_SCEN_ENABLE].table_list = mo_3aa1_static_table,
		.table[BS_CAM_SCEN_ENABLE].table_num = ARRAY_SIZE(mo_3aa1_static_table),
		.table[BS_CAM_SCEN_DISABLE].table_list = disable_table,
		.table[BS_CAM_SCEN_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_GSCL0] = {
		.id = BTS_GSCL0,
		.name = "gscl",
		.pa_base = EXYNOS5430_PA_BTS_GSCL0,
		.pd_name = "spd-gscl0",
		.clk_name = "gate_bts_gscl0",
		.table[BS_DEFAULT].table_list = disable_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(disable_table),
		.table[BS_GSCL_OTF_ENABLE].table_list = axiqos_0xffff_table,
		.table[BS_GSCL_OTF_ENABLE].table_num = ARRAY_SIZE(axiqos_0xffff_table),
		.table[BS_GSCL_OTF_DISABLE].table_list = disable_table,
		.table[BS_GSCL_OTF_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_GSCL1] = {
		.id = BTS_GSCL1,
		.name = "gscl1",
		.pa_base = EXYNOS5430_PA_BTS_GSCL1,
		.pd_name = "spd-gscl1",
		.clk_name = "gate_bts_gscl1",
		.table[BS_DEFAULT].table_list = disable_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(disable_table),
		.table[BS_GSCL_OTF_ENABLE].table_list = axiqos_0xffff_table,
		.table[BS_GSCL_OTF_ENABLE].table_num = ARRAY_SIZE(axiqos_0xffff_table),
		.table[BS_GSCL_OTF_DISABLE].table_list = disable_table,
		.table[BS_GSCL_OTF_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_GSCL2] = {
		.id = BTS_GSCL2,
		.name = "gscl2",
		.pa_base = EXYNOS5430_PA_BTS_GSCL2,
		.pd_name = "spd-gscl2",
		.clk_name = "gate_bts_gscl2",
		.table[BS_DEFAULT].table_list = disable_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(disable_table),
		.table[BS_GSCL_OTF_ENABLE].table_list = axiqos_0xffff_table,
		.table[BS_GSCL_OTF_ENABLE].table_num = ARRAY_SIZE(axiqos_0xffff_table),
		.table[BS_GSCL_OTF_DISABLE].table_list = disable_table,
		.table[BS_GSCL_OTF_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_MFC0] = {
		.id = BTS_MFC0,
		.name = "mfc0",
		.pa_base = EXYNOS5433_PA_BTS_MFC0,
		.pd_name = "pd-mfc",
		.clk_name = "gate_bts_mfc0_0",
		.table[BS_DEFAULT].table_list = disable_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(disable_table),
		.table[BS_MFC_UD_ENCODING_ENABLE].table_list = axiqos_uhd_mfc_table,
		.table[BS_MFC_UD_ENCODING_ENABLE].table_num = ARRAY_SIZE(axiqos_uhd_mfc_table),
		.table[BS_MFC_UD_ENCODING_DISABLE].table_list = disable_table,
		.table[BS_MFC_UD_ENCODING_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.table[BS_MFC_UD_DECODING_ENABLE].table_list = axiqos_uhd_mfc_table,
		.table[BS_MFC_UD_DECODING_ENABLE].table_num = ARRAY_SIZE(axiqos_uhd_mfc_table),
		.table[BS_MFC_UD_DECODING_DISABLE].table_list = disable_table,
		.table[BS_MFC_UD_DECODING_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_MFC1] = {
		.id = BTS_MFC1,
		.name = "mfc1",
		.pa_base = EXYNOS5433_PA_BTS_MFC1,
		.pd_name = "pd-mfc",
		.clk_name = "gate_bts_mfc0_1",
		.table[BS_DEFAULT].table_list = disable_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(disable_table),
		.table[BS_MFC_UD_ENCODING_ENABLE].table_list = axiqos_uhd_mfc_table,
		.table[BS_MFC_UD_ENCODING_ENABLE].table_num = ARRAY_SIZE(axiqos_uhd_mfc_table),
		.table[BS_MFC_UD_ENCODING_DISABLE].table_list = disable_table,
		.table[BS_MFC_UD_ENCODING_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.table[BS_MFC_UD_DECODING_ENABLE].table_list = axiqos_uhd_mfc_table,
		.table[BS_MFC_UD_DECODING_ENABLE].table_num = ARRAY_SIZE(axiqos_uhd_mfc_table),
		.table[BS_MFC_UD_DECODING_DISABLE].table_list = disable_table,
		.table[BS_MFC_UD_DECODING_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_G3D0] = {
		.id = BTS_G3D0,
		.name = "g3d0",
		.pa_base = EXYNOS5433_PA_BTS_G3D0,
		.pd_name = "pd-g3d",
		.clk_name = "gate_bts_g3d0",
		.table[BS_DEFAULT].table_list = disable_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(disable_table),
		.table[BS_G3D_SCENARIO_ENABLE].table_list = axiqos_g3d_table,
		.table[BS_G3D_SCENARIO_ENABLE].table_num = ARRAY_SIZE(axiqos_g3d_table),
		.table[BS_G3D_SCENARIO_DISABLE].table_list = disable_table,
		.table[BS_G3D_SCENARIO_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_G3D1] = {
		.id = BTS_G3D1,
		.name = "g3d1",
		.pa_base = EXYNOS5433_PA_BTS_G3D1,
		.pd_name = "pd-g3d",
		.clk_name = "gate_bts_g3d1",
		.table[BS_DEFAULT].table_list = disable_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(disable_table),
		.table[BS_G3D_SCENARIO_ENABLE].table_list = axiqos_g3d_table,
		.table[BS_G3D_SCENARIO_ENABLE].table_num = ARRAY_SIZE(axiqos_g3d_table),
		.table[BS_G3D_SCENARIO_DISABLE].table_list = disable_table,
		.table[BS_G3D_SCENARIO_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.table[BS_DISABLE].table_list = disable_table,
		.table[BS_DISABLE].table_num = ARRAY_SIZE(disable_table),
		.scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_MSCL0] = {
		.id = MAKE_BTS_ID(0),
		.name = "mscl0",
		.pa_base = EXYNOS5430_PA_BTS_MSCL0,
		.pd_name = "spd-mscl0",
		.clk_name = "gate_bts_m2mscaler0",
		.on = false,
	},
	[BTS_IDX_MSCL1] = {
		.id = MAKE_BTS_ID(1),
		.name = "mscl1",
		.pa_base = EXYNOS5430_PA_BTS_MSCL1,
		.pd_name = "spd-mscl1",
		.clk_name = "gate_bts_m2mscaler1",
		.on = false,
	},
	[BTS_IDX_JPEG] = {
		.id = MAKE_BTS_ID(2),
		.name = "jpeg",
		.pa_base = EXYNOS5430_PA_BTS_JPEG,
		.pd_name = "spd-jpeg",
		.clk_name = "gate_bts_jpeg",
		.on = false,
	},
	[BTS_IDX_FIMC_FD] = {
		.id = MAKE_BTS_ID(3),
		.name = "fimc_fd",
		.pa_base = EXYNOS5430_PA_BTS_FIMC_FD,
		.pd_name = "pd-cam1",
		.clk_name = "gate_bts_fd",
		.on = false,
	},
	[BTS_IDX_ISPCPU] = {
		.id = MAKE_BTS_ID(4),
		.name = "ispcpu",
		.pa_base = EXYNOS5430_PA_BTS_ISPCPU,
		.pd_name = "pd-cam1",
		.clk_name = "gate_bts_isp3p",
		.on = false,
	},
	[BTS_IDX_FIMC_ISP] = {
		.id = MAKE_BTS_ID(5),
		.name = "fimc_isp",
		.pa_base = EXYNOS5430_PA_BTS_FIMC_ISP,
		.pd_name = "pd-isp",
		.clk_name = "gate_bts_isp",
		.on = false,
	},
	[BTS_IDX_FIMC_DRC] = {
		.id = MAKE_BTS_ID(6),
		.name = "fimc_drc",
		.pa_base = EXYNOS5430_PA_BTS_FIMC_DRC,
		.pd_name = "pd-isp",
		.clk_name = "gate_bts_drc",
		.on = false,
	},
	[BTS_IDX_FIMC_SCLC] = {
		.id = MAKE_BTS_ID(7),
		.name = "fimc_sclc",
		.pa_base = EXYNOS5430_PA_BTS_FIMC_SCLC,
		.pd_name = "pd-isp",
		.clk_name = "gate_bts_scalerc",
		.on = false,
	},
	[BTS_IDX_FIMC_DIS0] = {
		.id = MAKE_BTS_ID(8),
		.name = "fimc_dis0",
		.pa_base = EXYNOS5430_PA_BTS_FIMC_DIS0,
		.pd_name = "pd-isp",
		.clk_name = "gate_bts_dis0",
		.on = false,
	},
	[BTS_IDX_FIMC_DIS1] = {
		.id = MAKE_BTS_ID(9),
		.name = "fimc_dis1",
		.pa_base = EXYNOS5430_PA_BTS_FIMC_DIS1,
		.pd_name = "pd-isp",
		.clk_name = "gate_bts_dis1",
		.on = false,
	},
	[BTS_IDX_FIMC_SCLP] = {
		.id = MAKE_BTS_ID(10),
		.name = "fimc_sclp",
		.pa_base = EXYNOS5430_PA_BTS_FIMC_SCLP,
		.pd_name = "pd-isp",
		.clk_name = "gate_bts_scalerp",
		.on = false,
	},
	[BTS_IDX_FIMC_3DNR] = {
		.id = MAKE_BTS_ID(11),
		.name = "fimc_3dnr",
		.pa_base = EXYNOS5430_PA_BTS_FIMC_3DNR,
		.pd_name = "pd-isp",
		.clk_name = "gate_bts_3dnr",
		.on = false,
	},
	[BTS_IDX_G2D] = {
		.id = MAKE_BTS_ID(18),
		.name = "g2d",
		.pa_base = EXYNOS5430_PA_BTS_G2D,
		.pd_name = "pd-g2d",
		.clk_name = "gate_bts_g2d",
		.on = false,
	},
	[BTS_IDX_HEVC0] = {
		.id = MAKE_BTS_ID(19),
		.name = "hevc0",
		.pa_base = EXYNOS5430_PA_BTS_HEVC0,
		.pd_name = "pd-hevc",
		.clk_name = "gate_bts_hevc_0",
		.on = false,
	},
	[BTS_IDX_HEVC1] = {
		.id = MAKE_BTS_ID(20),
		.name = "hevc1",
		.pa_base = EXYNOS5430_PA_BTS_HEVC1,
		.pd_name = "pd-hevc",
		.clk_name = "gate_bts_hevc_1",
		.on = false,
	},
};

static struct bts_scenario bts_scen[] = {
	[BS_DEFAULT] = {
		.name = "bts_default",
		.id = BS_DEFAULT,
	},
	[BS_MFC_UD_DECODING_ENABLE] = {
		.name = "bts_mfc_ud_decoding_enabled",
		.ip = BTS_MFC,
		.id = BS_MFC_UD_DECODING_ENABLE,
	},
	[BS_MFC_UD_DECODING_DISABLE] = {
		.name = "bts_mfc_ud_decoding_disable",
		.ip = BTS_MFC,
		.id = BS_MFC_UD_DECODING_DISABLE,
	},
	[BS_MFC_UD_ENCODING_ENABLE] = {
		.name = "bts_mfc_ud_encoding_enabled",
		.ip = BTS_MFC | BTS_DECON | BTS_DECONTV,
		.id = BS_MFC_UD_ENCODING_ENABLE,
	},
	[BS_MFC_UD_ENCODING_DISABLE] = {
		.name = "bts_mfc_ud_encoding_disable",
		.ip = BTS_MFC | BTS_DECON | BTS_DECONTV,
		.id = BS_MFC_UD_ENCODING_DISABLE,
	},
	[BS_CAM_SCEN_ENABLE] = {
		.name = "bts_cam_scen_enable",
		.ip = BTS_DECON | BTS_DECONTV,
		.id = BS_CAM_SCEN_ENABLE,
	},
	[BS_CAM_SCEN_DISABLE] = {
		.name = "bts_cam_scen_disable",
		.ip = BTS_DECON | BTS_DECONTV,
		.id = BS_CAM_SCEN_DISABLE,
	},
	[BS_GSCL_OTF_ENABLE] = {
		.name = "bts_gscl_otf_enable",
		.ip = BTS_GSCL,
		.id = BS_GSCL_OTF_ENABLE,
	},
	[BS_GSCL_OTF_DISABLE] = {
		.name = "bts_gscl_otf_disable",
		.ip = BTS_GSCL,
		.id = BS_GSCL_OTF_DISABLE,
	},
	[BS_G3D_SCENARIO_ENABLE] = {
		.name = "bts_g3d_scenario_eanble",
		.ip = BTS_G3D,
		.id = BS_G3D_SCENARIO_ENABLE,
	},
	[BS_G3D_SCENARIO_DISABLE] = {
		.name = "bts_g3d_scenario_disable",
		.ip = BTS_G3D,
		.id = BS_G3D_SCENARIO_DISABLE,
	},
	[BS_DISABLE] = {
		.name = "bts_disable",
		.id = BS_DISABLE,
	},
	[BS_MAX] = {
		.name = "undefined"
	}
};

static DEFINE_SPINLOCK(bts_lock);
static LIST_HEAD(bts_list);
static LIST_HEAD(bts_scen_list);

static enum exynos_bts_scenario bts_get_scen(struct bts_info *bts)
{
	enum exynos_bts_scenario scen;

	scen = bts->on ? BS_DEFAULT : BS_DISABLE;

	if (bts->id & BTS_CAM) {
		update_cam(bts->on);
		scen = bts->on ? BS_CAM_SCEN_ENABLE : BS_CAM_SCEN_DISABLE;
	} else if ((bts->id & BTS_DISPLAY) && bts->on) {
		if (pr_state.cam)
			scen = BS_CAM_SCEN_ENABLE;
		else if (pr_state.ud_scen)
			scen = BS_MFC_UD_ENCODING_ENABLE;
	}

	return scen;
}

static void set_bts_ip_table(enum exynos_bts_scenario scen,
				struct bts_info *bts)
{
	int i;
	struct bts_set_table *table = bts->table[scen].table_list;

	if (!table)
		return;

	BTS_DBG("[BTS] %s on:%d bts scen: [%s]->[%s]\n", bts->name, bts->on,
			bts_scen[bts->scen].name, bts_scen[scen].name);

	if (bts->scen == scen)
		return;

	if (bts->on && bts->clk)
		clk_enable(bts->clk);

	for (i = 0; i < bts->table[scen].table_num; i++) {
		__raw_writel(table->val, bts->va_base + table->reg);
		BTS_DBG1("[BTS] %x-%x\n", table->reg, table->val);
		table++;
	}

	if (!bts->on && bts->clk)
		clk_disable(bts->clk);

	bts->scen = scen;
}

static void set_bts_scenario(enum exynos_bts_scenario scen)
{
	struct bts_info *bts;

	if (scen == BS_DEFAULT)
		return;

	list_for_each_entry(bts, &bts_scen_list, scen_list)
		if (bts->id & bts_scen[scen].ip)
			if (bts->scen != scen && bts->on)
				set_bts_ip_table(scen, bts);
}

void bts_scen_update(enum bts_scen_type type, unsigned int val)
{
	enum exynos_bts_scenario scen = BS_DEFAULT;

	spin_lock(&bts_lock);

	switch (type) {
	case TYPE_MFC_UD_DECODING:

		scen = val ? BS_MFC_UD_DECODING_ENABLE : BS_MFC_UD_DECODING_DISABLE;
		BTS_DBG("[BTS] MFC_UD_DECODING: %s\n", bts_scen[scen].name);

		update_ud_scen(val);
		break;
	case TYPE_MFC_UD_ENCODING:

		scen = val ? BS_MFC_UD_ENCODING_ENABLE : BS_MFC_UD_ENCODING_DISABLE;
		BTS_DBG("[BTS] MFC_UD_ENCODING: %s\n", bts_scen[scen].name);

		update_ud_scen(val);
		break;
	case TYPE_G3D_SCENARIO:

		scen = val ? BS_G3D_SCENARIO_ENABLE: BS_G3D_SCENARIO_DISABLE;
		BTS_DBG("[BTS] G3D SCENARIO: %s\n", bts_scen[scen].name);

		update_g3d_scen(val);
		break;
	default:
		break;
	}

	set_bts_scenario(scen);

	spin_unlock(&bts_lock);
}

void bts_otf_initialize(unsigned int id, bool on)
{
	enum exynos_bts_scenario scen = BS_DEFAULT;
	BTS_DBG("[%s] pd_name: %s, on/off:%x\n", __func__, exynos5_bts[id + BTS_IDX_GSCL0].pd_name, on);

	spin_lock(&bts_lock);

	if (!exynos5_bts[id + BTS_IDX_GSCL0].on) {
		spin_unlock(&bts_lock);
		return;
	}

	scen = on ? BS_GSCL_OTF_ENABLE : BS_GSCL_OTF_DISABLE;

	set_bts_ip_table(scen, &exynos5_bts[id + BTS_IDX_GSCL0]);

	spin_unlock(&bts_lock);
}

void exynos5_bts_show_mo_status(void)
{
	unsigned int i;
	unsigned int r_ctrl, w_ctrl;
	unsigned int r_mo, w_mo;
	unsigned int drex_lpi_pause, drex_empty_state;
	unsigned int drex_r_occupancy, drex_w_occupancy;

	pr_err("--------DUMP ACTIVATED BTS MO COUNT & DREX STATUS----------\n");
	pr_err("-----------------------------------------------------------\n");
	drex_lpi_pause = __raw_readl(drex0_va_base + 0x500);
	drex_empty_state = __raw_readl(drex0_va_base + 0x504);
	drex_r_occupancy = __raw_readl(drex0_va_base + 0x508);
	drex_w_occupancy = __raw_readl(drex0_va_base + 0x50C);
	pr_err("DREX0 LPI_PAUSE: %#x, EMPTY_STATE: %#x, "
			"R_OCCUPANCY: %#x, W_OCCUPANCY: %#x\n",
			drex_lpi_pause, drex_empty_state,
			drex_r_occupancy, drex_w_occupancy);
	drex_lpi_pause = __raw_readl(drex1_va_base + 0x500);
	drex_empty_state = __raw_readl(drex1_va_base + 0x504);
	drex_r_occupancy = __raw_readl(drex1_va_base + 0x508);
	drex_w_occupancy = __raw_readl(drex1_va_base + 0x50C);
	pr_err("DREX1 LPI_PAUSE: %#x, EMPTY_STATE: %#x, "
			"R_OCCUPANCY: %#x, W_OCCUPANCY: %#x\n",
			drex_lpi_pause, drex_empty_state,
			drex_r_occupancy, drex_w_occupancy);
	pr_err("-----------------------------------------------------------\n");
	for(i = 0; i < ARRAY_SIZE(exynos5_bts); i++) {
		if (exynos5_bts[i].on) {
			if (!__clk_is_enabled(exynos5_bts[i].clk))
				return;
			r_ctrl = __raw_readl(exynos5_bts[i].va_base + READ_QOS_CONTROL);
			w_ctrl = __raw_readl(exynos5_bts[i].va_base + WRITE_QOS_CONTROL);
			r_mo = __raw_readl(exynos5_bts[i].va_base + READ_MO);
			w_mo = __raw_readl(exynos5_bts[i].va_base + WRITE_MO);
			pr_err("BTS[%s] R_MO: %d, W_MO: %d, R_CTRL: %#x, W_CTRL: %#x\n",
					exynos5_bts[i].name, r_mo, w_mo, r_ctrl, w_ctrl);

		}
	}
	pr_err("-----------------------------------------------------------\n");
}

void bts_initialize(const char *pd_name, bool on)
{
	struct bts_info *bts;
	enum exynos_bts_scenario scen = BS_DEFAULT;

	spin_lock(&bts_lock);

	BTS_DBG("[%s] pd_name: %s, on/off:%x\n", __func__, pd_name, on);
	list_for_each_entry(bts, &bts_list, list)
		if (pd_name && bts->pd_name && !strcmp(bts->pd_name, pd_name)) {
			BTS_DBG("[BTS] %s on/off:%d->%d\n", bts->name, bts->on, on);
			bts->on = on;

			if (!bts->enable) continue;

			scen = bts_get_scen(bts);
			set_bts_ip_table(scen, bts);
		}

	set_bts_scenario(scen);

	spin_unlock(&bts_lock);
}

static int exynos5_bts_status_open_show(struct seq_file *buf, void *d)
{
	unsigned int i;
	unsigned int val_r, val_w;

	for(i = 0; i < ARRAY_SIZE(exynos5_bts); i++) {
		if (exynos5_bts[i].on) {
			if (exynos5_bts[i].clk)
				clk_enable(exynos5_bts[i].clk);

			val_r = __raw_readl(exynos5_bts[i].va_base + READ_QOS_CONTROL);
			val_w = __raw_readl(exynos5_bts[i].va_base + WRITE_QOS_CONTROL);
			if (val_r && val_w) {
				val_r = __raw_readl(exynos5_bts[i].va_base + READ_CHANNEL_PRIORITY);
				val_w = __raw_readl(exynos5_bts[i].va_base + WRITE_CHANNEL_PRIORITY);
				seq_printf(buf, "%s on, priority ch_r:0x%x,ch_w:0x%x, scen:%s\n",
						exynos5_bts[i].name, val_r, val_w,
						bts_scen[exynos5_bts[i].scen].name);
			} else {
				seq_printf(buf, "%s control disable, scen:%s\n", exynos5_bts[i].name,
						bts_scen[exynos5_bts[i].scen].name);
			}

			if (exynos5_bts[i].clk)
				clk_disable(exynos5_bts[i].clk);
		} else {
			seq_printf(buf, "%s off\n", exynos5_bts[i].name);
		}
	}

	return 0;
}

static int exynos5_bts_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos5_bts_status_open_show, inode->i_private);
}

static const struct file_operations debug_status_fops = {
	.open		= exynos5_bts_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void bts_debugfs(void)
{
	struct dentry *den;

	den = debugfs_create_dir("bts_dbg", NULL);
	debugfs_create_file("bts_status", 0440,
			den, NULL, &debug_status_fops);
}

static void bts_drex_init(void)
{

	BTS_DBG("[BTS][%s] bts drex init\n", __func__);

	__raw_writel(0x00000000, drex0_va_base + 0xD8);
	__raw_writel(0x0FFF0FFF, drex0_va_base + 0xD0);
	__raw_writel(0x0FFF0FFF, drex0_va_base + 0xC8);
	__raw_writel(0x0FFF0FFF, drex0_va_base + 0xC0);
	__raw_writel(0x025E0294, drex0_va_base + 0xB0);
	__raw_writel(0x0FFF0FFF, drex0_va_base + 0xA0);
	__raw_writel(0x00000000, drex0_va_base + 0x100);
	__raw_writel(0x88558855, drex0_va_base + 0x104);
	__raw_writel(0x00000000, drex1_va_base + 0xD8);
	__raw_writel(0x0FFF0FFF, drex1_va_base + 0xD0);
	__raw_writel(0x0FFF0FFF, drex1_va_base + 0xC8);
	__raw_writel(0x0FFF0FFF, drex1_va_base + 0xC0);
	__raw_writel(0x025E0294, drex1_va_base + 0xB0);
	__raw_writel(0x0FFF0FFF, drex1_va_base + 0xA0);
	__raw_writel(0x00000000, drex1_va_base + 0x100);
	__raw_writel(0x88558855, drex1_va_base + 0x104);
}

static int exynos_bts_notifier_event(struct notifier_block *this,
					unsigned long event,
					void *ptr)
{
	switch (event) {
	case PM_POST_SUSPEND:

		bts_drex_init();
		return NOTIFY_OK;

		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block exynos_bts_notifier = {
	.notifier_call = exynos_bts_notifier_event,
};

static int __init exynos5_bts_init(void)
{
	int i;
	struct clk *clk;

	BTS_DBG("[BTS][%s] bts init\n", __func__);

	for (i = 0; i < ARRAY_SIZE(exynos5_bts); i++) {
		exynos5_bts[i].va_base
			= ioremap(exynos5_bts[i].pa_base, SZ_4K);

		if (exynos5_bts[i].clk_name) {
			clk = __clk_lookup(exynos5_bts[i].clk_name);
			if (IS_ERR(clk))
				pr_err("failed to get bts clk %s\n",
						exynos5_bts[i].clk_name);
			else {
				exynos5_bts[i].clk = clk;
				clk_prepare(exynos5_bts[i].clk);
				clk_enable(exynos5_bts[i].clk);
				printk("bts name = %s, state = %d\n",
					exynos5_bts[i].name, exynos5_bts[i].on);
			}
		}

		list_add(&exynos5_bts[i].list, &bts_list);

		if (is_bts_scen_ip(exynos5_bts[i].id))
			list_add(&exynos5_bts[i].scen_list, &bts_scen_list);
	}

	drex0_va_base = ioremap(EXYNOS5430_PA_DREX0, SZ_4K);
	drex1_va_base = ioremap(EXYNOS5430_PA_DREX1, SZ_4K);

	bts_drex_init();

	bts_debugfs();

	register_pm_notifier(&exynos_bts_notifier);

	return 0;
}
arch_initcall(exynos5_bts_init);
