/**
 * @file
 * @brief Peripheral Boot downloader over UART
 *
 * FileName: src/pserial.c
 *
 * Application which uses Peripheral mode configuration to download
 * an image using the OMAP ROM Code protocol
 *
 */
/*
 * (C) Copyright 2008-2009
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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "rev.h"
#include "serial.h"
#include "file.h"
#include "f_status.h"

#define ASIC_ID_SIZE	7
#define ASIC_ID_OMAP4430 0x4430
#define ASIC_ID_OMAP3430 0x3430
#define ASIC_ID_OMAP3630 0x3630

#define MAX_BUF		2048
#define TOT_SIZE	MAX_BUF
#define PRINT_SIZE	100

#define PORT_ARG	"p"
#define PORT_ARG_C	'p'
#define SEC_ARG		"f"
#define SEC_ARG_C	'f'
#define VERBOSE_ARG	"v"
#define VERBOSE_ARG_C	'v'

/**
 * @brief send_file - send a file to the host
 *
 * @param f_name file to send
 *
 * @return fail/success
 */
static int send_file(char *f_name)
{
	signed int size = 0;
	unsigned char buffer[TOT_SIZE] = { 0 };
	unsigned int tot_read = 0, to_read;
	signed int cur_read;
	signed int ret;

	size = f_size(f_name);

	if (size < 0) {
		APP_ERROR("File Size Operation failed! File exists?\n")
		    return size;
	}
	if (f_open(f_name) != FILE_OK) {
		APP_ERROR("File Open failed!File Exists & readable?\n")
		    return -1;
	}
	f_status_init(size, NORMAL_PRINT);
	/* Send the size */
	ret = s_write((unsigned char *)&size, sizeof(size));
	if (ret != sizeof(size)) {
		APP_ERROR("Oops.. did not send size properly :(\n")
		    return -1;
	}
	/* Send the file to target */
	while (tot_read < size) {
		int cur_write;
		to_read = size - tot_read;
		to_read = (to_read > TOT_SIZE) ? TOT_SIZE : to_read;
		cur_read = f_read(buffer, to_read);
		if (cur_read < 0) {
			APP_ERROR("Oops.. read failed!\n")
			    break;
		}
		cur_write = s_write(buffer, cur_read);
		if (cur_write != cur_read) {
			APP_ERROR("did not right data to serial port"
				  " serial cur_write = %d, file cur_read= %d\n",
				  cur_write, cur_read)
		}
		tot_read += cur_read;
		f_status_show(tot_read);
#ifdef DEBUG
		{
			int i = 0;
			for (i = 0; i < cur_read; i++) {
				printf("[%04d]0x%02X ", i, buffer[i]);
			}
		}
		printf("\n");
#endif
	}

	if (f_close() != FILE_OK) {
		APP_ERROR("File Close failed\n")
	}
	return 0;
}

/**
 * @brief usage - help info
 *
 * @param appname my name
 */
static void usage(char *appname)
{
#ifdef __WIN32__
#define PORT_NAME "COM1"
#define F_NAME "c:\\temp\\u-boot.bin"
#else
#define PORT_NAME "/dev/ttyS0"
#define F_NAME "~/tmp/u-boot.bin"
#endif
	printf("App description:\n"
	       "---------------\n"
	       "This Application helps download a second file as "
	       "response to ASIC ID over serial port\n\n"
	       "Syntax:\n"
	       "------\n"
	       "%s -" PORT_ARG " portName -" SEC_ARG " fileToDownload [-"
	       VERBOSE_ARG "]\n\n"
	       "Where:\n" "-----\n"
	       "portName - RS232 device being used. Example: " PORT_NAME "\n"
	       "fileToDownload - file to be downloaded as response to "
	       "asic id\n"
	       "-"VERBOSE_ARG " - print complete ASIC id dump(optional)\n"
	       "\nUsage Example:\n" "-------------\n"
	       "%s -" PORT_ARG " " PORT_NAME " -" SEC_ARG " " F_NAME "\n",
	       appname, appname);
	REVPRINT();
	LIC_PRINT();

}

/**
 * @brief main application entry
 *
 * @param argc
 * @param *argv
 *
 * @return pass/fail
 */
int main(int argc, char **argv)
{
	signed char ret = 0;
	unsigned char buff[MAX_BUF];
	unsigned int download_command = 0xF0030002;
	char *port = NULL;
	char *second_file = NULL;
	char *appname = argv[0];
	int c;
	int read_size = 0;
	unsigned int asic_id = 0;
	char verbose = 0;
	/* Option validation */
	opterr = 0;

	while ((c = getopt(argc, argv, PORT_ARG ":" SEC_ARG ":" VERBOSE_ARG)) !=
			-1)
		switch (c) {
		case VERBOSE_ARG_C:
			verbose = 1;
			break;
		case PORT_ARG_C:
			port = optarg;
			break;
		case SEC_ARG_C:
			second_file = optarg;
			break;
		case '?':
			if ((optopt == SEC_ARG_C) || (optopt == PORT_ARG_C)) {
				APP_ERROR("Option -%c requires an argument.\n",
					  optopt)
			} else if (isprint(optopt)) {
				APP_ERROR("Unknown option `-%c'.\n", optopt)
			} else {
				APP_ERROR("Unknown option character `\\x%x'.\n",
					  optopt)
			}
			usage(appname);
			return 1;
		default:
			abort();
		}
	if ((port == NULL) || (second_file == NULL)) {
		APP_ERROR("Error: Not Enough Args\n")
		    usage(appname);
		return -1;
	}

	/* Setup the port */
	ret = s_open(port);
	if (ret != SERIAL_OK) {
		APP_ERROR("serial open failed\n")
		    return ret;
	}
	ret = s_configure(115200, EVENPARITY, ONE_STOP_BIT, 8);
	if (ret != SERIAL_OK) {
		s_close();
		APP_ERROR("serial configure failed\n")
		    return ret;
	}

	/* Read ASIC ID */
	printf("Waiting For Device ASIC ID: Press Ctrl+C to stop\n");
	while (!ret) {
		ret = s_read(buff, 1);
		if (buff[0] != 0x04)
			ret = 0;
	}

	ret = 0;
	while (!ret) {
		ret = s_read(buff, ASIC_ID_SIZE);
		if ((ret != ASIC_ID_SIZE)) {
			APP_ERROR("Did not read asic ID ret = %d\n", ret)
			    s_close();
			return ret;
		}
	}
	if (verbose) {
		int i = 0;
		for (i = 0; i < ASIC_ID_SIZE; i++)
			printf("[%d] 0x%x[%c]\n", i, buff[i], buff[i]);
	}
	asic_id = (buff[3] << 8) + buff[4];
	switch (asic_id) {
	case ASIC_ID_OMAP4430:
		printf("ASIC ID Detected: OMAP 4430 with ROM Version"
			" 0x%02x%02x\n", buff[5], buff[6]);
		read_size = 59;
		break;
	case ASIC_ID_OMAP3430:
	case ASIC_ID_OMAP3630:
		printf("ASIC ID Detected: OMAP %04x with ROM Version"
			" 0x%02x%02x\n", asic_id, buff[5], buff[6]);
		read_size = 50;
		break;
	default:
		printf("ASIC ID Detected: 0x%02x 0x%02x 0x%02x 0x%02x\n",
			buff[3], buff[4], buff[5], buff[6]);
		read_size = 50;
		break;
	}

	ret = 0;
	while (!ret) {
		ret = s_read(buff, read_size);
		if ((ret != read_size)) {
			APP_ERROR("Did not read asic ID ret = %d\n", ret)
			    s_close();
			return ret;
		}
	}

	if (verbose) {
		int i = 0;
		for (i = 0; i < read_size; i++)
			printf("[%d] 0x%x[%c]\n", i, buff[i], buff[i]);
	}

	/* Send the Download command */
	printf("Sending 2ndFile:\n");
	ret =
	    s_write((unsigned char *)&download_command,
		    sizeof(download_command));
	if (ret != sizeof(download_command)) {
		APP_ERROR("oppps!! did not actually manage to send command\n")
		    return -1;
	}
	if (send_file(second_file) != 0) {
		APP_ERROR("send file failed!\n")
	}

	ret = s_close();
	if (ret != SERIAL_OK) {
		APP_ERROR("serial close failed\n")
		    return ret;
	}
	printf("\nFile download completed.\n");

	return ret;
}
