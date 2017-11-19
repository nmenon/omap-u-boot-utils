/**
 * @file
 * @brief Peripheral Boot downloader over USB
 *
 * FileName: src/pserial.c
 *
 * OMAP USB peripheral booting helper using libusb example
 * USB module usage based on testlibusb.c
 *
 * Caveat: NOTE: Windows support is still a work in progress
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
#ifdef __WIN32__
#include <windows.h>		/* for Sleep() in dos/mingw */
#else
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <libusb.h>
#include <time.h>
#include "rev.h"
#include "file.h"
#include "f_status.h"

/* Texas Instruments */
#define SEARCH_VENDOR_DEFAULT	0x0451
/* OMAP3430 */
#define SEARCH_PRODUCT_DEFAULT	0xD009
/* bConfigurationValue: 1 */
#define CONFIG_INDEX_DEFAULT	0x1
/* bInterfaceNumber: 0 */
#define INTERFACE_INDEX_DEFAULT	0x0
#define DEVICE_IN_ENDPOINT		0x81
#define DEVICE_OUT_ENDPOINT		0x1
#define ASICID_SIZE_OMAP3		69	/* 1+7+4+23+23+11 */
#define ASICID_SIZE_OMAP4		81	/* 1+7+4+23+35+11 */
#define ASIC_ID_TIMEOUT			100
#define COMMAND_SIZE			4
#define GET_ASICID_COMMAND		0xF0030003
#define DOWNLOAD_COMMAND		0xF0030002
#define MAX_SIZE				(64 * 1024)
#define READ_BUFFER_SIZE		4096

/* Print control */
#define V_PRINT(ARGS...) if (verbose >= 1) printf(ARGS)
#define N_PRINT(ARGS...) if (verbose >= 0) printf(ARGS)

static int verbose = 0;
static int asicid_size = ASICID_SIZE_OMAP3;
static unsigned int search_vendor = SEARCH_VENDOR_DEFAULT;
static unsigned int search_product = SEARCH_PRODUCT_DEFAULT;
static int config_idx = CONFIG_INDEX_DEFAULT;
static libusb_device_handle *udev;
static char *program_name;

/**
 * @brief sleep for a definite time
 *
 * @param ms delay in ms
 *
 * @return none
 */
static void usb_sleep(unsigned int ms)
{
#ifndef __WIN32__
	struct timespec poll_interval = {
		.tv_sec = 0,
		.tv_nsec = ms * 1000 * 1000,
	};
	nanosleep(&poll_interval, NULL);
#else
	Sleep(ms);
#endif
}

/******* ACTUAL CONFIGURATION OF THE DEVICE ********************/
int configure_device(libusb_device_handle *udev)
{
	int ret = 0;
	ret = libusb_set_configuration(udev, config_idx);
	if (ret != 0) {
		APP_ERROR("set configuration returned error :%d %s\n", ret,
			  strerror(errno));
		return ret;
	}
	return 0;
}

/* send commands to the boot rom; returns 0 on success */
static int send_command(libusb_device_handle *udev, unsigned int command)
{
	int ret = 0;
	int r = libusb_bulk_transfer(udev, DEVICE_OUT_ENDPOINT, (unsigned char *)&command,
			   sizeof(command), &ret, ASIC_ID_TIMEOUT);
        if (r != 0)
	   ret = 0;	
	if (ret != COMMAND_SIZE) {
		APP_ERROR
		    ("CMD:Expected to write %ld, "
		     "actual write %d - err str: %s\n",
		     (long int)sizeof(command), ret, strerror(errno));
	}

	return !(ret == COMMAND_SIZE);
}

/*************** ACTUAL TRANSMISSION OF FILE *****************/
int send_file(libusb_device_handle *udev, char *f_name)
{
	int ret = 0;
	unsigned long cur_size = 0;
	unsigned int filesize;
	unsigned char asic_buffer[ASICID_SIZE_OMAP4];
	unsigned char read_buffer[READ_BUFFER_SIZE];
	int fail = 0;
	int r;

	filesize = f_size(f_name);
	if (filesize > MAX_SIZE) {
		APP_ERROR("Filesize %d >%d max\n",
			  (unsigned int)filesize, MAX_SIZE);
		return -1;
	}

	ret = f_open(f_name);
	if (ret < 0) {
		APP_ERROR("error opening file! -%d\n", ret);
		return -1;
	}
	if (verbose >= 0) {
		f_status_init(filesize, NORMAL_PRINT);
	}
	/* Grab the interface for us to send data */
	ret = libusb_claim_interface(udev, INTERFACE_INDEX_DEFAULT);
	if (ret) {
		APP_ERROR("error claiming usb interface %d-%s\n", ret,
			  strerror(errno));
		return (ret);
	}

	/* read ASIC ID */
	r =
	  libusb_bulk_transfer(udev, DEVICE_IN_ENDPOINT, asic_buffer, asicid_size, &ret,
			  ASIC_ID_TIMEOUT);
	if (r != 0)
	  ret = 0;

	/* if no ASIC ID, request it explicitly */
	if (ret != asicid_size) {
		if (send_command(udev, GET_ASICID_COMMAND)) {
			APP_ERROR("Can't get ASIC ID out of the wretched chip.\n");
			fail = -1;
			goto closeup;
		}
		/* now try again */
		r = libusb_bulk_transfer(udev, DEVICE_IN_ENDPOINT, asic_buffer,
				    asicid_size, &ret, ASIC_ID_TIMEOUT);
		if (r != 0)
		  ret = 0;
		if (ret != asicid_size) {
			APP_ERROR("Expected to read %d, read %d - err str: %s\n",
				  asicid_size, ret, strerror(errno));
			fail = -1;
			goto closeup;
		}
	}
	if (verbose > 0) {
		int i = 0;
		printf("ASIC ID:\n");
		while (i < asicid_size) {
			printf("%d: 0x%x[%c]\n", i, asic_buffer[i],
			       asic_buffer[i]);
			i++;
		}
	}
	usb_sleep(50);
	/* Send the  Continue Peripheral boot command */
	if (send_command(udev, DOWNLOAD_COMMAND)) {
		fail = -1;
		goto closeup;
	}
	usb_sleep(50);
	/* Send in the filesize */
	r =
	    libusb_bulk_transfer(udev, DEVICE_OUT_ENDPOINT, (unsigned char *)&filesize,
			   sizeof(filesize), &ret, ASIC_ID_TIMEOUT);
	if (r != 0)
	  ret = 0;
	if (ret != sizeof(filesize)) {
		APP_ERROR
		    ("FSize:Expected to write %ld, "
		     "actual write %d - err str: %s\n",
		     (long int)sizeof(filesize), ret, strerror(errno));
		fail = -1;
		goto closeup;
	}
	/* pump in the data */
	while (filesize) {
		int r_size = f_read((unsigned char *)read_buffer,
				    sizeof(read_buffer));
		r =
		    libusb_bulk_transfer(udev, DEVICE_OUT_ENDPOINT, read_buffer,
				   r_size, &ret, ASIC_ID_TIMEOUT);
		if (r != 0)
		  ret = 0; 
		if (ret != r_size) {
			APP_ERROR
			    ("DDump:Expected to write %d, actual write %d %s\n",
			     r_size, ret, strerror(errno));
			fail = -1;
			goto closeup;
		}
		filesize -= r_size;
		if (verbose >= 0) {
			cur_size += r_size;
			f_status_show(cur_size);
		}
	}
	/* close the device */
      closeup:
	ret = libusb_release_interface(udev, INTERFACE_INDEX_DEFAULT);
	if (ret) {
		APP_ERROR("error releasing usb interface %d-%s\n", ret,
			  strerror(errno));
		/* Fall thru */
		fail = ret;
	}

	libusb_close(udev);

	ret = f_close();
	if (ret) {
		APP_ERROR("error closing file %d-%s\n", ret, strerror(errno));
		/* Fall thru */
		fail = ret;
	}
	return fail;
}

void help(void)
{
	printf("App description:\n"
	       "---------------\n"
	       "This Application helps download a second file as "
	       "response to ASIC ID over USB\n\n"
	       "Syntax:\n"
	       "------\n"
	       "  %s [-v] [-V] [-d device_ID] -f input_file \n"
	       "Where:\n" "-----\n"
	       "   -v          : (optional) verbose messages\n"
	       "   -V          : (optional) verbose messages + usblib debug messages\n"
	       "   -q          : (optional) Ultra quiet - no outputs other than error\n"
	       "   -d device_ID: (optional) USB Device id (Uses default of 0x%X)\n"
	       "   -f input_file: input file to be transmitted to target\n"
	       "NOTE: it is required to run this program in sudo mode to get access at times\n"
	       "Usage Example:\n" "-------------\n"
	       "sudo %s -f F_NAME \n", program_name, search_product,
	       program_name);
	REVPRINT();
	LIC_PRINT();
}

int main(int argc, char *argv[])
{
	char *filename = NULL;
	int found = 0;
	int x = 0;
	int old_dev = 0;
	int new_dev = 0;
	/* NOTE: mingw did not like this.. */
	int c;
	int filesize = -1;

	program_name = argv[0];
	/* Options supported:
	 *  -f input filename - file to send
	 *  -d deviceID - USB deviceID
	 *  -v -verbose
	 *  -q -ultraquiet
	 */
	while ((c = getopt(argc, argv, ":vVqd:f:4")) != -1) {
		switch (c) {
		case 'q':
			verbose = -1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'V':
			verbose = 2;
			break;
		case 'f':
			filename = optarg;
			break;
		case 'd':
			sscanf(optarg, "%x", &search_product);
			break;
		case '4':
			asicid_size = ASICID_SIZE_OMAP4;
			break;
		case '?':
		default:
			APP_ERROR("Missing/Wrong Arguments\n");
			help();
			return -1;
		}
	}
	if (filename)
		filesize = f_size(filename);
	/* Param validate */
	if ((NULL == filename) || (filesize == -1)) {
		APP_ERROR("Missing file to download\n");
		help();
		return -2;
	}
/*
	if (verbose == 2) {
		usb_set_debug(255);
	}
	*/
	libusb_init(NULL);
	N_PRINT("Waiting for USB device vendorID=0x%X "
		"and productID=0x%X:\n", search_vendor, search_product);
	while (1) {
                udev = libusb_open_device_with_vid_pid(NULL,  search_vendor, search_product);
		if (udev)
		  break;
		/* sleep a bit, just to not do too much busy waiting */
                usb_sleep(50);
	}
	c = configure_device(udev);
	if (c) {
		APP_ERROR("configure dev failed\n");
		goto exit;
	}
	c = send_file(udev, filename);
	if (c) {
		APP_ERROR("send file failed\n");
	}
	if (!c) {
		N_PRINT("\n%s downloaded successfully to target\n", filename);
	}
      exit:
	return c;
}
