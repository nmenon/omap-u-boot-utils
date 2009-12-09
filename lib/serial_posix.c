/**
 * @file
 * @brief Serial port operations wrapper for Linux/posix compliant OSs
 *
 * FileName: lib/serial_linux.c
 *
 * This implements the APIs in include/serial.h for linux and posix
 * compliant OSes. The code here is heavily indebted to:
 * @li http://tldp.org/HOWTO/Serial-Programming-HOWTO/x115.html#AEN129
 * @li http://www.comptechdoc.org/os/linux/programming/c/linux_pgcserial.html
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <serial.h>
#include <common.h>

/************* CONSTS ***************/
#define S_ERROR(ARGS...) APP_ERROR(ARGS); perror("Serial: System Error")
#ifdef DEBUG
#define S_INFO(FORMAT,ARGS...) printf ("%s[%d]:" FORMAT "\n",\
				__FUNCTION__,__LINE__,ARGS)
#else
#define S_INFO(ARGS...)
#endif

/* Setup delay time */
#define VTIME_SET 5

/************* VARS   ***************/
static unsigned char port[30];
static int fd;
static struct termios oldtio, newtio;

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
	int x;
	char cmd[200];
	if (fd) {
		S_ERROR("Port is already open\n");
		return SERIAL_FAILED;
	}
	/* Check if serial port is used by other process */

	/* NOTE: it is a bad idea to use lsof, but the alternative
	 * is to write large amount of code..
	 */
	sprintf(cmd, "lsof |grep %s 2>&1 >/dev/null", t_port);
	x = system(cmd);
	if (x == 0) {
		S_ERROR("device %s being used already?\n", t_port);
		sprintf(cmd, "lsof |grep %s", t_port);
		x = system(cmd);
		return SERIAL_FAILED;
	}
	fd = open(t_port, O_RDWR | O_NOCTTY);
	if (fd < 0) {
		S_ERROR("failed to open %s\n", t_port);
		perror(t_port);
		return SERIAL_FAILED;
	}
	strncpy((char *)port, t_port, 30);
	S_INFO("Serial port %s opend fine\n", port);
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
	int ret;

	if (!fd) {
		S_ERROR("terminal is not open!\n");
		return SERIAL_FAILED;
	}
	/* save current port settings */
	ret = tcgetattr(fd, &oldtio);
	if (ret < 0) {
		S_ERROR("failed to set get old attribs\n");
		return SERIAL_FAILED;
	}

	/* get current settings, and modify as needed */
	ret = tcgetattr(fd, &newtio);
	if (ret < 0) {
		S_ERROR("failed to get current attribs\n");
		return SERIAL_FAILED;
	}

	switch (s_baud_rate) {
	case 57600:
		s_baud_rate = B57600;
		break;
	case 115200:
		s_baud_rate = B115200;
		break;
		/* Add other baudrates - see /usr/include/bits/termios.h */
	default:
		S_ERROR("Unknown baudrate %d\n", (unsigned int)s_baud_rate);
		return SERIAL_FAILED;
	}
	cfsetospeed(&newtio, s_baud_rate);
	cfsetispeed(&newtio, s_baud_rate);

	newtio.c_cflag &= ~(CS5 | CS6 | CS7 | CS8);
	switch (s_data_bits) {
	case 5:
		newtio.c_cflag |= CS5;
		break;
	case 6:
		newtio.c_cflag |= CS6;
		break;
	case 7:
		newtio.c_cflag |= CS7;
		break;
	case 8:
		newtio.c_cflag |= CS8;
		break;
	default:
		S_ERROR("unknown data bit %d\n", s_data_bits);
		return SERIAL_FAILED;
	}

	switch (s_stop_bits) {
	case ONE_STOP_BIT:
		newtio.c_cflag &= ~CSTOPB;
		break;
	case TWO_STOP_BIT:
		newtio.c_cflag |= CSTOPB;
		break;
	default:
		S_ERROR("unknown stop bit %d\n", s_stop_bits);
		return SERIAL_FAILED;
	}

	newtio.c_cflag &= ~(PARENB | PARODD);
	newtio.c_iflag &= ~(IGNPAR);
	switch (s_parity) {
	case ODDPARITY:	/* odd */
		newtio.c_cflag |= PARENB | PARODD;
		newtio.c_iflag |= IGNPAR;
		break;
	case EVENPARITY:	/* even */
		newtio.c_cflag |= PARENB;
		newtio.c_iflag = 0;
		break;
	case NOPARITY:		/* none */
		break;
	default:
		S_ERROR("unknown parity %d", s_parity);
		return SERIAL_FAILED;
	}

	newtio.c_iflag |= IGNBRK;
	newtio.c_cflag |= CLOCAL | CREAD;

	S_INFO("c_cflag: 0x%08x\n", (unsigned int)(newtio.c_cflag));

	newtio.c_oflag = 0;
	/* set input mode (non-canonical, no echo,...) */
	newtio.c_lflag = 0;
	newtio.c_cc[VTIME] = VTIME_SET;
	newtio.c_cc[VMIN] = 1;
	newtio.c_cc[VSWTC] = 0;
	ret = tcflush(fd, TCIFLUSH);
	if (ret < 0) {
		S_ERROR("failed to set flush buffers\n");
		return SERIAL_FAILED;
	}
	ret = tcsetattr(fd, TCSANOW, &newtio);
	if (ret < 0) {
		S_INFO("tcsetattr -> %s (%d) fd=%d", strerror(errno), ret, fd);
		S_ERROR("failed to set new attribs\n");
		return SERIAL_FAILED;
	}
	S_INFO("Serial port %s configured fine\n", port);
	return SERIAL_OK;
}

/**
 * @brief Flush the serial port data
 *
 * @param rx_bytes return bytes that remains(TBD)
 * @param tx_bytes the bytes that are to be send(TBD)
 *
 * @return error/fail
 */
signed int s_flush(unsigned int *rx_bytes, unsigned int *tx_bytes)
{
	int ret;
	if (!fd) {
		S_ERROR("terminal is not open!\n");
		return SERIAL_FAILED;
	}
	ret = tcflush(fd, TCIFLUSH);
	if (ret < 0) {
		S_ERROR("failed to flush buffers2\n");
		return SERIAL_FAILED;
	}
	S_INFO("Serial port %s flushed fine\n", port);

	return SERIAL_OK;
}

/**
 * @brief s_close - close the serial port
 *
 * @return sucess/fail
 */
signed char s_close(void)
{
	int ret = 0;
	if (!fd) {
		S_ERROR("terminal is not open!\n");
		return SERIAL_FAILED;
	}
	/*
	 * To prevent switching modes before the last vestiges
	 * of the data bits have been send, sleep a second.
	 * This seems to be especially true for usb2serial
	 * convertors.. it does look as if the data is buffered
	 * at the usb2serial device itself and closing/changing
	 * attribs before the final data is pushed is going to
	 * kill the last bits which need to be send
	 */
	sleep(1);
	/* restore the old port settings */
	ret = tcsetattr(fd, TCSANOW, &oldtio);
	if (ret < 0) {
		S_ERROR("failed to rest old settings\n");
		return SERIAL_FAILED;
	}
	ret = tcflush(fd, TCIFLUSH);
	if (ret < 0) {
		S_ERROR("failed to flush serial file handle\n");
	}
	ret = close(fd);
	fd = 0;
	if (ret < 0) {
		S_ERROR("failed to close serial file handle\n");
		return SERIAL_FAILED;
	}
	S_INFO("Serial closed %s fine\n", port);
	return SERIAL_OK;
}

/**
 * @brief s_read - serial port read
 *
 * @param p_buffer buffer
 * @param size buffer length
 *
 * @return bytes read if ok, else SERIAL_FAILED
 */
signed int s_read(unsigned char *p_buffer, unsigned long size)
{
	int ret = 0;
	if (!fd) {
		S_ERROR("terminal is not open!\n");
		return SERIAL_FAILED;
	}
	/* read entire chunk.. no giving up! */
	newtio.c_cc[VMIN] = size;
	newtio.c_cc[VTIME] = VTIME_SET;
	ret = tcsetattr(fd, TCSANOW, &newtio);
	ret = read(fd, p_buffer, size);
	if (ret < 0) {
		S_ERROR("failed to read data\n");
		return SERIAL_FAILED;
	}

	S_INFO("Serial read requested=%d, read=%d\n", size, ret);
	return ret;
}

/**
 * @brief s_write - write to serial port
 *
 * @param p_buffer - buffer pointer
 * @param size -size of buffer
 *
 * @return bytes wrote if ok, else SERIAL_FAILED
 */
signed int s_write(unsigned char *p_buffer, unsigned long size)
{
	int ret = 0, ret1;
	if (!fd) {
		S_ERROR("terminal is not open!\n");
		return SERIAL_FAILED;
	}
	ret = write(fd, p_buffer, size);
	if (ret < 0) {
		S_ERROR("failed to write data\n");
		return SERIAL_FAILED;
	}
	/* Wait till it is emptied */
	ret1 = tcdrain(fd);
	if (ret1 < 0) {
		S_ERROR("failed in datai drain\n");
		perror(NULL);
		return ret1;
	}
	S_INFO("Serial wrote Requested=%ld wrote=%d\n", size, ret);
	return ret;
}

/**
 * @brief s_read_remaining - get the remaining bytes (TBD)
 *
 * @return error or num bytes remaining
 */
signed int s_read_remaining(void)
{
	if (!fd) {
		S_ERROR("terminal is not open!\n");
		return SERIAL_FAILED;
	}
	/* Not implemented yet */
	return (0);
}

/**
 * @brief s_getc - get a character from serial port
 *
 * @return character read or error
 */
signed int s_getc(void)
{
	unsigned char x = 0;
	int ret = 0;
	ret = s_read(&x, 1);
	if (ret < 0) {
		S_ERROR("getc failed-%d\n", ret);
		return ret;
	}
	S_INFO("[%c]%x", x, x);
	return x;
}

/**
 * @brief s_putc - put a character into serial port
 *
 * @param x - character to write
 *
 * @return character written or error
 */
signed int s_putc(char x)
{
	int ret = 0;
	S_INFO("[%c] 0x%02x", x, (unsigned char)x);
	ret = s_write((unsigned char *)&x, 1);
	if (ret < 0) {
		S_ERROR("putc failed-%d\n", ret);
		return ret;
	}
	return x;
}
