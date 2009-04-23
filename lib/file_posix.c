/**
 * @file
 * @brief file io implementation for Linux and POSIX compliant OSes
 *
 * FileName: lib/file_linux.c
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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "file.h"
#include <common.h>

/************* CONSTS ***************/
#define F_ERROR(ARGS...)  APP_ERROR(ARGS);perror("File:Error")

#ifdef DEBUG
#define F_INFO(ARGS...) fprintf(stdout,ARGS)
#else
#define F_INFO(ARGS...)
#endif

/************* VARS   ***************/
static FILE *file;

/**************** EXPOSED FUNCTIONS  ****************/

/**
 * @brief f_size - returns the size of the file
 *
 * @param f_name  - name of the file including path
 *
 * @return success/fail
 */
signed long f_size(const char *f_name)
{
	struct stat f_stat;
	if (stat(f_name, &f_stat)) {
		F_ERROR("Could not stat %s\n", f_name);
		return FILE_ERROR;
	}
	F_INFO("File size is %d\n", (unsigned int)f_stat.st_size);
	return f_stat.st_size;
}

/**
 * @brief f_open - open the file
 *
 * @param f_name file name
 *
 * @return success/fail
 */
signed char f_open(const char *f_name)
{
	if (file != NULL) {
		F_ERROR("file %s already open\n", f_name);
		return FILE_ERROR;
	}
	file = fopen(f_name, "r");
	if (file == NULL) {
		F_ERROR("could not open file %s\n", f_name);
		return FILE_ERROR;
	}
	F_INFO("file %s opened\n", (char *)f_name);
	return FILE_OK;
}

/**
 * @brief f_close - close the file
 *
 * @return pass/fail
 */
signed char f_close(void)
{
	if (file == NULL) {
		F_ERROR("file is not open\n");
		return FILE_ERROR;
	}
	if (fclose(file)) {
		F_ERROR("could not close file!\n");
		file = NULL;
		return FILE_ERROR;
	}
	file = NULL;
	F_INFO("file closed\n");
	return FILE_OK;
}

/**
 * @brief f_read - read from file
 *
 * @param buffer buffer
 * @param read_size size to read
 *
 * @return num bytes read
 */
signed int f_read(unsigned char *buffer, unsigned int read_size)
{
	int ret;
	if (file == NULL) {
		F_ERROR("file is not open\n");
		return FILE_ERROR;
	}
	ret = fread(buffer, 1, read_size, file);
	F_INFO("read operation returned %d\n", ret);
	return ret;
}
