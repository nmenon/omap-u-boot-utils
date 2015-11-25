/**
 * @file
 * @brief Linux serial port breaker routine
 *
 * FileName: src/sysrq.c
 *
 * Just generate the magic sysrq sequence given a serial port.
 *
 */
/*
 * (C) Copyright 2015
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
#define SYSRQ_ARG		"f"
#define SYSRQ_ARG_C	'f'

/**
 * @brief send_sysrq - send a file to the host
 *
 * @param sysrq_key: key sequence to send
 *
 * @return fail/success
 */
static int send_sysrq(unsigned char *sysrq_key)
{
	signed int ret;

	/* Apply break for 1 second */
	ret = s_break(1, 0);
	if (ret < 0) {
		APP_ERROR("Opps.. failed to send break\n");
		return -1;
	}
	/* Send the sysrq key */
	ret = s_write(sysrq_key, 1);
	if (ret != 1) {
		APP_ERROR("Oops.. did not send size properly :(\n")
		    return -1;
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
#else
#define PORT_NAME "/dev/ttyS0"
#endif
	printf("App description:\n"
	       "---------------\n"
	       "This Application sends a sysrq keysequence over serial port\n\n"
	       "Syntax:\n"
	       "------\n"
	       "%s -" PORT_ARG " portName -" SYSRQ_ARG " sysrq_key\n\n"
	       "Where:\n" "-----\n"
	       "portName - RS232 device being used. Example: " PORT_NAME "\n"
	       "sysrq_key - Sysrq key\n"
	       "\nUsage Example:\n" "-------------\n"
	       "%s -" PORT_ARG " " PORT_NAME " -" SYSRQ_ARG " t\n",
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
	char *port = NULL;
	char *sysrq_key = NULL;
	char *appname = argv[0];
	int c;
	/* Option validation */
	opterr = 0;

	while ((c = getopt(argc, argv, PORT_ARG ":" SYSRQ_ARG ":")) != -1)
		switch (c) {
		case PORT_ARG_C:
			port = optarg;
			break;
		case SYSRQ_ARG_C:
			sysrq_key = optarg;
			break;
		case '?':
			if ((optopt == SYSRQ_ARG_C) || (optopt == PORT_ARG_C)) {
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
	if ((port == NULL) || (sysrq_key == NULL)) {
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
	ret = s_configure(115200, NOPARITY, ONE_STOP_BIT, 8);
	if (ret != SERIAL_OK) {
		s_close();
		APP_ERROR("serial configure failed\n")
		    return ret;
	}

	if (send_sysrq((unsigned char *)sysrq_key) != 0) {
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
