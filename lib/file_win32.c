/**
 * @file
 * @brief Windows API based implementation of include/file.h
 *
 * FileName: lib/file_win32.c
 *
 * Support File operations using Win32 APIs. Compiled with MingW
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

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include "file.h"
#include <common.h>

/***** TYPES(WINDOWS SPECIFIC) *******/
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef NULL
#define NULL    ((void *)0)
#endif

/************* CONSTS ***************/
#define F_ERROR(ARGS...) APP_ERROR(ARGS)
#define F_INFO(ARGS...)
//#define F_INFO(ARGS...) fprintf(stdout,ARGS)

/************* VARS   ***************/
static HANDLE Fhandle = INVALID_HANDLE_VALUE;

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
	HANDLE MF;
	if (Fhandle != INVALID_HANDLE_VALUE) {
		F_ERROR("File already opend\n");
		return FILE_ERROR;
	}
	MF = CreateFile(f_name, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
	if (MF != INVALID_HANDLE_VALUE) {
		DWORD High, Low;
		Low = GetFileSize(MF, &High);
		if ((High == 0) && (Low != 0xFFFFFFFF)) {
			F_INFO("%s: Size : High = %X, Low = %X\n", f_name,
			       (unsigned int)High, (unsigned int)Low);
			CloseHandle(MF);
			return Low;
		}
		F_INFO("%s High %d Low %d - cannot handle\n", f_name,
		       (unsigned int)High, (unsigned int)Low);
		CloseHandle(MF);
	} else {
		F_ERROR("%s: Could not open file: Error=%i\n", f_name,
			(int)GetLastError());

	}
	return FILE_ERROR;
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
	if (Fhandle != INVALID_HANDLE_VALUE) {
		F_ERROR("File already opend\n");
		return FILE_ERROR;
	}
	Fhandle = CreateFile(f_name, GENERIC_READ, FILE_SHARE_READ, NULL,
			     OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
	if (Fhandle != INVALID_HANDLE_VALUE) {
		F_INFO("%s: opened fine\n", f_name);
		return FILE_OK;
	} else {
		F_ERROR("%s: Could not open file: Error=%i\n", f_name,
			(int)GetLastError());
	}
	return FILE_ERROR;
}

/**
 * @brief f_close - close the file
 *
 * @return pass/fail
 */
signed char f_close(void)
{
	if (Fhandle == INVALID_HANDLE_VALUE) {
		F_ERROR("File not opened to close!\n");
		return FILE_ERROR;
	}
	CloseHandle(Fhandle);
	F_INFO("File closed fine\n");
	Fhandle = INVALID_HANDLE_VALUE;
	return FILE_OK;
}

/**
 * @brief f_read - read from file
 *
 * @param buffer buffer
 * @param read_size size to read
 *
 * @return pass/fail
 */
signed int f_read(unsigned char *buffer, unsigned int read_size)
{
	unsigned long actual_read = 0, total_read = 0, to_read = read_size;
	if (Fhandle == INVALID_HANDLE_VALUE) {
		F_ERROR("File not opened to read!\n");
		return FILE_ERROR;
	}
	while (total_read < read_size) {
		BOOL err =
		    ReadFile(Fhandle, buffer, to_read, &actual_read, NULL);
		if (err != TRUE) {
			F_ERROR("Read operation failed! error = %i,to_read=%d\n"
				"read_size=%d,total_read=%d\n",
				(int)GetLastError(), (unsigned int)to_read,
				(unsigned int)read_size,
				(unsigned int)total_read);
			return FILE_ERROR;
		}
		if (actual_read == 0) {
			F_ERROR("Should not have read just 0 bytes!!!\n");
			break;
		}
		to_read -= actual_read;
		total_read += actual_read;
		buffer += actual_read;
		F_INFO("Read %d, rem %d total_read %d!\n",
		       (unsigned int)actual_read, (unsigned int)to_read,
		       (unsigned int)total_read);

	}
	F_INFO("All read!\n");

	return total_read;
}
