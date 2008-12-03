/**
 * @file
 * @brief Generic header for OS independent APIs to access serial port
 *
 * FileName: include/serial.h
 *
 * These APIs should be implemented by OS specific code
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
#ifndef _SERIAL_H
#define _SERIAL_H

signed char s_open(char *port);
signed char s_configure(unsigned long baud_rate, unsigned char parity,
			unsigned char stop_bits, unsigned char data);
signed char s_close(void);
signed int s_read_remaining(void);
signed int s_read(unsigned char *p_buffer, unsigned long size);
signed int s_write(unsigned char *p_buffer, unsigned long size);
signed int s_getc(void);
signed int s_putc(char x);
signed int s_flush(unsigned int *rx_left, unsigned int *tx_left);

#define SERIAL_OK 0
#define SERIAL_FAILED -1
#define SERIAL_TIMEDOUT -2

#define NOPARITY 0
#define ODDPARITY 1
#define EVENPARITY 2

#define ONE_STOP_BIT 0
#define ONEPFIVE_STOP_BIT 1
#define TWO_STOP_BIT 2

#endif				/* _SERIAL_H */
