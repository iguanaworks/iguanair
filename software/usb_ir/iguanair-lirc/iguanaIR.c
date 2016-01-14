/****************************************************************************
** hw_iguanaIR.c ***********************************************************
****************************************************************************
*
* routines for interfacing with Iguanaworks USB IR devices
*
* Copyright (C) 2006, Joseph Dunn <jdunn@iguanaworks.net>
*
* Distribute under GPL version 2.
*
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <arpa/inet.h>
#if defined __APPLE__
#include <sys/wait.h>
#include <sys/ioctl.h>
#else
#include <wait.h>
#endif
#include <arpa/inet.h>

#include "lirc_driver.h"

#include "iguanaIR.h"
static const logchannel_t logchannel = LOG_DRIVER;

static int sendConn = -1;
static pid_t child = 0;
static int recvDone = 0;
static int currentCarrier = -1;

static void quitHandler(int sig)
{
	recvDone = 1;
}

static void recv_loop(int fd, int notify)
{
	int conn;

	alarm(0);
	signal(SIGTERM, quitHandler);
	/*    signal(SIGPIPE, SIG_DFL); */
	signal(SIGINT, quitHandler);
	signal(SIGHUP, SIG_IGN);
	signal(SIGALRM, SIG_IGN);

	/* notify parent by closing notify */
	close(notify);

	conn = iguanaConnect(drv.device);
	if (conn != -1) {
		iguanaPacket request, response;
		lirc_t prevCode = -1;

		request = iguanaCreateRequest(IG_DEV_RECVON, 0, NULL);
		if (iguanaWriteRequest(request, conn)) {
			while (!recvDone) {
				/* read from device */
				do
					response = iguanaReadResponse(conn, 1000);
				while (!recvDone && ((response == NULL && errno == ETIMEDOUT)
						     || (iguanaResponseIsError(response) && errno == ETIMEDOUT)));

				if (iguanaResponseIsError(response)) {
					/* be quiet during exit */
					if (!recvDone)
						log_error("error response: %s\n", strerror(errno));
					break;
				} else if (iguanaCode(response) == IG_DEV_RECV) {
					uint32_t* code;
					unsigned int length, x, y = 0;
					lirc_t buffer[8];       /* we read 8 bytes max at a time
								 * from the device, i.e. packet
								 * can only contain 8
								 * signals. */

					/* pull the data off the packet */
					code = (uint32_t*)iguanaRemoveData(response, &length);
					length /= sizeof(uint32_t);

					/* translate the code into lirc_t pulses (and make
					 * sure they don't split across iguana packets. */
					for (x = 0; x < length; x++) {
						if (prevCode == -1) {
							prevCode = (code[x] & IG_PULSE_MASK);
							if (prevCode > PULSE_MASK)
								prevCode = PULSE_MASK;
							if (code[x] & IG_PULSE_BIT)
								prevCode |= PULSE_BIT;
						} else if (((prevCode & PULSE_BIT) && (code[x] & IG_PULSE_BIT))
							   || (!(prevCode & PULSE_BIT) && !(code[x] & IG_PULSE_BIT))) {
							/* can overflow pulse mask, so just set to
							 * largest possible */
							if ((prevCode & PULSE_MASK) + (code[x] & IG_PULSE_MASK) >
							    PULSE_MASK)
								prevCode = (prevCode & PULSE_BIT) | PULSE_MASK;
							else
								prevCode += code[x] & IG_PULSE_MASK;
						} else {
							buffer[y] = prevCode;
							y++;

							prevCode = (code[x] & IG_PULSE_MASK);
							if (prevCode > PULSE_MASK)
								prevCode = PULSE_MASK;
							if (code[x] & IG_PULSE_BIT)
								prevCode |= PULSE_BIT;
						}
					}

					/* write the data and free it */
					if (y > 0) {
						chk_write(fd,
							  buffer,
							  sizeof(lirc_t) * y);
					}
					free(code);
				}

				iguanaFreePacket(response);
			}
		}

		iguanaFreePacket(request);
	}

	iguanaClose(conn);
	close(fd);
}

static int iguana_init(void)
{
	int recv_pipe[2], retval = 0;

	rec_buffer_init();

	if (pipe(recv_pipe) != 0) {
		log_error("couldn't open pipe: %s", strerror(errno));
	} else {
		int notify[2];

		if (pipe(notify) != 0) {
			log_error("couldn't open pipe: %s", strerror(errno));
			close(recv_pipe[0]);
			close(recv_pipe[1]);
		} else {
			drv.fd = recv_pipe[0];

			child = fork();
			if (child == -1) {
				log_error("couldn't fork child process: %s", strerror(errno));
			} else if (child == 0) {
				close(recv_pipe[0]);
				close(notify[0]);
				recv_loop(recv_pipe[1], notify[1]);
				_exit(0);
			} else {
				int dummy;

				close(recv_pipe[1]);
				close(notify[1]);
				/* make sure child has set its signal handler to avoid race with iguana_deinit() */
				chk_read(notify[0], &dummy, 1);
				close(notify[0]);
				sendConn = iguanaConnect(drv.device);
				if (sendConn == -1) {
					log_error("couldn't open connection to iguanaIR daemon: %s",
						  strerror(errno));
				} else {
					retval = 1;
				}
			}
		}
	}

	return retval;
}

static pid_t dowaitpid(pid_t pid, int* stat_loc, int options)
{
	pid_t retval;

	do
		retval = waitpid(pid, stat_loc, options);
	while (retval == (pid_t)-1 && errno == EINTR);

	return retval;
}

static int iguana_deinit(void)
{
	/* close the connection to the iguana daemon */
	if (sendConn != -1) {
		iguanaClose(sendConn);
		sendConn = -1;
	}

	/* signal the child process to exit */
	if (child > 0 && (kill(child, SIGTERM) == -1 || dowaitpid(child, NULL, 0) != (pid_t)-1))
		child = 0;

	/* close drv.fd since otherwise we leak open files */
	close(drv.fd);
	drv.fd = -1;

	return child == 0;
}

static char* iguana_rec(struct ir_remote* remotes)
{
	char* retval = NULL;

	if (rec_buffer_clear())
		retval = decode_all(remotes);
	return retval;
}

static bool daemonTransaction(unsigned char code, void* value, size_t size)
{
	uint8_t* data;
	bool retval = false;

	data = (uint8_t*)malloc(size);
	if (data != NULL) {
		iguanaPacket request, response = NULL;

		memcpy(data, value, size);
		request = iguanaCreateRequest(code, size, data);
		if (request) {
			if (iguanaWriteRequest(request, sendConn))
				response = iguanaReadResponse(sendConn, 10000);
			iguanaFreePacket(request);
		} else {
			free(data);
		}

		/* handle success */
		if (!iguanaResponseIsError(response))
			retval = true;
		iguanaFreePacket(response);
	}
	return retval;
}

static int iguana_send(struct ir_remote* remote, struct ir_ncode* code)
{
	int retval = 0;
	uint32_t freq;

	/* set the carrier frequency if necessary */
	freq = htonl(remote->freq);
	if (remote->freq != currentCarrier && remote->freq >= 25000 && remote->freq <= 100000
	    && daemonTransaction(IG_DEV_SETCARRIER, &freq, sizeof(freq)))
		currentCarrier = remote->freq;

	if (send_buffer_put(remote, code)) {
		int length, x;
		const lirc_t* signals;
		uint32_t* igsignals;

		length = send_buffer_length();
		signals = send_buffer_data();

		igsignals = (uint32_t*)malloc(sizeof(uint32_t) * length);
		if (igsignals != NULL) {
			iguanaPacket request, response = NULL;

			/* must pack the data into a unit32_t array */
			for (x = 0; x < length; x++) {
				igsignals[x] = signals[x] & PULSE_MASK;
				if (signals[x] & PULSE_BIT)
					igsignals[x] |= IG_PULSE_BIT;
			}

			/* construct a request and send it to the daemon
			 * TRICKY: IguanaFreePacket free()'s both the
			 * igsignals  chunk and the request packet, but
			 * iguanaCreateRequest does not malloc that chunk.
			 */
			request = iguanaCreateRequest(IG_DEV_SEND, sizeof(uint32_t) * length, igsignals);
			if (iguanaWriteRequest(request, sendConn)) {
				/* response will only come back after the device has
				 * transmitted */
				response = iguanaReadResponse(sendConn, 10000);
				if (!iguanaResponseIsError(response))
					retval = 1;

				iguanaFreePacket(response);
			}

			/* free the packet and the data */
			iguanaFreePacket(request);
		}
	}

	return retval;
}

static int iguana_ioctl(unsigned int code, void* arg)
{
	int retcode = -1;
	uint8_t channels = *(uint8_t*)arg;

	/* set the transmit channels: return 0 on success, 4 if
	 * out-of-range (see ioctl) */
	if (code == LIRC_SET_TRANSMITTER_MASK) {
		if (channels > 0x0F)
			retcode = 4;
		else if (daemonTransaction(IG_DEV_SETCHANNELS, &channels, sizeof(channels)))
			retcode = 0;
	}

	return retcode;
}

static lirc_t readdata(lirc_t timeout)
{
	lirc_t code = 0;
	struct pollfd pfd = {.fd = drv.fd, .events = POLLIN, .revents = 0};
	/* attempt a read with a timeout using select */
	if (poll(&pfd, 1, timeout / 1000) > 0) {
		/* if we failed to get data return 0 */
		if (read(drv.fd, &code, sizeof(lirc_t)) <= 0)
			iguana_deinit();
	}
	return code;
}

const struct driver hw_iguanaIR = {
	.name		= "iguanaIR",
	.device		= "0",
	.features	= LIRC_CAN_REC_MODE2 | \
			  LIRC_CAN_SEND_PULSE | \
			  LIRC_CAN_SET_SEND_CARRIER | \
			  LIRC_CAN_SET_TRANSMITTER_MASK,
	.send_mode	= LIRC_MODE_PULSE,
	.rec_mode	= LIRC_MODE_MODE2,
	.code_length	= sizeof(int),
	.init_func	= iguana_init,
	.deinit_func	= iguana_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= iguana_send,
	.rec_func	= iguana_rec,
	.decode_func	= receive_decode,
	.drvctl_func	= iguana_ioctl,
	.readdata	= readdata,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "See file://" PLUGINDOCS "/iguanair.html",
	.device_hint    = "/var/run/iguanaIR/*",
};

const struct driver* hardwares[] = { &hw_iguanaIR, (const struct driver*)NULL };
