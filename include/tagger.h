/**
 * @file
 * @brief Tagging utility for zImage files
 *
 * FileName: include/tagger.h
 *
 * ATAG information and related header info
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
#ifndef __TAGGER_H__
#define __TAGGER_H__

#ifndef __ASSEMBLER__

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

/* The list ends with an ATAG_NONE node. */
#define ATAG_NONE	0x00000000
struct tag_header {
	u32 size;
	u32 tag;
} __attribute__ ((__packed__));

/* The list must start with an ATAG_CORE node */
#define ATAG_CORE	0x54410001
struct tag_core {
	u32 flags;		/* bit 0 = read-only */
	u32 pagesize;
	u32 rootdev;
} __attribute__ ((__packed__));

/* it is allowed to have multiple ATAG_MEM nodes */
#define ATAG_MEM	0x54410002
struct tag_mem32 {
	u32 size;
	u32 start;		/* physical start address */
} __attribute__ ((__packed__));

/* VGA text type displays */
#define ATAG_VIDEOTEXT	0x54410003
struct tag_videotext {
	u8 x;
	u8 y;
	u16 video_page;
	u8 video_mode;
	u8 video_cols;
	u16 video_ega_bx;
	u8 video_lines;
	u8 video_isvga;
	u16 video_points;
} __attribute__ ((__packed__));

/* describes how the ramdisk will be used in kernel */
#define ATAG_RAMDISK	0x54410004
struct tag_ramdisk {
	u32 flags;		/* bit 0 = load, bit 1 = prompt */
	u32 size;		/* decompressed ramdisk size in _kilo_ bytes */
	u32 start;		/* starting block of floppy-based RAM disk image */
} __attribute__ ((__packed__));

/* describes where the compressed ramdisk image lives (virtual address) */
/*
 * this one accidentally used virtual addresses - as such,
 * its depreciated.
 */
#define ATAG_INITRD	0x54410005

/* describes where the compressed ramdisk image lives (physical address) */
#define ATAG_INITRD2	0x54420005
struct tag_initrd {
	u32 start;		/* physical start address */
	u32 size;		/* size of compressed ramdisk image in bytes */
} __attribute__ ((__packed__));

/* board serial number. "64 bits should be enough for everybody" */
#define ATAG_SERIAL	0x54410006
struct tag_serialnr {
	u32 low;
	u32 high;
} __attribute__ ((__packed__));

/* board revision */
#define ATAG_REVISION	0x54410007
struct tag_revision {
	u32 rev;
} __attribute__ ((__packed__));

/* initial values for vesafb-type framebuffers. see struct screen_info
 * in include/linux/tty.h
 */
#define ATAG_VIDEOLFB	0x54410008
struct tag_videolfb {
	u16 lfb_width;
	u16 lfb_height;
	u16 lfb_depth;
	u16 lfb_linelength;
	u32 lfb_base;
	u32 lfb_size;
	u8 red_size;
	u8 red_pos;
	u8 green_size;
	u8 green_pos;
	u8 blue_size;
	u8 blue_pos;
	u8 rsvd_size;
	u8 rsvd_pos;
} __attribute__ ((__packed__));

/* command line: \0 terminated string */
#define ATAG_CMDLINE	0x54410009
struct tag_cmdline {
	char cmdline[1];	/* this is the minimum size */
} __attribute__ ((__packed__));
struct tag {
	struct tag_header hdr;
	union {
		struct tag_core core;
		struct tag_mem32 mem;
		struct tag_videotext videotext;
		struct tag_ramdisk ramdisk;
		struct tag_initrd initrd;
		struct tag_serialnr serialnr;
		struct tag_revision revision;
		struct tag_videolfb videolfb;
		struct tag_cmdline cmdline;
	} u;
} __attribute__ ((__packed__));

#define tag_next(t)     ((struct tag *)((u32 *)(t) + (t)->hdr.size))
#define tag_size(type)  ((sizeof(struct tag_header) + sizeof(struct type)) >> 2)

#define CONFIG_NR_DRAM_BANKS 2
struct board {
	u32 mach_id;
	u32 board_rev;
	char bootargs[1024];
	char sty_file[1024];
	/* Ram */
	struct {
		u32 start;
		u32 size;	/* 0 means no existo */
	} ram[CONFIG_NR_DRAM_BANKS];
	/* Framebuffer? */
	struct {
		u32 base;
		u32 size;	/* 0 means does not exist */
	} fb;
	/* Serial */
	struct { /* both 0 means does not exist */
		u32 high;
		u32 low;
	} serial;
};

#endif	/* !__ASSEMBLER__ */

/* Magic code to be used in sty-xxx.S will be replaced by tagger */
#define MAGIC_MACHID		0xDEADBEEF
#define MAGIC_INTERIMSIZE	0xCAFEBEEF

#endif	/* __TAGGER_H__ */
