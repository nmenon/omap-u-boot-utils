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
#include <usb.h>
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
static struct usb_device *found_dev = NULL;
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

/************ POMPOUS SHOW OF USB DESCRIPTOR **********************/
void print_endpoint(struct usb_endpoint_descriptor *endpoint)
{
	printf("      bEndpointAddress: %02xh\n", endpoint->bEndpointAddress);
	printf("      bmAttributes:     %02xh\n", endpoint->bmAttributes);
	printf("      wMaxPacketSize:   %d\n", endpoint->wMaxPacketSize);
	printf("      bInterval:        %d\n", endpoint->bInterval);
	printf("      bRefresh:         %d\n", endpoint->bRefresh);
	printf("      bSynchAddress:    %d\n", endpoint->bSynchAddress);
}

void print_altsetting(struct usb_interface_descriptor *interface)
{
	int i;

	printf("    bInterfaceNumber:   %d\n", interface->bInterfaceNumber);
	printf("    bAlternateSetting:  %d\n", interface->bAlternateSetting);
	printf("    bNumEndpoints:      %d\n", interface->bNumEndpoints);
	printf("    bInterfaceClass:    %d\n", interface->bInterfaceClass);
	printf("    bInterfaceSubClass: %d\n", interface->bInterfaceSubClass);
	printf("    bInterfaceProtocol: %d\n", interface->bInterfaceProtocol);
	printf("    iInterface:         %d\n", interface->iInterface);

	for (i = 0; i < interface->bNumEndpoints; i++)
		print_endpoint(&interface->endpoint[i]);
}

void print_interface(struct usb_interface *interface)
{
	int i;

	for (i = 0; i < interface->num_altsetting; i++)
		print_altsetting(&interface->altsetting[i]);
}

void print_configuration(struct usb_config_descriptor *config)
{
	int i;

	printf("  wTotalLength:         %d\n", config->wTotalLength);
	printf("  bNumInterfaces:       %d\n", config->bNumInterfaces);
	printf("  bConfigurationValue:  %d\n", config->bConfigurationValue);
	printf("  iConfiguration:       %d\n", config->iConfiguration);
	printf("  bmAttributes:         %02xh\n", config->bmAttributes);
	printf("  MaxPower:             %d\n", config->MaxPower);

	for (i = 0; i < config->bNumInterfaces; i++)
		print_interface(&config->interface[i]);
}

int print_device(struct usb_device *dev, int level)
{
	usb_dev_handle *udev;
	char description[256];
	char string[256];
	int ret, i;

	udev = usb_open(dev);
	if (udev) {
		if (dev->descriptor.iManufacturer) {
			ret =
			    usb_get_string_simple(udev,
						  dev->descriptor.iManufacturer,
						  string, sizeof(string));
			if (ret > 0)
				snprintf(description, sizeof(description),
					 "%s - ", string);
			else
				snprintf(description, sizeof(description),
					 "%04X - ", dev->descriptor.idVendor);
		} else
			snprintf(description, sizeof(description), "%04X - ",
				 dev->descriptor.idVendor);

		if (dev->descriptor.iProduct) {
			ret =
			    usb_get_string_simple(udev,
						  dev->descriptor.iProduct,
						  string, sizeof(string));
			if (ret > 0)
				snprintf(description + strlen(description),
					 sizeof(description) -
					 strlen(description), "%s", string);
			else
				snprintf(description + strlen(description),
					 sizeof(description) -
					 strlen(description), "%04X",
					 dev->descriptor.idProduct);
		} else
			snprintf(description + strlen(description),
				 sizeof(description) - strlen(description),
				 "%04X", dev->descriptor.idProduct);

	} else
		snprintf(description, sizeof(description), "%04X - %04X",
			 dev->descriptor.idVendor, dev->descriptor.idProduct);

	N_PRINT("%.*sDev #%d: %s\n", level * 2, "                    ",
		dev->devnum, description);

	if (udev) {
		if (dev->descriptor.iSerialNumber) {
			ret =
			    usb_get_string_simple(udev,
						  dev->descriptor.iSerialNumber,
						  string, sizeof(string));
			if (ret > 0)
				N_PRINT("%.*s  - Serial Number: %s\n",
					level * 2, "                    ",
					string);
		}
	}

	if (udev)
		usb_close(udev);

	if (verbose > 0) {
		if (!dev->config) {
			printf("  Couldn't retrieve descriptors\n");
			return 0;
		}

		for (i = 0; i < dev->descriptor.bNumConfigurations; i++)
			print_configuration(&dev->config[i]);
	} else {
		for (i = 0; i < dev->num_children; i++)
			print_device(dev->children[i], level + 1);
	}

	return 0;
}

/************ SEARCH FOR THE DEVICE **********************/
int search_device(struct usb_device *dev, int level)
{
	int ret = 0, i;
	if (verbose > 0)
		for (i = 0; i < level; i++) {
			printf("--");
		}
	V_PRINT("[%d]%04X - %04X", level, dev->descriptor.idVendor,
		dev->descriptor.idProduct);
	/* Exit condition: check match */
	if ((dev->descriptor.idVendor == search_vendor)
	    && (dev->descriptor.idProduct == search_product)) {
		V_PRINT("-->Hoorrah!!! found!!!\n");
		found_dev = dev;
		return 1;
	} else {
		V_PRINT("\n");
	}
	for (i = 0; i < dev->num_children; i++) {
		ret = search_device(dev->children[i], level + 1);
		if (ret)
			break;
	}

	return ret;
}

/******* ACTUAL CONFIGURATION OF THE DEVICE ********************/
int configure_device(struct usb_device *dev)
{
	usb_dev_handle *udev;
	int ret = 0;
	udev = usb_open(dev);
	if (!udev) {
		APP_ERROR("Unable to open the device.. giving up!\n");
		return -1;
	}
	ret = usb_set_configuration(udev, config_idx);
	if (ret != 0) {
		APP_ERROR("set configuration returned error :%d %s\n", ret,
			  strerror(errno));
		return ret;
	}
	usb_close(udev);
	return 0;
}

/* send commands to the boot rom; returns 0 on success */
static int send_command(usb_dev_handle *udev, unsigned int command)
{
	int ret =
	    usb_bulk_write(udev, DEVICE_OUT_ENDPOINT, (char *)&command,
			   sizeof(command), ASIC_ID_TIMEOUT);
	if (ret != COMMAND_SIZE) {
		APP_ERROR
		    ("CMD:Expected to write %ld, "
		     "actual write %d - err str: %s\n",
		     (long int)sizeof(command), ret, strerror(errno));
	}

	return !(ret == COMMAND_SIZE);
}

/*************** ACTUAL TRANSMISSION OF FILE *****************/
int send_file(struct usb_device *dev, char *f_name)
{
	usb_dev_handle *udev;
	int ret = 0;
	unsigned long cur_size = 0;
	unsigned int filesize;
	char asic_buffer[ASICID_SIZE_OMAP4];
	char read_buffer[READ_BUFFER_SIZE];
	int fail = 0;

	filesize = f_size(f_name);
	if (filesize > MAX_SIZE) {
		APP_ERROR("Filesize %d >%d max\n",
			  (unsigned int)filesize, MAX_SIZE);
		return -1;
	}

	/* Open the required device and file */
	udev = usb_open(dev);
	if (!udev) {
		APP_ERROR("Unable to open the device.. giving up!\n");
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
	ret = usb_claim_interface(udev, INTERFACE_INDEX_DEFAULT);
	if (ret) {
		APP_ERROR("error claiming usb interface %d-%s\n", ret,
			  strerror(errno));
		return (ret);
	}

	/* read ASIC ID */
	ret =
	    usb_bulk_read(udev, DEVICE_IN_ENDPOINT, asic_buffer, asicid_size,
			  ASIC_ID_TIMEOUT);
	/* if no ASIC ID, request it explicitly */
	if (ret != asicid_size && send_command(udev, GET_ASICID_COMMAND)) {
		APP_ERROR("Can't get ASIC ID out of the wretched chip.\n");
		fail = -1;
		goto closeup;
	}

	/* now try again */
	ret =
	    usb_bulk_read(udev, DEVICE_IN_ENDPOINT, asic_buffer, asicid_size,
			  ASIC_ID_TIMEOUT);
	if (ret != asicid_size) {
		APP_ERROR("Expected to read %d, read %d - err str: %s\n",
			  asicid_size, ret, strerror(errno));
		fail = -1;
		goto closeup;
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
	/* Send the  Continue Peripheral boot command */
	if (send_command(udev, DOWNLOAD_COMMAND)) {
		fail = -1;
		goto closeup;
	}
	/* Send in the filesize */
	ret =
	    usb_bulk_write(udev, DEVICE_OUT_ENDPOINT, (char *)&filesize,
			   sizeof(filesize), ASIC_ID_TIMEOUT);
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
		ret =
		    usb_bulk_write(udev, DEVICE_OUT_ENDPOINT, read_buffer,
				   r_size, ASIC_ID_TIMEOUT);
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
	ret = usb_release_interface(udev, INTERFACE_INDEX_DEFAULT);
	if (ret) {
		APP_ERROR("error releasing usb interface %d-%s\n", ret,
			  strerror(errno));
		/* Fall thru */
		fail = ret;
	}

	ret = usb_close(udev);
	if (ret) {
		APP_ERROR("error closing usb interface %d-%s\n", ret,
			  strerror(errno));
		/* Fall thru */
		fail = ret;
	}
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
	struct usb_bus *bus;
	char *filename = NULL;
	int found = 0;
	int x = 0;
	int old_bus = 0;
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

	if (verbose == 2) {
		usb_set_debug(255);
	}
	usb_init();
	old_bus = usb_find_busses();
	old_dev = 0;
	N_PRINT("Waiting for USB device vendorID=0x%X "
		"and productID=0x%X:\n", search_vendor, search_product);
	while (!found) {
		/* Enumerate buses once more.. just in case
		 * we have some one plugging in a device with a hub..
		 */
		new_dev = usb_find_devices();
		if (new_dev == old_dev) {
			/* no change in bus state.. BRUTE retry */
			usb_sleep(10);
			continue;
		}
		old_dev = new_dev;
		/* new device detected */
		for (bus = usb_get_busses(); bus; bus = bus->next) {
			if (bus->root_dev)
				found = search_device(bus->root_dev, 0);
			else {
				struct usb_device *dev;

				for (dev = bus->devices; dev; dev = dev->next) {
					found = search_device(dev, 0);
					if (found)
						break;
				}
			}
			if (found)
				break;
		}
		V_PRINT("Bus Scan %d:\n", x++);

	}

	c = 0;
	if (found) {
		if (!found_dev) {
			APP_ERROR("found_dev NULL! quitting sadly\n");
			c = -1;
			goto exit;
		}
		if (verbose >= 0) {
			c = print_device(found_dev, 0);
			if (c) {
				APP_ERROR("print dev failed\n");
				goto exit;
			}
		}
		c = configure_device(found_dev);
		if (c) {
			APP_ERROR("configure dev failed\n");
			goto exit;
		}
		c = send_file(found_dev, filename);
		if (c) {
			APP_ERROR("send file failed\n");
		}
	}
	if (!c) {
		N_PRINT("\n%s downloaded successfully to target\n", filename);
	}
      exit:
	return c;
}
