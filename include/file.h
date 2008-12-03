/**
 * @file
 * @brief Header for Generic I/O definition.
 *
 * FileName: include/file.h
 *
 * The OS specific implementation should implement these APIs.
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
#ifndef __FILE__H_
#define __FILE__H_
/** File Operation failed */
#define FILE_ERROR -1
/** File Operation OK */
#define FILE_OK 0

signed long f_size(const char *f_name);
signed char f_open(const char *f_name);
signed char f_close(void);
signed int f_read(unsigned char *buffer, unsigned int read_size);

#endif				/* __FILE__H_ */
