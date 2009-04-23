/**
 * @file
 * @brief stuff that belong no where, common header for all files
 *
 * FileName: include/rev.h
 *
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
#ifndef __LIB_COMMON_H
#define __LIB_COMMON_H

#ifndef DISABLE_COLOR
/* Colored prints enabled by default */
#ifndef __WIN32__

#define _COLOR_(m)	"\x1b[" # m
#define RED		_COLOR_(1;31m)
#define GREEN		_COLOR_(1;32m)
#define BLUE		_COLOR_(1;34m)
#define RESET		_COLOR_(0m)

#define COLOR_PRINT(COLOR,ARGS...)\
{\
	printf(COLOR ARGS);\
	printf(RESET);\
}

#else
#include <windows.h>
#include <stdio.h>

#define RED		FOREGROUND_RED
#define GREEN		FOREGROUND_GREEN
#define BLUE		FOREGROUND_BLUE
#define COLOR_PRINT(COLOR,ARGS...)\
{\
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);\
	CONSOLE_SCREEN_BUFFER_INFO pConsoleScreenBufferInfo;\
	GetConsoleScreenBufferInfo(hStdout,&pConsoleScreenBufferInfo);\
	SetConsoleTextAttribute(hStdout, COLOR | FOREGROUND_INTENSITY);\
	printf(ARGS);\
	SetConsoleTextAttribute(hStdout, pConsoleScreenBufferInfo.wAttributes);\
}
#endif	/* __WIN32__ */

#define APP_ERROR(ARGS...) {COLOR_PRINT(RED,ARGS)}

#else	/* #ifndef DISABLE_COLOR */

#define COLOR_PRINT(COLOR,ARGS...) printf(ARGS);
#define APP_ERROR(ARGS...) {fprintf(stderr,ARGS);}

#endif	/* #ifndef DISABLE_COLOR */

#endif /* __LIB_COMMON_H */
