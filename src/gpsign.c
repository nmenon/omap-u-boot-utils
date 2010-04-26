/**
 * @file
 * @brief Signing utility for omap3
 *
 * FileName: src/chsign.c
 *
 * Read the .bin file and write out the .bin.ift file.
 * The signed image is the original pre-pended with the size of the image
 * and the load address.  If not entered on command line, file name is
 * assumed to be x-load.bin in current directory and load address is
 * 0x40200800.
 */
/*
 * (C) Copyright 2009
 * Texas Instruments, <www.ti.com>
 * Nishanth Menon <nm@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>

#include "lcfg_static.h"
#include "rev.h"
#include "compare.h"

#define CONFIG_FILE_ARG "c"
#define CONFIG_FILE_ARG_C 'c'
#define IMAGE_FILE_ARG "f"
#define IMAGE_FILE_ARG_C 'f'
#define LOAD_ADDRESS_ARG "l"
#define LOAD_ADDRESS_ARG_C 'l'

/********************** CHSETTINGS STRUCTURES *****************************/
#define CH_SETTINGS "CHSETTINGS"
#define CH_RAM "CHRAM"
#define CH_FLASH "CHFLASH"
#define CH_MMCSD "CHMMCSD"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
/* Every TOC has the same structure */
#define OFFSET_TOC_START 0x0
struct ch_toc_struct {
	u32 start;
	u32 size;
	u8 reserved[12];
	u8 name[12];
} __attribute__ ((__packed__));

#define OFFSET_START_CHSETTINGS 0xA0
struct ch_settings {
#define CH_S_KEY 0xC0C0C0C1
	u32 section_key;
#define CH_S_VALID 0x1
	u8 valid;		/* 1 for valid, 0 for not valid */
#define CH_S_VERSION 0x1
	u8 version;
	u16 reserved;
#define CH_S_APPLY_SETTING	(0x1 << 0)
#define CH_S_PERFORM_CLK_S	(0x1 << 2)
#define CH_S_LOCK_DPLL4		(0x1 << 3)
#define CH_S_LOCK_DPLL1		(0x1 << 4)
#define CH_S_LOCK_DPLL3		(0x1 << 5)
#define CH_S_BYPASS_DPLL4	(0x1 << 6)
#define CH_S_BYPASS_DPLL1	(0x1 << 7)
#define CH_S_BYPASS_DPLL3	(0x1 << 8)
#define CH_S_CLK_12P0MHZ	(0x1 << 24)
#define CH_S_CLK_13P0MHZ	(0x2 << 24)
#define CH_S_CLK_16P8MHZ	(0x3 << 24)
#define CH_S_CLK_19P2MHZ	(0x4 << 24)
#define CH_S_CLK_26P0MHZ	(0x5 << 24)
#define CH_S_CLK_38P4MHZ	(0x6 << 24)
	u32 flags;
	/*General clock settings */
	u32 GEN_PRM_CLKSRC_CTRL;
	u32 GEN_PRM_CLKSEL;
	u32 GEN_CM_CLKSEL1_EMU;
	/* Clock Configuration */
	u32 CLK_CM_CLKSEL_CORE;
	u32 CLK_CM_CLKSEL_WKUP;
	u32 DPLL3_CM_CLKEN_PLL;
	u32 DPLL3_CM_AUTOIDLE_PLL;
	u32 DPLL3_CM_CLKSEL1_PLL;
	u32 DPLL4_CM_CLKEN_PLL;
	u32 DPLL4_CM_AUTOIDLE_PLL;
	u32 DPLL4_CM_CLKSEL2_PLL;
	u32 DPLL4_CM_CLKSEL3_PLL;
	u32 DPLL1_CM_CLKEN_PLL_MPU;
	u32 DPLL1_CM_AUTOIDLE_PLL_MPU;
	u32 DPLL1_CM_CLKSEL1_PLL_MPU;
	u32 DPLL1_CM_CLKSEL2_PLL_MPU;
	u32 DPLL1_CM_CLKSTCTRL_MPU;
} __attribute__ ((__packed__));

#define OFFSET_START_CHRAM 0xF0
struct ch_ram {
#define CH_R_KEY 0xC0C0C0C2
	u32 section_key;	/* 0x0 */
#define CH_R_VALID 0x1
	u8 valid;		/* 0x4 */
	u8 reserved0[3];	/* 0x5 */
	u16 SDRC_SYSCONFIG_LSB;	/* 0x8 */
	u16 SDRC_CS_CFG_LSB;
	u16 SDRC_SHARING_LSB;
	u16 SDRC_ERR_TYPE_LSB;
	u32 SDRC_DLLA_CTRL;
	u32 reserved1;
	u32 SDRC_POWER;
	/* CS0 */
#define CH_R_TYPE_SDRAM		0
#define CH_R_TYPE_LPSDR		1
#define CH_R_TYPE_DDR		2
#define CH_R_TYPE_MDDR		3
#define CH_R_TYPE_UNKNOWN	4
	u16 mem_type_cs0;
	u16 reserved2;
	u32 SDRC_MCFG_0;
	u16 SDRC_MR_0_LSB;
	u16 SDRC_EMR1_0_LSB;
	u16 SDRC_EMR2_0_LSB;
	u16 SDRC_EMR3_0_LSB;
	u32 SDRC_ACTIM_CTRLA_0;
	u32 SDRC_ACTIM_CTRLB_0;
	u32 SDRC_RFRCTRL_0;
	/* CS1 */
	u16 mem_type_cs1;
	u16 reserved3;
	u32 SDRC_MCFG_1;
	u16 SDRC_MR_1_LSB;
	u16 SDRC_EMR1_1_LSB;
	u16 SDRC_EMR2_1_LSB;
	u16 SDRC_EMR3_1_LSB;
	u32 SDRC_ACTIM_CTRLA_1;
	u32 SDRC_ACTIM_CTRLB_1;
	u32 SDRC_RFRCTRL_1;

	u32 reserved4;

#define CH_R_FLAGS_CS0_CONFIGURED	(0x1 << 0)
#define CH_R_FLAGS_CS1_CONFIGURED	(0x1 << 1)
	u16 flags;

	u16 reserved5;
} __attribute__ ((__packed__));

#define OFFSET_START_CHFLASH	 0x14C
struct ch_flash {
#define CH_F_KEY		0xC0C0C0C3
	u32 section_key;	/* 0x0 */
#define CH_F_VALID		0x1
	u8 valid;		/* 0x4 */
	u8 reserved[3];		/* 0x5 */
	u16 GPMC_SYSCONFIG_LSB;	/* 0x8 */
	u16 GPMC_IRQENABLE_LSB;	/* 0xA */
	u16 GPMC_TIMEOUT_CONTROL_LSB;	/* 0xC */
	u16 GPMC_CONFIG_LSB;	/* 0xE */
	u32 GPMC_CONFIG1_0;	/* 0x10 */
	u32 GPMC_CONFIG2_0;	/* 0x14 */
	u32 GPMC_CONFIG3_0;	/* 0x18 */
	u32 GPMC_CONFIG4_0;	/* 0x1C */
	u32 GPMC_CONFIG5_0;	/* 0x20 */
	u32 GPMC_CONFIG6_0;	/* 0x24 */
	u32 GPMC_CONFIG7_0;	/* 0x28 */
	u32 GPMC_PREFETCH_CONFIG1;	/* 0x2C */
	u16 GPMC_PREFETCH_CONFIG2_LSB;	/* 0x30 */
	u16 GPMC_PREFETCH_CONTROL_LSB;	/* 0x32 */
	u16 GPMC_ECC_CONFIG;	/* 0x34 */
	u16 GPMC_ECC_CONTROL;	/* 0x36 */
	u16 GPMC_ECC_SIZE_CONFIG_LSB;	/* 0x38 */
#define CH_F_ENABLEA1_A0	 1
	u16 enable_A1_A10;	/* 0x3A */
} __attribute__ ((__packed__));

#define OFFSET_START_CHMMCSD	0x188
struct ch_mmcsd {
#define CH_M_KEY		0xC0C0C0C3
	u32 section_key;	/* 0x0 */
#define CH_M_VALID		0x1
	u8 valid;		/* 0x4 */
	u8 reserved[3];		/* 0x5 */
	u16 MMCHS_SYSCTRL_MSB;	/* 0x8 */
#define CH_M_SYSCTRL_LSB_NOUPDATE	0xFFFF
	u16 MMCHS_SYSCTRL_LSB;	/* 0xA */
#define CH_M_BUS_WIDTH_1BIT		1
#define CH_M_BUS_WIDTH_4BIT		2
#define CH_M_BUS_WIDTH_8BIT		4
#define CH_M_BUS_WIDTH_NOUPDATE		0xFFFFFFFF
	u32 bus_width;		/* 0xC */
} __attribute__ ((__packed__));

/********************** VARIABLES *****************************/
static u8 ch_buffer[512];
static struct ch_settings platform_chsettings;
static struct ch_ram platform_chram;
static struct ch_flash platform_chflash;
static struct ch_mmcsd platform_chmmcsd;
unsigned int loadaddr;
static struct compare_map variable_map[] = {
/* *INDENT-OFF* */
	/* GENERAL SETTINGS */
	{"loadaddr", &loadaddr, TYPE_U32},

	/* CHSETTINGS VALUES */
	{"platform_chsettings.section_key", &platform_chsettings.section_key, TYPE_U32},
	{"platform_chsettings.valid", &platform_chsettings.valid, TYPE_U8},
	{"platform_chsettings.version", &platform_chsettings.version, TYPE_U8},
	{"platform_chsettings.flags", &platform_chsettings.flags, TYPE_U32},
	{"platform_chsettings.GEN_PRM_CLKSRC_CTRL", &platform_chsettings.GEN_PRM_CLKSRC_CTRL, TYPE_U32},
	{"platform_chsettings.GEN_PRM_CLKSEL", &platform_chsettings.GEN_PRM_CLKSEL, TYPE_U32},
	{"platform_chsettings.GEN_CM_CLKSEL1_EMU", &platform_chsettings.GEN_CM_CLKSEL1_EMU, TYPE_U32},
	{"platform_chsettings.CLK_CM_CLKSEL_CORE", &platform_chsettings.CLK_CM_CLKSEL_CORE, TYPE_U32},
	{"platform_chsettings.CLK_CM_CLKSEL_WKUP", &platform_chsettings.CLK_CM_CLKSEL_WKUP, TYPE_U32},
	{"platform_chsettings.DPLL3_CM_CLKEN_PLL", &platform_chsettings.DPLL3_CM_CLKEN_PLL, TYPE_U32},
	{"platform_chsettings.DPLL3_CM_AUTOIDLE_PLL", &platform_chsettings.DPLL3_CM_AUTOIDLE_PLL, TYPE_U32},
	{"platform_chsettings.DPLL3_CM_CLKSEL1_PLL", &platform_chsettings.DPLL3_CM_CLKSEL1_PLL, TYPE_U32},
	{"platform_chsettings.DPLL4_CM_CLKEN_PLL", &platform_chsettings.DPLL4_CM_CLKEN_PLL, TYPE_U32},
	{"platform_chsettings.DPLL4_CM_AUTOIDLE_PLL", &platform_chsettings.DPLL4_CM_AUTOIDLE_PLL, TYPE_U32},
	{"platform_chsettings.DPLL4_CM_CLKSEL2_PLL", &platform_chsettings.DPLL4_CM_CLKSEL2_PLL, TYPE_U32},
	{"platform_chsettings.DPLL4_CM_CLKSEL3_PLL", &platform_chsettings.DPLL4_CM_CLKSEL3_PLL, TYPE_U32},
	{"platform_chsettings.DPLL1_CM_CLKEN_PLL_MPU", &platform_chsettings.DPLL1_CM_CLKEN_PLL_MPU, TYPE_U32},
	{"platform_chsettings.DPLL1_CM_AUTOIDLE_PLL_MPU", &platform_chsettings.DPLL1_CM_AUTOIDLE_PLL_MPU, TYPE_U32},
	{"platform_chsettings.DPLL1_CM_CLKSEL1_PLL_MPU", &platform_chsettings.DPLL1_CM_CLKSEL1_PLL_MPU, TYPE_U32},
	{"platform_chsettings.DPLL1_CM_CLKSEL2_PLL_MPU", &platform_chsettings.DPLL1_CM_CLKSEL2_PLL_MPU, TYPE_U32},
	{"platform_chsettings.DPLL1_CM_CLKSTCTRL_MPU", &platform_chsettings.DPLL1_CM_CLKSTCTRL_MPU, TYPE_U32},

	/* CHRAM VALUES */
	{"platform_chram.section_key", &platform_chram.section_key, TYPE_U32},
	{"platform_chram.valid", &platform_chram.valid, TYPE_U8},
	{"platform_chram.SDRC_SYSCONFIG_LSB", &platform_chram.SDRC_SYSCONFIG_LSB, TYPE_U16},
	{"platform_chram.SDRC_CS_CFG_LSB", &platform_chram.SDRC_CS_CFG_LSB, TYPE_U16},
	{"platform_chram.SDRC_SHARING_LSB", &platform_chram.SDRC_SHARING_LSB, TYPE_U16},
	{"platform_chram.SDRC_ERR_TYPE_LSB", &platform_chram.SDRC_ERR_TYPE_LSB, TYPE_U16},
	{"platform_chram.SDRC_DLLA_CTRL", &platform_chram.SDRC_DLLA_CTRL, TYPE_U32},
	{"platform_chram.SDRC_POWER", &platform_chram.SDRC_POWER, TYPE_U32},
	{"platform_chram.mem_type_cs0", &platform_chram.mem_type_cs0, TYPE_U16},
	{"platform_chram.SDRC_MCFG_0", &platform_chram.SDRC_MCFG_0, TYPE_U32},
	{"platform_chram.SDRC_MR_0_LSB", &platform_chram.SDRC_MR_0_LSB, TYPE_U16},
	{"platform_chram.SDRC_EMR1_0_LSB", &platform_chram.SDRC_EMR1_0_LSB, TYPE_U16},
	{"platform_chram.SDRC_EMR2_0_LSB", &platform_chram.SDRC_EMR2_0_LSB, TYPE_U16},
	{"platform_chram.SDRC_EMR3_0_LSB", &platform_chram.SDRC_EMR3_0_LSB, TYPE_U16},
	{"platform_chram.SDRC_ACTIM_CTRLA_0", &platform_chram.SDRC_ACTIM_CTRLA_0, TYPE_U32},
	{"platform_chram.SDRC_ACTIM_CTRLB_0", &platform_chram.SDRC_ACTIM_CTRLB_0, TYPE_U32},
	{"platform_chram.SDRC_RFRCTRL_0", &platform_chram.SDRC_RFRCTRL_0, TYPE_U32},
	{"platform_chram.mem_type_cs1", &platform_chram.mem_type_cs1, TYPE_U16},
	{"platform_chram.SDRC_MCFG_1", &platform_chram.SDRC_MCFG_1, TYPE_U32},
	{"platform_chram.SDRC_MR_1_LSB", &platform_chram.SDRC_MR_1_LSB, TYPE_U16},
	{"platform_chram.SDRC_EMR1_1_LSB", &platform_chram.SDRC_EMR1_1_LSB, TYPE_U16},
	{"platform_chram.SDRC_EMR2_1_LSB", &platform_chram.SDRC_EMR2_1_LSB, TYPE_U16},
	{"platform_chram.SDRC_EMR3_1_LSB", &platform_chram.SDRC_EMR3_1_LSB, TYPE_U16},
	{"platform_chram.SDRC_ACTIM_CTRLA_1", &platform_chram.SDRC_ACTIM_CTRLA_1, TYPE_U32},
	{"platform_chram.SDRC_ACTIM_CTRLB_1", &platform_chram.SDRC_ACTIM_CTRLB_1, TYPE_U32},
	{"platform_chram.SDRC_RFRCTRL_1", &platform_chram.SDRC_RFRCTRL_1, TYPE_U32},
	{"platform_chram.flags", &platform_chram.flags, TYPE_U32},

	/* CHFLASH VALUES */
	{"platform_chflash.section_key", &platform_chflash.section_key, TYPE_U32},
	{"platform_chflash.valid", &platform_chflash.valid, TYPE_U8, },
	{"platform_chflash.GPMC_SYSCONFIG_LSB", &platform_chflash.GPMC_SYSCONFIG_LSB, TYPE_U16},
	{"platform_chflash.GPMC_IRQENABLE_LSB", &platform_chflash.GPMC_IRQENABLE_LSB, TYPE_U16},
	{"platform_chflash.GPMC_TIMEOUT_CONTROL_LSB", &platform_chflash.GPMC_TIMEOUT_CONTROL_LSB, TYPE_U16},
	{"platform_chflash.GPMC_CONFIG_LSB", &platform_chflash.GPMC_CONFIG_LSB, TYPE_U16},
	{"platform_chflash.GPMC_CONFIG1_0", &platform_chflash.GPMC_CONFIG1_0, TYPE_U32},
	{"platform_chflash.GPMC_CONFIG2_0", &platform_chflash.GPMC_CONFIG2_0, TYPE_U32},
	{"platform_chflash.GPMC_CONFIG3_0", &platform_chflash.GPMC_CONFIG3_0, TYPE_U32},
	{"platform_chflash.GPMC_CONFIG4_0", &platform_chflash.GPMC_CONFIG4_0, TYPE_U32},
	{"platform_chflash.GPMC_CONFIG5_0", &platform_chflash.GPMC_CONFIG5_0, TYPE_U32},
	{"platform_chflash.GPMC_CONFIG6_0", &platform_chflash.GPMC_CONFIG6_0, TYPE_U32},
	{"platform_chflash.GPMC_CONFIG7_0", &platform_chflash.GPMC_CONFIG7_0, TYPE_U32},
	{"platform_chflash.GPMC_PREFETCH_CONFIG1", &platform_chflash.GPMC_PREFETCH_CONFIG1, TYPE_U32},
	{"platform_chflash.GPMC_PREFETCH_CONFIG2_LSB", &platform_chflash.GPMC_PREFETCH_CONFIG2_LSB, TYPE_U16},
	{"platform_chflash.GPMC_PREFETCH_CONTROL_LSB", &platform_chflash.GPMC_PREFETCH_CONTROL_LSB, TYPE_U16},
	{"platform_chflash.GPMC_ECC_CONFIG", &platform_chflash.GPMC_ECC_CONFIG, TYPE_U16},
	{"platform_chflash.GPMC_ECC_CONTROL", &platform_chflash.GPMC_ECC_CONTROL, TYPE_U16},
	{"platform_chflash.GPMC_ECC_SIZE_CONFIG_LSB", &platform_chflash.GPMC_ECC_SIZE_CONFIG_LSB, TYPE_U16},
	{"platform_chflash.enable_A1_A10", &platform_chflash.enable_A1_A10, TYPE_U16},

	/* CHMMC VALUES */
	{"platform_chmmcsd.section_key", &platform_chmmcsd.section_key, TYPE_U32},
	{"platform_chmmcsd.valid", &platform_chmmcsd.valid, TYPE_U8},
	{"platform_chmmcsd.MMCHS_SYSCTRL_MSB", &platform_chmmcsd.MMCHS_SYSCTRL_MSB, TYPE_U16},
	{"platform_chmmcsd.MMCHS_SYSCTRL_LSB", &platform_chmmcsd.MMCHS_SYSCTRL_LSB, TYPE_U16},
	{"platform_chmmcsd.bus_width", &platform_chmmcsd.bus_width, TYPE_U32},
/* *INDENT-ON* */

};

/**
 * @brief fillup_ch - fill up the structures
 *
 * This will fill up the ch_buffer with the required
 * TOC etc.. this is the image formatting logic
 *
 * @return 0 if ch was required else returns 1
 */
static int fillup_ch(void)
{
	int toc_idx = 0;
	struct ch_toc_struct toc;
	/* CHSETTINGS IS A MUST */
	if (!platform_chsettings.valid) {
		return 1;
	}
	memset(ch_buffer, 0, sizeof(ch_buffer));
	memset(&toc, 0, sizeof(toc));
	toc.start = OFFSET_START_CHSETTINGS;
	toc.size = sizeof(platform_chsettings);
	strcpy((char *)toc.name, CH_SETTINGS);
	memcpy(ch_buffer + toc_idx, &toc, sizeof(toc));
	memcpy(ch_buffer + toc.start, &platform_chsettings,
	       sizeof(platform_chsettings));
	toc_idx += sizeof(toc);
	if (platform_chram.valid) {
		memset(&toc, 0, sizeof(toc));
		toc.start = OFFSET_START_CHRAM;
		toc.size = sizeof(platform_chram);
		strcpy((char *)toc.name, CH_RAM);
		memcpy(ch_buffer + toc_idx, &toc, sizeof(toc));
		memcpy(ch_buffer + toc.start, &platform_chram,
		       sizeof(platform_chram));
		toc_idx += sizeof(toc);
	}
	if (platform_chflash.valid) {
		memset(&toc, 0, sizeof(toc));
		toc.start = OFFSET_START_CHFLASH;
		toc.size = sizeof(platform_chflash);
		strcpy((char *)toc.name, CH_FLASH);
		memcpy(ch_buffer + toc_idx, &toc, sizeof(toc));
		memcpy(ch_buffer + toc.start, &platform_chflash,
		       sizeof(platform_chflash));
		toc_idx += sizeof(toc);
	}
	if (platform_chmmcsd.valid) {
		memset(&toc, 0, sizeof(toc));
		toc.start = OFFSET_START_CHMMCSD;
		toc.size = sizeof(platform_chmmcsd);
		strcpy((char *)toc.name, CH_MMCSD);
		memcpy(ch_buffer + toc_idx, &toc, sizeof(toc));
		memcpy(ch_buffer + toc.start, &platform_chmmcsd,
		       sizeof(platform_chflash));
		toc_idx += sizeof(toc);
	}
	/* Fill up the TOC */
	memset(&toc, 0xff, sizeof(toc));
	memcpy(ch_buffer + toc_idx, &toc, sizeof(toc));

	return 0;
}

/**
 * @brief usage - help info
 *
 * @param appname my name
 * @param extend extended help
 */
static void usage(char *appname, int extend)
{
	int i;
	struct compare_map *c = NULL;
	printf("App description:\n"
	       "---------------\n"
	       "generates a formatted image which may be used for\n"
	       "nand, onenand or mmc boot on a OMAP GP device.\n"
	       "This can also add a configuration header which\n"
	       "allows for preconfiguration of various clock,ram\n"
	       "GPMC or MMC settings prior to the image starting.\n"
	       "Syntax:\n"
	       "%s [-" CONFIG_FILE_ARG " config file] [-" LOAD_ADDRESS_ARG
	       " loadaddr] [-" IMAGE_FILE_ARG " input_file] [-?]\n"
	       "Where:\n"
	       "------\n"
	       " -" CONFIG_FILE_ARG " config_file: CH configuration file "
	       "[Default none]\n"
	       " -" LOAD_ADDRESS_ARG " loadaddress: load address for result "
	       "image [Default 0x40208800]\n"
	       " -" IMAGE_FILE_ARG " input_file: input binary to sign "
	       "[Default x-load.bin]\n"
	       " -? : provide extended help including a sample config file\n"
	       "------\n", appname);
	REVPRINT();
	LIC_PRINT();
	COLOR_PRINT(GREEN, OMAP_UBOOT_UTILS_LICENSE
		    "\nThis uses http://liblcfg.carnivore.it/ (libcfg)for parsing "
		    "configuration files\n"
		    "Copyright (c) 2007--2009  Paul Baecher\n"
		    "This program is free software; you can redistribute it and/or\n"
		    "modify  it under  the terms of the GNU General Public License\n"
		    "as published by the  Free Software Foundation; either version\n"
		    "2 of the License,  or  (at your  option) any  later version.\n");
	if (extend) {
		char line[60];
		memset(line, '-', sizeof(line));
		line[sizeof(line) - 1] = 0;
		printf("\nExtended help\n" "---------------\n"
		       "Configuration options used from config file:\n");
		printf("+%s+\n| %-49s | %5s |\n+%s+\n", line,
		       "Section.Variable_name", "size", line);
		for (i = 0;
		     i < sizeof(variable_map) / sizeof(struct compare_map);
		     i++) {
			c = &variable_map[i];
			printf("| %-49s | %5s |\n", c->name,
			       (c->type == TYPE_U32) ? "32bit" : (c->type ==
								  TYPE_U16) ?
			       "16bit" : "8bit");
		}
		printf("+%s+\n", line);
		COLOR_PRINT(RED,
			    "Please refer to 3430 TRM for further details on "
			    "the fields and bit locations\n");
		printf("\nSample Cfg file:\n" "load_addr = \"0x80000000\"\n"
		       "platform_chsettings = {\n"
		       "	section_key = \"0xC0C0C0C1\"\n"
		       "	valid = \"0x1\"\n" "	version = \"0x1\"\n"
		       "	flags = \"0x050001FD\"\n"
		       "	/*General clock settings */\n"
		       "	GEN_PRM_CLKSRC_CTRL = \"0x40\"\n"
		       "	GEN_PRM_CLKSEL = \"0x3\"\n"
		       "	GEN_CM_CLKSEL1_EMU = \"0x2030A50\"\n"
		       "	/* Clock Configuration */\n"
		       "	CLK_CM_CLKSEL_CORE = \"0x40A\"\n"
		       "	CLK_CM_CLKSEL_WKUP = \"0x14\"\n"
		       "	/* DPLL3 (Core) Settings */\n"
		       "	DPLL3_CM_CLKEN_PLL = \"0x770077\"\n"
		       "	DPLL3_CM_AUTOIDLE_PLL = \"0x0\"\n"
		       "	DPLL3_CM_CLKSEL1_PLL = \"0x8A60C00\"\n"
		       "	/* DPLL4 (Peripheral) Settings */\n"
		       "	DPLL4_CM_CLKEN_PLL = \"0x770077\"\n"
		       "	DPLL4_CM_AUTOIDLE_PLL = \"0xD80C\"\n"
		       "	DPLL4_CM_CLKSEL2_PLL = \"0x0\"\n"
		       "	DPLL4_CM_CLKSEL3_PLL = \"0x9\"\n"
		       "	/* DPLL1(MPU) Settings */\n"
		       "	DPLL1_CM_CLKEN_PLL_MPU = \"0x77\"\n"
		       "	DPLL1_CM_AUTOIDLE_PLL_MPU = \"0x0\"\n"
		       "	DPLL1_CM_CLKSEL1_PLL_MPU = \"0x10FA0C\"\n"
		       "	DPLL1_CM_CLKSEL2_PLL_MPU = \"0x1\"\n"
		       "	DPLL1_CM_CLKSTCTRL_MPU = \"0x0\"\n"
		       "}\n"
		       "\n"
		       "platform_chram = {\n"
		       "	section_key = \"0xC0C0C0C2\"\n"
		       "	valid = \"0x1\"\n"
		       "	SDRC_SYSCONFIG_LSB = \"0x0\"\n"
		       "	SDRC_CS_CFG_LSB = \"0x1\"\n"
		       "	SDRC_SHARING_LSB = \"0x100\"\n"
		       "	SDRC_DLLA_CTRL = \"0xA\"\n"
		       "	SDRC_POWER = \"0x81\"\n"
		       "	/* I use CS0 and CS1 */\n"
		       "	flags = \"0x3\"\n"
		       "	/* CS0 */\n"
		       "	mem_type_cs0 = \"0x3\"\n"
		       "	SDRC_MCFG_0 =  \"0x02D04011\"\n"
		       "	SDRC_MR_0_LSB = \"0x00000032\"\n"
		       "	SDRC_ACTIM_CTRLA_0 = \"0xBA9DC4C6\"\n"
		       "	SDRC_ACTIM_CTRLB_0 = \"0x00012522\"\n"
		       "	SDRC_RFRCTRL_0 = \"0x0004E201\"\n"
		       "	/* CS1 */\n"
		       "	mem_type_cs1 = \"0x3\"\n"
		       "	SDRC_MCFG_1 =  \"0x02D04011\"\n"
		       "	SDRC_MR_1_LSB = \"0x00000032\"\n"
		       "	SDRC_ACTIM_CTRLA_1 = \"0xBA9DC4C6\"\n"
		       "	SDRC_ACTIM_CTRLB_1 = \"0x00012522\"\n"
		       "	SDRC_RFRCTRL_1 = \"0x0004E201\"\n" "}\n");

	}
}

/* main application */
int main(int argc, char *argv[])
{
	int i;
	char *ifname, ofname[FILENAME_MAX], *cfile, ch;
	FILE *ifile, *ofile;
	unsigned long len;
	struct stat sinfo;
	unsigned int cmd_loadaddr = 0xFFFFFFFF;
	int c;
	char *appname = argv[0];

	/* Default to x-load.bin and 0x40200800 and no config file */
	ifname = "x-load.bin";
	cfile = NULL;
	loadaddr = 0x40208800;

	while ((c =
		getopt(argc, argv,
		       CONFIG_FILE_ARG ":" IMAGE_FILE_ARG ":" LOAD_ADDRESS_ARG
		       ":")) != -1)
		switch (c) {
		case CONFIG_FILE_ARG_C:
			cfile = optarg;
			break;
		case IMAGE_FILE_ARG_C:
			ifname = optarg;
			break;
		case LOAD_ADDRESS_ARG_C:
			sscanf(optarg, "%x", &cmd_loadaddr);
			break;
		case '?':
			i = 0;
			if ((optopt == IMAGE_FILE_ARG_C)
			    || (optopt == CONFIG_FILE_ARG_C)
			    || (optopt == LOAD_ADDRESS_ARG_C)) {
				APP_ERROR("Option -%c requires an argument.\n",
					  optopt);
			} else if (optopt == '?') {
				APP_ERROR("EXTENDED help\n")
				    i = 1;
			} else if (isprint(optopt)) {
				APP_ERROR("Unknown option `-%c'.\n", optopt)
			} else {
				APP_ERROR("Unknown option character `\\x%x'.\n",
					  optopt)
			}
			usage(appname, i);
			return 1;
		default:
			abort();
		}
	/*
	 * reset the struct params
	 * This is completely uneccesary in most cases, but I like being
	 * paranoid
	 */
	memset(&platform_chsettings, 0, sizeof(platform_chsettings));
	memset(&platform_chram, 0, sizeof(platform_chram));
	memset(&platform_chflash, 0, sizeof(platform_chflash));
	memset(&platform_chmmcsd, 0, sizeof(platform_chmmcsd));
	/* Parse the configuration header */
	if (cfile != NULL) {
		/* parse Configuration file */
		struct lcfg *c = lcfg_new(cfile);
		enum lcfg_status stat;
		if (!c) {
			APP_ERROR("Config file %s init failed\n", cfile)
			    usage(appname, 0);
			return -2;
		}
		stat = lcfg_parse(c);
		if (stat == lcfg_status_ok) {
			variable_map_g= variable_map;
			size_var_map = sizeof(variable_map)/sizeof(struct compare_map);
			lcfg_accept(c, compare_eventhandler, 0);
		} else {
			APP_ERROR("Config file %s: %s\n", cfile,
				  lcfg_error_get(c))
		}
		lcfg_delete(c);
		if (stat != lcfg_status_ok) {
			usage(appname, 1);
			return -3;
		}
	}

	/* Over Ride configuration file values with values from
	 * command line if provided
	 */
	if (cmd_loadaddr != 0xFFFFFFFF)
		loadaddr = cmd_loadaddr;

	/* Form the output file name. */
	strcpy(ofname, ifname);
	strcat(ofname, ".ift");

	/* Open the input file. */
	ifile = fopen(ifname, "rb");
	if (ifile == NULL) {
		APP_ERROR("Cannot open %s\n", ifname);
		usage(appname, 0);
		return -3;
	}
	/* Get file length. */
	stat(ifname, &sinfo);
	len = sinfo.st_size;

	/* Open the output file and write it. */
	ofile = fopen(ofname, "wb");
	if (ofile == NULL) {
		APP_ERROR("Cannot open %s\n", ofname);
		fclose(ifile);
		exit(0);
	}
	/* Fill up Configuration header */
	if (!fillup_ch())
		fwrite(ch_buffer, 1, sizeof(ch_buffer), ofile);

	fwrite(&len, 1, 4, ofile);
	fwrite(&loadaddr, 1, 4, ofile);
	for (i = 0; i < len; i++) {
		int z = fread(&ch, 1, 1, ifile);
		if (!z)
			APP_ERROR("no data?\n");
		fwrite(&ch, 1, 1, ofile);
	}

	fclose(ifile);
	fclose(ofile);
	return 0;
}
