/**
 * @file
 * @brief Win32 based serial port operations.
 *
 * FileName: lib/serial_win32.c
 *
 * I am heavily indebted to MSDN for guiding me on this.
 * http://msdn.microsoft.com/
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
#include "serial.h"
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
#define S_ERROR(ARGS...) APP_ERROR(ARGS)
#define S_INFO(ARGS...)
#ifdef DEBUG
#define S_INFO(ARGS...) fprintf(stdout,ARGS)
#define S_DEBUG(FORMAT,ARGS...) printf ("%s[%d]:" FORMAT "\n",\
		__FUNCTION__,__LINE__,ARGS)
#else
#define S_DEBUG(ARGS...)
#endif

#define BUF_SIZE (8096*2)
#define RX_HW_BUF_SIZE BUF_SIZE
#define TX_HW_BUF_SIZE BUF_SIZE
#define R_EVENT_MASK EV_RXCHAR
#define T_EVENT_MASK EV_TXEMPTY

/************* VARS   ***************/
static unsigned char port[30];
static HANDLE h_serial;
static OVERLAPPED read_overlapped;
static OVERLAPPED write_overlapped;

/**************** HELPERS ****************/
static signed int reset_comm_error(unsigned int *rx_bytes,
				   unsigned int *tx_bytes);

/**************** EXPOSED FUNCTIONS  ****************/
/**
 * @brief s_open - open a serial port
 *
 * @param t_port -port number
 *
 * @return success/fail
 */
signed char s_open(char *t_port)
{
	char com_port[20] = "\\\\.\\";
	S_DEBUG("%s", t_port);
	if (strlen(port) >= 30) {
		S_ERROR("too large portname %s\n", t_port);
		return SERIAL_FAILED;
	}
	strcat(com_port, t_port);

	h_serial =
	    CreateFile(com_port, GENERIC_READ | GENERIC_WRITE, 0, NULL,
		       OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
	if (h_serial == INVALID_HANDLE_VALUE) {
		S_ERROR("Could not open port %s %s\n", port, com_port);
		return SERIAL_FAILED;
	}
	strcpy(port, t_port);

	memset(&read_overlapped, 0, sizeof(read_overlapped));
	read_overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	memset(&write_overlapped, 0, sizeof(write_overlapped));
	write_overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	S_INFO("port %s opened fine!\n", port);
	return SERIAL_OK;
}

/**
 * @brief s_configure - configure the serial port
 *
 * @param s_baud_rate -baudrate
 * @param s_parity -parity
 * @param s_stop_bits -num stop bits
 * @param s_data_bits -data bits
 *
 * @return -success/failure
 */
signed char s_configure(unsigned long s_baud_rate, unsigned char s_parity,
			unsigned char s_stop_bits, unsigned char s_data_bits)
{
	COMSTAT com_stat;
	DWORD errors;
	COMMTIMEOUTS timeouts;
	DCB dcb = {
		0
	};
	dcb.DCBlength = sizeof(DCB);

	S_DEBUG("%d %d %d %d", (unsigned int)s_baud_rate, s_parity, s_stop_bits,
		s_data_bits);
	/* Get current configuration of serial port. */
	if (!GetCommState(h_serial, &dcb)) {
		S_ERROR("Could not get state of port %s\n", port);
		return SERIAL_FAILED;
	}

	/* Setup new params
	 * See http://msdn.microsoft.com/en-us/library/aa363214(VS.85).aspx
	 */
	dcb.BaudRate = s_baud_rate;
	dcb.fBinary = TRUE;
	dcb.fParity = (s_parity == NOPARITY) ? FALSE : TRUE;
	dcb.fOutxCtsFlow = FALSE;
	dcb.fOutxDsrFlow = FALSE;
	dcb.fDtrControl = DTR_CONTROL_DISABLE;
	dcb.fDsrSensitivity = FALSE;
	dcb.fTXContinueOnXoff = FALSE;
	dcb.fOutX = FALSE;
	dcb.fInX = FALSE;
	dcb.fErrorChar = FALSE;
	dcb.fNull = FALSE;
	dcb.fRtsControl = RTS_CONTROL_DISABLE;
	dcb.fAbortOnError = FALSE;
	dcb.Parity = s_parity;
	dcb.ByteSize = s_data_bits;
	dcb.StopBits = s_stop_bits;
	if (!SetCommState(h_serial, &dcb)) {
		S_ERROR
		    ("Could not set state of port %s baudrate=%d "
		     "s_parity=%d stop=%d s_data_bits=%d\n", port,
		     (unsigned int)s_baud_rate, s_parity, s_stop_bits,
		     s_data_bits);
		return SERIAL_FAILED;
	}
	if (!SetupComm(h_serial, RX_HW_BUF_SIZE, TX_HW_BUF_SIZE)) {
		S_ERROR("Buffer setup failed on port %s\n", port);
		return SERIAL_FAILED;
	}

	/* Setup Timeout */
	timeouts.ReadIntervalTimeout = MAXDWORD;
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.ReadTotalTimeoutConstant = 0;
	timeouts.WriteTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutConstant = 0;
	if (!SetCommTimeouts(h_serial, &timeouts)) {
		S_ERROR("Timeout setting failed\n");
		return SERIAL_FAILED;
	}
	ClearCommError(h_serial, &errors, &com_stat);
	PurgeComm(h_serial,
		  PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR |
		  PURGE_RXCLEAR);

	S_INFO("Configured fine\n");
	return SERIAL_OK;
}

signed int s_flush(unsigned int *rx_bytes, unsigned int *tx_bytes)
{
	COMSTAT com_stat;
	unsigned long errors;
	S_DEBUG("%p %p\n", rx_bytes, tx_bytes);
	ClearCommError(h_serial, &errors, &com_stat);
	CancelIo(h_serial);
	PurgeComm(h_serial,
		  PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR |
		  PURGE_RXCLEAR);
	if (rx_bytes)
		*rx_bytes = com_stat.cbInQue;
	if (tx_bytes)
		*tx_bytes = com_stat.cbOutQue;
	return 0;
}

/**
 * @brief s_close - close the serial port
 *
 * @return sucess/fail
 */
signed char s_close(void)
{
	unsigned long ret;

	S_DEBUG("%s ", "enter");
	if (h_serial != INVALID_HANDLE_VALUE) {
		SetCommMask(h_serial, R_EVENT_MASK | T_EVENT_MASK);
		CancelIo(h_serial);
		PurgeComm(h_serial,
			  PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR |
			  PURGE_RXCLEAR);
		ret = CloseHandle(h_serial);
		if (ret == 0) {
			S_ERROR("Opps.. failed to close port %s\n", port);
		}
		h_serial = INVALID_HANDLE_VALUE;
	}
	if (read_overlapped.hEvent != NULL)
		CloseHandle(read_overlapped.hEvent);
	if (write_overlapped.hEvent != NULL)
		CloseHandle(write_overlapped.hEvent);
	S_INFO("Closed fine\n");
	return SERIAL_OK;
}

/**
 * @brief s_read - serial port read
 *
 * @param p_buffer buffer
 * @param size buffer length
 *
 * @return success/fail
 */
signed int s_read(unsigned char *p_buffer, unsigned long size)
{
	signed int ret = 0;
	signed int err = 0;
	unsigned long size_to_read = size;
	unsigned long tot_read = 0;
	/*unsigned long event_mask = R_EVENT_MASK; */
	S_DEBUG("%p:%d", p_buffer, (unsigned int)size);

	if (h_serial == INVALID_HANDLE_VALUE) {
		S_ERROR("Not opened\n");
		return SERIAL_FAILED;
	}
	if (size == 0) {
		S_INFO("0 byte read..ok\n");
		return SERIAL_OK;
	}
	if (size >= RX_HW_BUF_SIZE) {
		S_ERROR("More than read buffer\n");
		return SERIAL_FAILED;
	}
	/* Read mask is set already in transmit */
	do {
		ret = ReadFile(h_serial, p_buffer, size_to_read, &size,
			       &read_overlapped);
		if (size) {
			S_INFO("read %d bytes\n", size);
			size_to_read -= size;
			p_buffer += size;
			tot_read += size;
		}
	} while (size_to_read > 0);
	ret = tot_read;
	S_INFO("size = %d\n", (unsigned int)size);
	if (!size && (GetLastError() == ERROR_IO_PENDING)) {
		err = GetLastError();
		S_ERROR("Read[%s]: Readfile error: %i\n", port, err);
		reset_comm_error(NULL, NULL);
		PurgeComm(h_serial,
			  PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR |
			  PURGE_RXCLEAR);
	}

	S_INFO("**** Read[%s]Rem: %d Got: %d, Timeout:%d\n", port,
	       (unsigned int)size_to_read, ret, timeout);
	S_DEBUG("%d", ret);
	return ret;
}

/**
 * @brief s_write - write to serial port
 *
 * @param p_buffer - buffer pointer
 * @param size -size of buffer
 *
 * @return success/failure
 */
signed int s_write(unsigned char *p_buffer, unsigned long size)
{
	signed int ret = 0;
	signed int err = 0;
	unsigned long sizeToWrite = size;
	unsigned long tot_write = 0;
	unsigned long event_mask = T_EVENT_MASK;

	S_DEBUG("%p %d", p_buffer, (unsigned int)size);
	if (h_serial == INVALID_HANDLE_VALUE) {
		S_ERROR("Not opened\n");
		return SERIAL_FAILED;
	}
	if (size == 0) {
		S_INFO("0 byte write..ok\n");
		return SERIAL_OK;
	}
	if (size >= TX_HW_BUF_SIZE) {
		S_ERROR("More than normal buffer size\n");
		return SERIAL_FAILED;
	}

	ResetEvent(write_overlapped.hEvent);

	SetCommMask(h_serial, T_EVENT_MASK);
	WaitCommEvent(h_serial, &event_mask, &write_overlapped);
	do {
		ret = WriteFile(h_serial, p_buffer, sizeToWrite, &size,
				&write_overlapped);
		if (!ret)
			WaitForSingleObject(write_overlapped.hEvent, INFINITE);
		S_INFO("write %d bytes\n", size);
		size = sizeToWrite;

		sizeToWrite -= size;
		p_buffer += size;
		tot_write += size;
	} while (sizeToWrite > 0);
	ret = tot_write;

	if (!size && (GetLastError() == ERROR_IO_PENDING)) {
		err = GetLastError();
		S_ERROR("Write[%s]: Writefile error: %i\n", port, err);
		reset_comm_error(NULL, NULL);
		PurgeComm(h_serial,
			  PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR |
			  PURGE_RXCLEAR);
	}
	SetCommMask(h_serial, R_EVENT_MASK);

	S_INFO("**** Write[%s]Rem: %d Got: %d, Timeout:%d\n", port,
	       (unsigned int)sizeToWrite, ret, timeout);
	S_DEBUG("%d", ret);
	return ret;
}

/**
 * @brief s_read_remaining - read remaining bytes
 *
 * @return num bytes remaining OR error.
 */
signed int s_read_remaining(void)
{
	unsigned int rx_bytes;
	unsigned int tx_bytes;
	signed int ret = 0;
	S_DEBUG("%s", "enter");
	ret = reset_comm_error(&rx_bytes, &tx_bytes);
	S_DEBUG("%d %d", ret, rx_bytes);
	return (ret < 0) ? ret : rx_bytes;
}

/**
 * @brief s_getc - get a single character
 *
 * @return character read or error
 */
signed int s_getc(void)
{
	char x = 0;
	int ret = 0;
	ret = s_read(&x, 1);
	if (ret < 0) {
		S_ERROR("getc failed-%d\n", ret);
		return ret;
	}
	S_DEBUG("[%c]%x", x, x);
	return x;
}

/**
 * @brief s_putc put a single character
 *
 * @param x -character to write to serial port
 *
 * @return -written character or error
 */
signed int s_putc(char x)
{
	int ret = 0;
	S_DEBUG("[%c] 0x%02x", x, (unsigned char)x);
	ret = s_write(&x, 1);
	if (ret < 0) {
		S_ERROR("putc failed-%d\n", ret);
		return ret;
	}
	return x;
}

/**************** HELPERS ****************/

/**
 * @brief reset_comm_error - Reset comport errors
 *
 * @param rx_bytes - read the bytes left
 * @param tx_bytes  - read the bytes left
 *
 * @return success/failure
 */
static signed int reset_comm_error(unsigned int *rx_bytes,
				   unsigned int *tx_bytes)
{
	char errStr[20] = { 0 };
	COMSTAT com_stat;
	unsigned long errors;
	ClearCommError(h_serial, &errors, &com_stat);
	/* If we see errors, then print the same */
	if (errors & CE_DNS)
		strcat(errStr, "DNS ");
	if (errors & CE_IOE)
		strcat(errStr, "IOE ");
	if (errors & CE_OOP)
		strcat(errStr, "OOP ");
	if (errors & CE_PTO)
		strcat(errStr, "PTO ");
	if (errors & CE_MODE)
		strcat(errStr, "MODE ");
	if (errors & CE_BREAK)
		strcat(errStr, "BREAK ");
	if (errors & CE_FRAME)
		strcat(errStr, "FRAME ");
	if (errors & CE_RXOVER)
		strcat(errStr, "RXOVER ");
	if (errors & CE_TXFULL)
		strcat(errStr, "TXFULL ");
	if (errors & CE_OVERRUN)
		strcat(errStr, "OVERRUN ");
	if (errors & CE_RXPARITY)
		strcat(errStr, "RXPARITY ");
	S_ERROR
	    ("Reset:Port[%s] error = 0x%X errorstring = %s "
	     "RX_PEND=%d TX_PEND=%d.\n",
	     port, (unsigned int)errors, errStr,
	     (unsigned int)com_stat.cbInQue, (unsigned int)com_stat.cbOutQue);
	if (rx_bytes)
		*rx_bytes = com_stat.cbInQue;
	if (tx_bytes)
		*tx_bytes = com_stat.cbOutQue;
	return errors;
}
