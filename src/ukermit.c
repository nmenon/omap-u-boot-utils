/**
 * @file
 * @brief Reverse of kermit protocol implemented by uboot
 *
 * FileName: src/ukermit.c
 *
 * Ref: http://git.denx.de/?p=u-boot/u-boot-v2.git;f=commands/loadb.c;hb=HEAD
 *
 * Much of this code is based on loadb.c in U-Boot. This provides the
 * reverse logic of providing data packets alone to the host. we support
 * Kermit Data packet format only.
 * Typical usage is to run loadb in uboot, close the same and run this app
 *
 * Kermit Protocol consists of multiple packets flowing back and froth b/w
 * a Host a client. First implemented by columbia university can be seen here:
 * http://www.columbia.edu/kermit/
 *
 * Motivation:
 *
 * For a full fledged implementation, look for ckermit(Linux), kermit95(win),
 * gkermit or ekermit. Many terminal applications such as Hyperterminal(win)
 * support kermit. However, in many cases, users desire to have a simple app
 * which can send data to uboot by exploiting the crc checks over serial.
 *
 * Details:
 *
 * This code is written by reversing the sequence of data reception expected by
 * U-Boot. it may not be compatible with standard kermit protocol
 *
 * Kermit protocol in it's simplest form is described as a transmission
 * followed by a end of transmission character. Each transmission consists of
 * multiple packets. Each packet can be a session initiation, data packet or
 * many other packet types. This application supports only sending data packets.
 * So, it is not complete on it's own, but this is fine, since U-Boot cares only
 * for data packets.
 *
 * Every packet is an individual entity of it's own. Every packet has a start
 * and end packet marker. Data packets come in two flavors - large packets and
 * small packets. Data packets have their own header kermit_data_header_small
 * structure shows how it looks like. Following the header, there is a bunch of
 * data bytes which end with a CRC and end character.
 *
 * The data bytes could create confusing state for kermit protocol, hence the
 * control characters are specially encoded by the protocol. encoding is simple:
 * an escape character is prefixed to characters which are specially encoded.
 *
 * If the reciever gets the packet properly, it acknowledges the receipt of
 * packet. The kermit_ack_nack_type packet describes how it looks like. If a
 * NAK(negative acknowledgement) or the recieved ack packet itself is corrupted,
 * the transmitter retries the old packet.
 *
 * The packet sequence number allows for the sender and reciever to keep track
 * of the packet flow sequence.
 */
/*
 *
 * (C) Copyright 2008-2009
 * Texas Instruments, <www.ti.com>
 * Nishanth Menon <nm@ti.com>
 *
 * (C) Copyright 2000-2004
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifdef __WIN32__
#include <windows.h>		/* for sleep() in dos/mingw */
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "rev.h"
#include "serial.h"
#include "file.h"
#include "f_status.h"

#define XON_CHAR		17
#define XOFF_CHAR		19
#define START_CHAR		0x01
#define ETX_CHAR		0x03
#define END_CHAR		0x0D
#define SPACE			0x20
#define K_ESCAPE		0x23
#define SEND_TYPE		'S'
#define DATA_TYPE		'D'
#define ACK_TYPE		'Y'
#define NACK_TYPE		'N'
#define BREAK_TYPE		'B'
#define tochar(x)		((char) (((x) + SPACE) & 0xff))
#define untochar(x)		((int) (((x) - SPACE) & 0xff))
#define escape_it(ch)		((ch) ^ 0x40 )

#define SEQ_ERROR		0x40
#define CHK_ERROR		0x41

/* Enable Large packet transfers? */
#undef LARGE_PACKETS_ENABLE

#ifdef CONSOLE_TEST
/* Debugging */
#define console_getc() getc(stdin)
#define console_putc(x) printf ("->0x%02X[%c]0x%x\n",(x)&0xFF,(x)&0xFF,\
		((x>=SPACE)?untochar(x):x))
#endif

/* use page sized chunks */
#ifdef LARGE_PACKETS_ENABLE
#define MAX_CHUNK		500
#else
#define MAX_CHUNK		100
#endif
#define RETRY_MAX		4
#define PRINT_SIZE		100

#define PORT_ARG		"p"
#define PORT_ARG_C		'p'
#define DNLD_ARG		"f"
#define DNLD_ARG_C		'f'
#define DLY_ARG			"d"
#define DLY_ARG_C		'd'

#ifdef LARGE_PACKETS_ENABLE
struct kermit_data_header_large {
	unsigned char start;
	unsigned char length_normal;
	unsigned char sequence_number;
	unsigned char packet_type;
	unsigned char length_hi;
	unsigned char length_lo;
	unsigned char header_checksum;
};
#endif

/**
 * Kermit data packet header
 */
struct kermit_data_header_small {
	unsigned char start;
	unsigned char length_normal;
	unsigned char sequence_number;
	unsigned char packet_type;
};

/**
 * Kermit ack packet type
 */
struct kermit_ack_nack_type {
	/** start marker */
	unsigned char start;
	unsigned char length_norm;
	unsigned char sequence_number;
	unsigned char packet_type;
	unsigned char checksum;
	/** end marker */
	unsigned char eol;
};

static unsigned int delay;
/**
 * @brief s1_getpacket - get a packet from the target
 *  s_read is expected to be blocking in this case
 * @param packet  - the buffer to which data is stored
 * @param size  the size of the packet
 */
static void s1_getpacket(char *packet, int size)
{
#ifdef CONSOLE_TEST
	while (size) {
		*packet = console_getc();
		packet++;
		size--;
	}
#else
	memset((void *)packet, 0x00, size);
	if (delay)
#ifndef __WIN32__
		usleep(1000 * delay);
#else
		Sleep(delay);
#endif
	s_read((unsigned char *)packet, size);
#endif
}

/**
 * @brief s1_sendpacket - send a data packet to the target
 *
 * @param packet - buffer containing the data buffer
 * @param size - size of the buffer
 */
static void s1_sendpacket(char *packet, int size)
{
#ifdef CONSOLE_TEST
	while (size) {
		console_putc(*packet);
		packet++;
		size--;
	}
#else
	s_write((unsigned char *)packet, size);
#endif
}

#ifdef DEBUG
/* Converts escaped kermit char to binary char
 * This is from loadb.c in U-Boot
 */
static char ktrans(char in)
{
	if ((in & 0x60) == 0x40) {
		return (char)(in & ~0x40);
	} else if ((in & 0x7f) == 0x3f) {
		return (char)(in | 0x40);
	} else
		return in;
}
#endif

/**
 * @brief should_escape - decides if a binary character should be escaped
 * as per kermit protocol
 *
 * Escape is a concept which prefixes '#' in front of a converted character
 * The idea is to deny CONTROL characters from going over serial port directly.
 * so all characters 0 to 31 and 127 ASCII code is considered control
 * characters
 *
 * @param out - character to be analyzed
 *
 * @return -1 if it should be escaped, else 0
 */
static int should_escape(char out)
{
	char a = out & 0x7F;	/* Get low 7 bits of character */
	/* If data is control prefix, OR
	 * Data is the escape character itself
	 */
	if ((a < SPACE || a == 0x7F)
	    || (a == K_ESCAPE))
		return 1;
	return 0;
}

/**
 * @brief k_escape - provide escape character convertion
 *
 * @param out - character to be escaped
 *
 * @return -escaped character
 */
static char k_escape(char out)
{
	char a = out & 0x7F;	/* Get low 7 bits of character */
	if (a != K_ESCAPE)
		out = escape_it(out);	/* and make character printable. */
	/* Escape character send as is */
	return out;
}

/**
 * @brief k_send_data_packet_small - send a small data packet
 *
 * Kermit protocol allows for two types of data packets ->small and large.
 * This function sends a small packet to the target. the identification of a
 * small packet is if length_normal ==0 in which case length_hi and lo are
 * used from k_large structure.
 *
 * @param buffer - buffer to send
 * @param size -size of the buffer to send
 * @param sequence - sequence number of the transmission.
 *
 * @return success/fail
 */
static signed int k_send_data_packet_small(unsigned char *buffer,
					   unsigned int size,
					   unsigned char sequence)
{
	struct kermit_data_header_small k_small;
	int sum = 0;
	int count = 0;
	int new_size = 0;
	/* Allocation assuming all need escape characters.. */
	char *my_new_buffer = calloc(1, (size * 2) + 3);
	if (my_new_buffer == NULL) {
		APP_ERROR("failed to allocate memory \n")
		    perror(NULL);
		return -1;
	}
	memset(&k_small, 0, sizeof(struct kermit_data_header_small));
	k_small.start = START_CHAR;
	k_small.sequence_number = tochar(sequence);
	k_small.packet_type = DATA_TYPE;
	while (count < size) {

		/* handle kermit escape character in buffer */
		if (should_escape(*(buffer + count))) {
			*(my_new_buffer + new_size) = K_ESCAPE;
			sum += *(my_new_buffer + new_size);
			new_size++;
			*(my_new_buffer + new_size) =
			    k_escape(*(buffer + count));
#ifdef DEBUG
			printf("escape - Ori=0x%x New = 0x%x ReTrans=0x%x\n",
			       (0xFF) & (*(buffer + count)),
			       (0xFF) & *(my_new_buffer + new_size),
			       (0xFF) & ktrans(*(my_new_buffer + new_size)));
#endif
		} else
			*(my_new_buffer + new_size) = (*(buffer + count));

#ifdef DEBUG
		printf("**>[%d]-%d 0x%02x 0x%02x[%c]\n", count, new_size,
		       (char)*(buffer + count),
		       (char)*(my_new_buffer + new_size),
		       (char)*(my_new_buffer + new_size));
#endif
		sum += *(my_new_buffer + new_size);
		count++;
		new_size++;
	}
	/* Add to cover for sequence, packet type and checksum */
	k_small.length_normal = tochar((new_size) + 3);
	sum +=
	    k_small.length_normal + k_small.sequence_number +
	    k_small.packet_type;
	/* Store the checksum for the packet */
	*(my_new_buffer + new_size) =
	    tochar((sum + ((sum >> 6) & 0x03)) & 0x3f);
	new_size++;
	/* Add end character */
	*(my_new_buffer + new_size) = END_CHAR;
	new_size++;
#ifdef DEBUG
	{
		int i = 0;
		char *buf = (char *)&k_small;

		printf("ksmall\n");
		for (i = 0; i < sizeof(k_small); i++) {
			printf("k_small[%d]=0x%x[%c]-%d\n", i, (char)buf[i],
			       (char)buf[i], untochar(buf[i]));
		}
		printf("buffer\n");
		for (i = 0; i < new_size; i++) {
			printf("buff[%d]=0x%02x[%c]-%d\n", i,
			       (char)my_new_buffer[i], (char)my_new_buffer[i],
			       untochar(my_new_buffer[i]));
		}
	}
#endif
	s1_sendpacket((char *)&k_small, sizeof(k_small));
	s1_sendpacket(my_new_buffer, new_size);
	free(my_new_buffer);

	return 0;
}

#ifdef LARGE_PACKETS_ENABLE
/**
 * @brief k_send_data_packet_large - send a large data packet
 *
 * Kermit protocol allows for two types of data packets ->small and large.
 * This function sends a large packet to the target. the identification of a
 * large packet is if length_normal ==0 in which case length_hi and lo are
 * used from k_large structure.
 *
 * @param buffer - buffer to send
 * @param size -size of the buffer to send
 * @param sequence - sequence number of the transmission.
 *
 * @return success/fail
 */
static signed int k_send_data_packet_large(unsigned char *buffer,
					   unsigned int size,
					   unsigned char sequence)
{
	struct kermit_data_header_large k_large;
	int sum = 0;
	int sum_pkt = 0;
	int count = 0;
	int new_size = 0;
	/* Allocation assuming all need escape characters.. */
	char *my_new_buffer = calloc(1, (size * 2) + 3);
	if (my_new_buffer == NULL) {
		APP_ERROR("failed to allocate memory \n")
		    perror(NULL);
		return -1;
	}
	memset(&k_large, 0, sizeof(struct kermit_data_header_large));
	k_large.start = START_CHAR;
	k_large.sequence_number = tochar(sequence);
	k_large.packet_type = DATA_TYPE;
	while (count < size) {

		/* handle kermit escape character in buffer */
		if (should_escape(*(buffer + count))) {
			*(my_new_buffer + new_size) = K_ESCAPE;
			sum += *(my_new_buffer + new_size);
			new_size++;
			*(my_new_buffer + new_size) =
			    k_escape(*(buffer + count));
#ifdef DEBUG
			printf("escape - Ori=0x%x New = 0x%x ReTrans=0x%x\n",
			       (0xFF) & (*(buffer + count)),
			       (0xFF) & *(my_new_buffer + new_size),
			       (0xFF) & ktrans(*(my_new_buffer + new_size)));
#endif
		} else
			*(my_new_buffer + new_size) = (*(buffer + count));

#ifdef DEBUG
		printf("**>[%d]-%d 0x%02x 0x%02x[%c]\n", count, new_size,
		       (char)*(buffer + count),
		       (char)*(my_new_buffer + new_size),
		       (char)*(my_new_buffer + new_size));
#endif
		sum += *(my_new_buffer + new_size);
		count++;
		new_size++;
	}
	/* Add to cover checksum */
	new_size += 1;
	k_large.length_normal = tochar(0x0);
	k_large.length_hi = new_size / 95;
	k_large.length_lo = new_size - (k_large.length_hi * 95);
	k_large.length_hi = tochar(k_large.length_hi);
	k_large.length_lo = tochar(k_large.length_lo);
	sum_pkt =
	    k_large.length_normal + k_large.sequence_number +
	    k_large.packet_type + k_large.length_lo + k_large.length_hi;
	k_large.header_checksum =
	    tochar((sum_pkt + ((sum_pkt >> 6) & 0x03)) & 0x3f);
	new_size -= 1;
	sum += sum_pkt + k_large.header_checksum;
	/* Store the checksum for the packet */
	*(my_new_buffer + new_size) =
	    tochar((sum + ((sum >> 6) & 0x03)) & 0x3f);
	new_size++;
	/* Add end character */
	*(my_new_buffer + new_size) = END_CHAR;
	new_size++;
#ifdef DEBUG
	{
		int i = 0;
		char *buf = (char *)&k_large;

		printf("klarge\n");
		for (i = 0; i < sizeof(k_large); i++) {
			printf("k_large[%d]=0x%x[%c]-%d\n", i, (char)buf[i],
			       (char)buf[i], untochar(buf[i]));
		}
		printf("buffer\n");
		for (i = 0; i < new_size; i++) {
			printf("buff[%d]=0x%02x[%c]-%d\n", i,
			       (char)my_new_buffer[i], (char)my_new_buffer[i],
			       untochar(my_new_buffer[i]));
		}
	}
#endif
	s1_sendpacket((char *)&k_large, sizeof(k_large));
	s1_sendpacket(my_new_buffer, new_size);
	free(my_new_buffer);

	return 0;
}
#endif

/**
 * @brief print_ack_packet - Debug print of the ack packet from target
 *
 * @param k_ack -ack structure
 */
static void print_ack_packet(struct kermit_ack_nack_type *k_ack)
{
	char *buffer = (char *)k_ack;
	int x = 0;
	printf("start = 0x%x\n", k_ack->start);
	printf("length =0x%x %d\n", k_ack->length_norm,
	       untochar(k_ack->length_norm));
	printf("sequence_num = 0x%x[%d]\n", k_ack->sequence_number,
	       untochar(k_ack->sequence_number));
	printf("packet_type = 0x%x[%c]\n", k_ack->packet_type,
	       k_ack->packet_type);
	printf("checksum= 0x%x %d\n", k_ack->checksum,
	       untochar(k_ack->checksum));
	printf("eol = 0x%x\n", k_ack->eol);
	for (x = 0; x < sizeof(struct kermit_ack_nack_type); x++)
		printf("[%d] 0x%x [%c]\n", x, buffer[x], buffer[x]);

}

/**
 * @brief kermit_ack_type - Analyse the ack packet
 *
 * @param seq_num - sequence number expected
 *
 * @return - result
 */
static signed int kermit_ack_type(int seq_num)
{
	struct kermit_ack_nack_type k_ack;
	int sum = 0;
	memset((void *)&k_ack, 0, sizeof(k_ack));
	s1_getpacket((char *)&k_ack, sizeof(k_ack));
	sum = k_ack.length_norm + k_ack.sequence_number + k_ack.packet_type;

	/* Check if this is a valid packet by checking checksum */
	if (k_ack.checksum != tochar((sum + ((sum >> 6) & 0x03)) & 0x3f)) {
#ifdef DEBUG
		APP_ERROR("checksum mismatch\n")
		    print_ack_packet(&k_ack);
#endif
		return CHK_ERROR;
	}
	/* message for the right seq? */
	if (untochar(k_ack.sequence_number) != seq_num) {
		APP_ERROR("sequence_num mismatch. expected [%d] Got %d\n",
			  seq_num, untochar(k_ack.sequence_number))
#ifdef DEBUG
		    print_ack_packet(&k_ack);
#endif
		return SEQ_ERROR;
	}
	if (k_ack.packet_type == NACK_TYPE) {
		APP_ERROR("NACKed!!\n")
		    return NACK_TYPE;
	}
	if (k_ack.packet_type == ACK_TYPE) {
		return ACK_TYPE;
	}
	/* other chars */
	APP_ERROR("Unexpected packet type\n")
	    print_ack_packet(&k_ack);
	return -3;
}

/**
 * @brief k_send_data - send a file to the target using kermit
 *
 * @param f_name - file name to send
 *
 * @return -success/failure
 */
static signed int k_send_data(char *f_name)
{
	unsigned char sequence = 0;
	unsigned int send_size = MAX_CHUNK;
	unsigned char retry = 0;
	int ret = 0;
	signed long ori_size, size = 0;
	char done_transmit = ETX_CHAR;
	char buffer[MAX_CHUNK];

	ori_size = size = f_size(f_name);

	if (size < 0) {
		APP_ERROR("File Size Operation failed! File exists?\n")
		    return size;
	}
	if (f_open(f_name) != FILE_OK) {
		APP_ERROR("File Open failed!File Exists & readable?\n")
		    return -1;
	}
	f_status_init(ori_size, NORMAL_PRINT);
	while (size) {
		send_size = (size > MAX_CHUNK) ? MAX_CHUNK : size;
		send_size = f_read((unsigned char *)buffer, send_size);
		if (send_size < 0) {
			APP_ERROR("Oops.. file read failed!\n")
			    break;
		}
		retry = 0;
		/* we will retry packets to an extent! */
		do {
#ifdef LARGE_PACKETS_ENABLE
			if (send_size < 95)
				ret = k_send_data_packet_small((unsigned char *)
							       buffer,
							       send_size,
							       sequence);
			else
				ret = k_send_data_packet_large((unsigned char *)
							       buffer,
							       send_size,
							       sequence);
#else
			ret = k_send_data_packet_small((unsigned char *)
						       buffer,
						       send_size, sequence);
#endif
			if (ret < 0) {
				APP_ERROR("Failedin send\n")
				    return ret;
			}
			ret = kermit_ack_type(sequence);
			if (ret < 0) {
				APP_ERROR("Failedin ack %d\n", ret)
				    return ret;
			}
			if (ret != ACK_TYPE) {
				retry++;
			}
		} while ((ret != ACK_TYPE) && (retry < RETRY_MAX));
		if (retry == RETRY_MAX) {
			APP_ERROR("Failed after %d retries in sequence %d - "
				  "success send = %ld bytes\n",
				  RETRY_MAX, sequence, (ori_size - size))
			    return -1;
		}
		sequence++;
		/* Roll over sequence number */
		if (sequence > 0x7F)
			sequence = 0;

		size -= send_size;
		f_status_show(ori_size - size);
	}
	/* Send the completion char */
	s1_sendpacket(&done_transmit, 1);
	if (f_close() != FILE_OK) {
		APP_ERROR("File Close failed\n")
	}
	return 0;
}

/**
 * @brief usage help
 *
 * @param appname my application name
 *
 * @return none
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
	       "This Application helps download a file"
	       "using U-Boot's kermit protocol over serial port\n\n"
	       "Syntax:\n"
	       "------\n"
	       "%s -" PORT_ARG " portName -" DNLD_ARG " fileToDownload"
	       " [-" DLY_ARG " delay_time]\n\n"
	       "Where:\n" "-----\n"
	       "portName - RS232 device being used. Example: " PORT_NAME "\n"
	       "fileToDownload - file to be downloaded\n\n"
	       "delay_time - delay time in ms for ack reciept(optional)\n\n"
	       "Usage Example:\n" "-------------\n"
	       "%s -" PORT_ARG " " PORT_NAME " -" DNLD_ARG " " F_NAME "\n",
	       appname, appname);
	REVPRINT();
	LIC_PRINT();
}

/**
 * @brief  application entry
 *
 * @param argc -argument count
 * @param *argv - argument string
 *
 * @return fail/pass
 */
int main(int argc, char **argv)
{
	char *port = NULL;
	char *download_file = NULL;
	char *appname = argv[0];
	int c;
	int ret = 0;
	/* Option validation */
	opterr = 0;

	while ((c =
		getopt(argc, argv,
		       DLY_ARG ":" PORT_ARG ":" DNLD_ARG ":")) != -1)
		switch (c) {
		case DLY_ARG_C:
			sscanf(optarg, "%d", &delay);
			break;
		case PORT_ARG_C:
			port = optarg;
			break;
		case DNLD_ARG_C:
			download_file = optarg;
			break;
		case '?':
			if ((optopt == DNLD_ARG_C) || (optopt == PORT_ARG_C)) {
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
	if ((port == NULL) || (download_file == NULL)) {
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

	s_flush(NULL, NULL);
	ret = k_send_data(download_file);
	if (ret != 0) {
		s_close();
		APP_ERROR("Data transmit failed\n")
		    return ret;
	}
	ret = s_close();
	if (ret != SERIAL_OK) {
		APP_ERROR("serial close failed\n")
		    return ret;
	}
	printf("\nFile Download completed\n");
	return 0;
}
