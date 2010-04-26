/**
 * @file
 * @brief Tagging utility for zImage files
 *
 * FileName: src/tagger.c
 *
 * Generate ATAG information and add external data to
 * a zImage to allow the image to boot out of the box
 */
/*
 * tagger:
 * (C) Copyright 2010
 * Texas Instruments, <www.ti.com>
 *
 * Nishanth Menon <nm@ti.com>
 * Original code from: u-boot/include/asm-arm/setup.h
 *  linux/include/asm/setup.h
 *
 *  Copyright (C) 1997-1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Structure passed to kernel to tell it about the
 * hardware it's running on.  See linux/Documentation/arm/Setup
 * for more info.
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
#include "tagger.h"
#include "rev.h"
#include "compare.h"

#define CONFIG_FILE_ARG "c"
#define CONFIG_FILE_ARG_C 'c'
#define IMAGE_FILE_ARG "f"
#define IMAGE_FILE_ARG_C 'f'

static void setup_start_tag(struct tag **_params)
{
	struct tag *params = *_params;
	params->hdr.tag = ATAG_CORE;
	params->hdr.size = tag_size(tag_core);
	params->u.core.flags = 0;
	params->u.core.pagesize = 0;
	params->u.core.rootdev = 0;
	params = tag_next(params);
	*_params = params;
}

static void setup_memory_tags(struct tag **_params, struct board *b)
{
	int i;
	struct tag *params = *_params;
	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++) {
		if (!b->ram[i].size)
			continue;
		params->hdr.tag = ATAG_MEM;
		params->hdr.size = tag_size(tag_mem32);
		params->u.mem.start = b->ram[i].start;
		params->u.mem.size = b->ram[i].size;
		params = tag_next(params);
	}
	*_params = params;
}

static void setup_videolfb_tag(struct tag **_params, struct board *b)
{
	struct tag *params = *_params;

	if (!b->fb.size)
		return;

	/* An ATAG_VIDEOLFB node tells the kernel where and how large
	 * the framebuffer for video was allocated (among other things).
	 * Note that a _physical_ address is passed !
	 *
	 * We only use it to pass the address and size, the other entries
	 * in the tag_videolfb are not of interest.
	 */
	params->hdr.tag = ATAG_VIDEOLFB;
	params->hdr.size = tag_size(tag_videolfb);
	params->u.videolfb.lfb_base = b->fb.base;

	/* Fb size is calculated according to parameters for our panel
	 */
	params->u.videolfb.lfb_size = b->fb.size;
	params = tag_next(params);
	*_params = params;
}

static void setup_serial_tag(struct tag **_params, struct board *b)
{
	struct tag *params = *_params;
	if (!(b->serial.high + b->serial.low))
		return;
	params->hdr.tag = ATAG_SERIAL;
	params->hdr.size = tag_size(tag_serialnr);
	params->u.serialnr.low = b->serial.low;
	params->u.serialnr.high = b->serial.high;
	params = tag_next(params);
	*_params = params;
}

static void setup_revision_tag(struct tag **_params, struct board *b)
{
	struct tag *params = *_params;
	if (!b->board_rev)
		return;
	params->hdr.tag = ATAG_REVISION;
	params->hdr.size = tag_size(tag_revision);
	params->u.revision.rev = b->board_rev;
	params = tag_next(params);
	*_params = params;
}

#if 0
static void setup_initrd_tag(struct tag **_params, u32 initrd_start,
			     u32 initrd_end)
{
	struct tag *params = *_params;

	/* an ATAG_INITRD node tells the kernel where the compressed
	 * ramdisk can be found. ATAG_RDIMG is a better name, actually.
	 */
	params->hdr.tag = ATAG_INITRD2;
	params->hdr.size = tag_size(tag_initrd);
	params->u.initrd.start = initrd_start;
	params->u.initrd.size = initrd_end - initrd_start;
	params = tag_next(params);
	*_params = params;
}
#endif

static void setup_commandline_tag(struct tag **_params, struct board *b)
{
	char *p;
	struct tag *params = *_params;
	char *commandline = b->bootargs;
	if (!commandline)
		return;

	/* eat leading white space */
	for (p = commandline; *p == ' '; p++) ;

	/* skip non-existent command lines so the kernel will still
	 * use its default command line.
	 */
	if (*p == '\0')
		return;
	params->hdr.tag = ATAG_CMDLINE;
	params->hdr.size = (sizeof(struct tag_header) + strlen(p) + 4) >> 2;
	strcpy(params->u.cmdline.cmdline, p);
	params = tag_next(params);
	*_params = params;
}

static void setup_end_tag(struct tag **_params)
{
	struct tag *params = *_params;
	params->hdr.tag = ATAG_NONE;
	params->hdr.size = 0;
	params = ((struct tag *)(((u8 *)(params)) +sizeof(struct tag_header)));
	*_params = params;
}

static int generate_tag(unsigned char *buffer, struct board *b)
{
	struct tag *param = (struct tag *)buffer;
	setup_start_tag(&param);
	setup_serial_tag(&param, b);
	setup_revision_tag(&param, b);
	setup_memory_tags(&param, b);
	setup_commandline_tag(&param, b);
#if 0
	setup_initrd_tag(b, images->rd_start, images->rd_end);
#endif
	setup_videolfb_tag(&param, b);
	setup_end_tag(&param);
	return (u32) param - (u32) buffer;
}

static struct board myboard;
static struct compare_map variable_map[] = {
	/* *INDENT-OFF* */
	{"board.mach_id", &myboard.mach_id, TYPE_U32},
	{"board.board_rev",&myboard.board_rev, TYPE_U32},
	{"board.bootargs", myboard.bootargs,TYPE_S},
	{"board.ram0.start", &myboard.ram[0].start, TYPE_U32},
	{"board.ram0.size", &myboard.ram[0].size, TYPE_U32},
	{"board.ram1.start", &myboard.ram[1].start, TYPE_U32},
	{"board.ram1.size", &myboard.ram[1].size, TYPE_U32},
	{"board.fb.base", &myboard.fb.base, TYPE_U32},
	{"board.fb.size", &myboard.fb.size, TYPE_U32},
	{"board.serial.high", &myboard.serial.high, TYPE_U32},
	{"board.serial.low", &myboard.serial.low, TYPE_U32},
	{"board.sty_file", myboard.sty_file, TYPE_S},
	/* *INDENT-ON* */
};

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
	       "%s [-" CONFIG_FILE_ARG " config file] [-"
	       IMAGE_FILE_ARG " input_file] [-?]\n"
	       "Where:\n"
	       "------\n"
	       " -" CONFIG_FILE_ARG " config_file: configuration file"
	       "[Default none]\n"
	       " -" IMAGE_FILE_ARG " input_file: input binary to sign "
	       "[Default zImage]\n"
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
		char line[62];
		memset(line, '-', sizeof(line));
		line[sizeof(line) - 1] = 0;
		printf("\nExtended help\n" "---------------\n"
		       "Configuration options used from config file:\n");
		printf("+%s+\n| %-49s | %7s |\n+%s+\n", line,
		       "Section.Variable_name", "size", line);
		for (i = 0;
		     i < sizeof(variable_map) / sizeof(struct compare_map);
		     i++) {
			c = &variable_map[i];
			printf("| %-49s | %7s |\n", c->name,
			       (c->type == TYPE_U32) ? "32bit" : (c->type ==
								  TYPE_U16) ?
			       "16bit" : (c->type == TYPE_U8) ? "8bit": "String");
		}
		printf("+%s+\n", line);
	}
}

int main(int argc, char **argv)
{
	char *ifname, ofname[FILENAME_MAX], *cfile, ch;
	FILE *ifile, *ofile, *sty_file;
	unsigned long len, tlen;
	struct stat sinfo;
	int c;
	char *appname = argv[0];
	unsigned char buffer[1024];
	int i;

	/* Default to zImage */
	ifname = "zImage";
	cfile = NULL;

	while ((c =
		getopt(argc, argv,
		       CONFIG_FILE_ARG ":" IMAGE_FILE_ARG ":")) != -1)
		switch (c) {
		case CONFIG_FILE_ARG_C:
			cfile = optarg;
			break;
		case IMAGE_FILE_ARG_C:
			ifname = optarg;
			break;
		case '?':
			i = 0;
			if ((optopt == IMAGE_FILE_ARG_C)
			    || (optopt == CONFIG_FILE_ARG_C)) {
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
	memset(&myboard, 0, sizeof(struct board));
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
			variable_map_g = variable_map;
			size_var_map =
			    sizeof(variable_map) / sizeof(struct compare_map);
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

	/* Form the output file name. */
	strcpy(ofname, ifname);
	strcat(ofname, ".tag");

	sty_file = fopen(myboard.sty_file, "rb");
	if (sty_file == NULL) {
		APP_ERROR("Cannot open Styfile %s in cfile %s\n",
			  myboard.sty_file, cfile);
		usage(appname, 0);
		return -3;
	}
	/* Open the input file. */
	ifile = fopen(ifname, "rb");
	if (ifile == NULL) {
		APP_ERROR("Cannot open %s\n", ifname);
		usage(appname, 0);
		return -3;
	}
	/* Open the output file and write it. */
	ofile = fopen(ofname, "wb");
	if (ofile == NULL) {
		APP_ERROR("Cannot open %s\n", ofname);
		fclose(ifile);
		exit(0);
	}

	/* lets first generate the tag */
	tlen = generate_tag(buffer, &myboard);
	/* 1. Fill up the sty */
	stat(myboard.sty_file, &sinfo);
	len = sinfo.st_size;
	for (i = 0; i < (len/4); i++) {
		u32 sty_val = 0;
		int z = fread(&sty_val, 4, 1, sty_file);
		if (!z)
			APP_ERROR("Styfile:%s no data?%d 0x%04X\n",
				myboard.sty_file, z, sty_val)
		switch (sty_val) {
		case MAGIC_MACHID:
			sty_val = myboard.mach_id;
			break;
		case MAGIC_INTERIMSIZE:
			sty_val = tlen;
			break;
		}
		fwrite(&sty_val, 4, 1, ofile);
	}
	fclose(sty_file);

	/* 2: Fill up the ATAG */
	fwrite(buffer, 1, tlen, ofile);

	/* 3. Fill in the zImage */
	stat(ifname, &sinfo);
	len = sinfo.st_size;
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
