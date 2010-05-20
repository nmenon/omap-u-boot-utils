/**
 * @file
 * @brief lib for showing file transfer status
 *
 * FileName: lib/f_status.c
 *
 * File transfer status apis
 *
 */
/*
 * (C) Copyright 2008
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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <f_status.h>

#define BACK_SPACE_CHAR '\b'
/* max buffer size */
#define PRINT_SIZE 100

/* Store my old buffer */
static char print_buff[PRINT_SIZE];
/* backspace characters */
static char back_buff[PRINT_SIZE];
/* total size of the file */
static signed long tsize;
/* Special prints of the order of percent/cur/total size */
static unsigned char s_special;

/**
 * @brief f_status_init initialize the status structures
 *
 * @param total_size - total size of the file
 * @param special  - display s_special display -> for
 *	apps interfacing with ui stuff
 */
void f_status_init(signed long total_size, unsigned char special)
{
	tsize = total_size;
	memset(back_buff, BACK_SPACE_CHAR, sizeof(back_buff));
	print_buff[0] = '\0';
	s_special = special;
}

/**
 * @brief f_status_show show current status -> to be called by apps when they
 * desire to display
 *
 * @param cur_size - current size
 *
 * @return 0 - ok ,1 - not ok.. size was too large
 */
int f_status_show(signed long cur_size)
{
	int off;
	float percent;
	if (cur_size > tsize) {
		return -1;
	}
	percent = cur_size;
	percent /= tsize;
	percent *= 100;
	if (s_special) {
		printf("%.03f/%ld/%ld", percent, cur_size, tsize);
		return 0;
	}
	/* get length of old buffer */
	off = strlen(print_buff);
	/* should i print backspaces? */
	if (off) {
		back_buff[off] = '\0';
		printf("%s", back_buff);
		/* restore backspace */
		back_buff[off] = BACK_SPACE_CHAR;
	}
	sprintf(print_buff,
		"Downloading file: %.03f%% completed (%ld/%ld bytes)", percent,
		cur_size, tsize);
	printf("%s", print_buff);
	fflush(stdout);
	return 0;
}
