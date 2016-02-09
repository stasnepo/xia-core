/* ts=4 */
/*
** Copyright 2015 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
** http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <stdint.h> // uint*_t
#include <stdio.h> // printf()
#include <unistd.h> // getopt()
#include <stdarg.h> // va_*()
#include <time.h> // time()
#include <sys/time.h> // gettimeofday()
#include <errno.h> // errno
#include <pthread.h> // pthread_*()
#include <signal.h> // signal()
#include <poll.h> // pollfd
#include <string.h> // strerror()

#include "Xsocket.h"
#include "dagaddr.hpp"

#define VERSION "v1.0"
#define TITLE "XIA Echo Client Speed Test"

#define DGRAM_NAME "www_s.dgram_echo.aaa.xia"

// global configuration options
uint8_t verbose = 0;	// display all messages
unsigned int pktSize = 512;	// default pkt size (bytes)
unsigned int testTime = 180; // default test duration (seconds)
uint8_t terminate = 0;  // set to 1 when it's time to quit
unsigned long long npackets = 0;

struct addrinfo *ai;
sockaddr_x *sa;

/**
 * display cmd line options and exit
 */
void help(const char *name){

	printf("\n%s (%s)\n", TITLE, VERSION);
	printf("usage: %s [-v] -[t time] [-s size]\n", name);
	printf("where:\n");
	printf(" -v : verbose mode\n");
	printf(" -s size : set packet size to <size>, default %u bytes\n", pktSize);
	printf(" -t time : test time, default %u secs\n", testTime);
	printf("\n");
	exit(0);
}

/**
 * configure the app
 */
void getConfig(int argc, char** argv){

	int c;

	opterr = 0;

	while ((c = getopt(argc, argv, "hvt:s:")) != -1) {
		switch (c) {
			case '?':
			case 'h':
				// Help Me!
				help(basename(argv[0]));
				break;
			case 'v':
				// turn off info messages
				verbose = 1;
				break;
			case 't':
				// loop <loops> times and exit
				// if 0, loop forever
				testTime = atoi(optarg);
				break;
			case 's':
				// send pacets of size <size> maximum is 1024
				// if 0, send random sized packets
				pktSize = atoi(optarg);
				if (pktSize < 1) pktSize = 1;
				if (pktSize > XIA_MAXBUF) pktSize = XIA_MAXBUF;
				break;
			default:
				help(basename(argv[0]));
				break;
		}
	}
}

/**
 * write the message to stdout, and exit the app
 */
void die(int ecode, const char *fmt, ...){

	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	fprintf(stdout, "%s: exiting\n", TITLE);
	exit(ecode);
}


/**
 * create a semi-random alphanumeric string of the specified size
 */
char *randomString(char *buf, int size){

	int i;
	static const char *filler = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	static int refresh = 1;
	int samples = strlen(filler);

	if (!(--refresh)) {
		// refresh rand every now and then so it doesn't degenerate too much
		// use a prime number to keep it interesting
		srand(time(NULL));
		refresh = 997;
	}
	for (i = 0; i < size - 1; i ++) {
		buf[i] = filler[rand() % samples];
	}
	buf[size - 1] = 0;

	return buf;
}



/**
 * the main loop thread
 * The parameter and return code are there to satisify the thread library
 */
void *mainLoopThread(void * /*arg*/){

	int ssock;

    if ((ssock = Xsocket(AF_XIA, SOCK_DGRAM, 0)) < 0){
		die(-2, "unable to create the server socket\n");
	}
    
    if (verbose){
        printf("Xsock %4d created\n", ssock);
    }

	if (Xconnect(ssock, (struct sockaddr *)sa, sizeof(sockaddr_x)) < 0){
		die(-3, "unable to connect to the destination dag\n");
    }

    if (verbose){
        printf("Xsock %4d connected\n", ssock);
    }

	char sndBuf[XIA_MAXBUF + 1], rcvBuf[XIA_MAXBUF + 1];
	int nsntBytes=0, nrcvdBytes=0, rc=0;
    
    // set up poll file descriptor
    struct pollfd pfds[2];
	pfds[0].fd = ssock;
	pfds[0].events = POLLIN;

	randomString(rcvBuf, pktSize); // set the snd buffer to a random string

	while (!terminate) { // the main loop proper

        if ((nsntBytes = Xsend(ssock, sndBuf, pktSize, 0)) < 0){
            die(-4, "Send error %d on socket %d\n", errno, ssock);
        }

        if (verbose){
            printf("Xsock %4d sent %d of %d bytes\n", ssock, nsntBytes, \
                pktSize);
        }


        // poll waiting for reply
        if ((rc = Xpoll(pfds, 1, 5000)) <= 0) {
            die(-5, "Poll returned %d\n", rc);
        }

        // read reply
        memset(rcvBuf, 0, sizeof(rcvBuf));
        if ((nrcvdBytes = Xrecv(ssock, rcvBuf, sizeof(rcvBuf), 0)) < 0){
            die(-5, "Receive error %d on socket %d\n", errno, ssock);
        }

        if (verbose){
            printf("Xsock %4d received %d bytes\n", ssock, nrcvdBytes);
        }

        if (nsntBytes != nrcvdBytes || strncmp(sndBuf, rcvBuf, pktSize) != 0){
            printf("Xsock %4d received data different from sent data!\
 (bytes sent/recv'd: %d/%d)\n", ssock, nsntBytes, nrcvdBytes);
        }
        
        npackets++;
	}
    
	Xclose(ssock);
    
    if (verbose){
        printf("Xsock %4d closed\n", ssock);
    }

	return NULL;
}


/**
 * where it all happens
 */
int main(int argc, char **argv){

	srand(time(NULL));
	getConfig(argc, argv);

    if (verbose){
        printf("\n%s (%s): started\n", TITLE, VERSION);
    }

	if (Xgetaddrinfo(DGRAM_NAME, NULL, NULL, &ai)){
		die(-1, "unable to lookup name %s\n", DGRAM_NAME);
    }
    
	sa = (sockaddr_x*)ai->ai_addr;
	Graph g(sa);

    pthread_t mainThreadId;
    struct timeval starTime, endTime;

    if (pthread_create(&mainThreadId, NULL, mainLoopThread, NULL)){
        die(-1, "unable to create thread: %s\n", strerror(errno));
    }

    gettimeofday(&starTime, NULL);

    sleep(testTime); // sleep for the test's duration

    terminate = 1; // the fun is over

    if(pthread_join(mainThreadId, NULL)){
            die(-1, "unable to join thread: %s\n", strerror(errno));
    }

    gettimeofday(&endTime, NULL);

    const uint64_t elapsedTime = \
        (((uint64_t)endTime.tv_sec)*1000000+endTime.tv_usec) - 
        (((uint64_t)starTime.tv_sec)*1000000+starTime.tv_usec);
    
    
    const double throughput = ((double)npackets*1000000)/elapsedTime;
    
    printf("Test complete: %us @ %.2f packets/s\n", testTime, throughput);

	return 0;
}
