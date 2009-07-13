/**
 * @file
 * @brief Tiny little app which sends a command to uboot and waits
 *        for a response
 *
 * FileName: src/ucmd.c
 *
 * This sends a command to the serial port for uboot and exepects a
 * matching response
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

#define PORT_ARG	"p"
#define PORT_ARG_C	'p'
#define CMD_ARG		"c"
#define CMD_ARG_C	'c'
#define EXP_ARG		"e"
#define EXP_ARG_C	'e'

/**
 * @brief send_cmd - send the command to uboot
 *
 * @param cmd  - command to send
 *
 * @return - result
 */
static int send_cmd(char *cmd)
{
	int ret = 0;
	char *buffer;
	int len = strlen(cmd);
	buffer = calloc(len + 2, 1);
	if (buffer == NULL) {
		APP_ERROR("failed to allocate %d bytes\n", len + 2)
		    perror("fail reason:");
		return SERIAL_FAILED;
	}
	/* The command */
	strcpy(buffer, cmd);
	/* The enter key */
	strcat(buffer, "\n");

	ret = s_write((unsigned char *)buffer, strlen(buffer));
	free(buffer);
	if (ret < 0)
		return SERIAL_FAILED;
	return SERIAL_OK;
}

/**
 * @brief get_response - get the response from uboot
 *
 * @param expected - expected string from uboot
 *
 * @return -match or fail
 */
static int get_response(char *expected)
{
	int match_idx = 0;
	int len = strlen(expected);
	char current_char;
	int ret = 0;
	while (match_idx < len) {
		ret = s_getc();
		if (ret < 0) {
			APP_ERROR("Failed to read character\n")
			    return ret;
		}
		current_char = (char)ret;
		if (current_char == expected[match_idx]) {
			match_idx++;
		} else
			match_idx = 0;
		/* Dump the character to user screen */
		printf("%c", current_char);
		fflush(stdout);
	}
	/* Hoorah!! match found */
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
	       "This sends a command and expects a provided matching response "
	       "from target\n\n"
	       "Syntax:\n"
	       "------\n"
	       "%s -" PORT_ARG " portName -" CMD_ARG " \"command to send\" -"
	       EXP_ARG " \"Expect String\"\n\n"
	       "Where:\n" "-----\n"
	       "portName - RS232 device being used. Example: " PORT_NAME "\n"
	       "command to send - Command to send to uboot\n"
	       "Expect string - String to expect from target - on match"
	       " the application returns\n"
	       "Usage Example:\n" "-------------\n"
	       "%s -" PORT_ARG " " PORT_NAME " -" CMD_ARG " \"help\" -" EXP_ARG
	       " \"U-Boot>\"\n", appname, appname);
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
	char *command = NULL;
	char *expect = NULL;
	char *appname = argv[0];
	int c;
	/* Option validation */
	opterr = 0;

	while ((c =
		getopt(argc, argv, PORT_ARG ":" CMD_ARG ":" EXP_ARG ":")) != -1)
		switch (c) {
		case PORT_ARG_C:
			port = optarg;
			break;
		case CMD_ARG_C:
			command = optarg;
			break;
		case EXP_ARG_C:
			expect = optarg;
			break;
		case '?':
			if ((optopt == CMD_ARG_C) || (optopt == EXP_ARG_C)
			    || (optopt == PORT_ARG_C)) {
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
	if ((port == NULL) || (command == NULL) || (expect == NULL)) {
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

	/* Dump all previous data */
	ret = s_flush(NULL, NULL);
	if (ret != SERIAL_OK) {
		s_close();
		APP_ERROR("Failed to flush data\n")
		    return ret;
	}
	/* send the command to uboot */
	ret = send_cmd(command);
	if (ret != SERIAL_OK) {
		s_close();
		APP_ERROR("Failed to send command '%s'\n", command)
		    return ret;
	}

	printf("Output:\n");
	/* Wait for the response to come from target */
	ret = get_response(expect);
	if (ret != SERIAL_OK) {
		s_close();
		APP_ERROR("Failed to get expected '%s'\n", expect)
		    return ret;
	}

	ret = s_close();
	if (ret != SERIAL_OK) {
		APP_ERROR("serial close failed\n")
		    return ret;
	}
	printf("\nMatch Found. Operation completed!\n");

	return ret;
}
