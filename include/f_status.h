/**
 * @file
 * @brief header for lib for showing file transfer status
 *
 * FileName: include/f_status.h
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
#ifndef __LIB_INCLUDE_F_STATUS
#define __LIB_INCLUDE_F_STATUS

#define SPECIAL_PRINT 1
#define NORMAL_PRINT  0
/**
 * @brief f_status_init initialize the status structures
 *
 * @param total_size - total size of the file
 * @param special  - display special display -> for apps interfacing with ui stuff
 */
void f_status_init(signed long total_size, unsigned char special);

/**
 * @brief f_status_show show current status -> to be called by apps when they desire to display
 *
 * @param cur_size - current size
 *
 * @return 0 - ok ,1 - not ok.. size was too large
 */
int f_status_show(signed long cur_size);
#endif				/* __LIB_INCLUDE_F_STATUS */
