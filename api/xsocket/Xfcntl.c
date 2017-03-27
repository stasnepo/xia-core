/* ts=4 */
/*
** Copyright 2011 Carnegie Mellon University
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
/*!
** @file Xfcntl.c
** @brief Xfcntl() - manipulate a socket
*/

#include "Xsocket.h"
/*! \cond */
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include "Xinit.h"
#include "Xutil.h"
/*! \endcond */


/*!
** @brief manipulate an Xsocket
**
** Xfcntl() performs a subset of the fcntl() system call supporting the
** operations described below on the open Xsocket sockfd. The operation
** is determined by cmd.
**
** Xfcntl() can take an optional third argument.  Whether or not this
** argument is required is determined by cmd.  The required argument
** type is an int or void if the argument is not required.
**
** @note See the man page for the standard fcntl() call for more details.
**
** @param sockfd a file descriptor create by Xsocket()
** @param cmd the command to execute
** <br/>F_GETFL: Get the socket's status flags
** <br/>F_SETFL: Set the socket's flags. Currently only O_NONBLOCK is
** supported. Other flags are ignored but do not cause an error.
** @param arg the flag to set if cmd == F_GETFL, otherwise omitted
**
** @returns  socket flags if cmd == F_GETFL
** @returns 0 on success if cmd == F_SETFL
** @returns -1 on error with errno set to an error compatible with those
** returned by the standard fcntl call.
*/
int Xfcntl(int sockfd, int cmd, ...)
{
	int rc = 0;

	if (getSocketType(sockfd) == XSOCK_INVALID) {
		errno = EBADF;
		return -1;
	}

	switch(cmd) {
		case F_GETFL:
			rc = (_f_fcntl)(sockfd, cmd);
			if (rc >= 0 && isBlocking(sockfd))
				rc |= O_NONBLOCK;
			break;

		case F_SETFL:
		{
			va_list args;
			va_start(args, cmd);
			int f = va_arg(args, int);
			va_end(args);

			int block = (f & O_NONBLOCK) ? false : true;

			LOGF("Blocking set to %s", block ? "true" : "false");
			setBlocking(sockfd, block);
			rc = 0;

			if (f & ~O_NONBLOCK) {
				LOGF("unsupported flag(s) found (%s) ignoring...", fcntlFlags(f & ~O_NONBLOCK));
			}
			break;
		}

		case F_GETFD:
			// always say F_CLOEXEC is not enabled for now
			rc = 0;
			break;

		default:
			LOGF("Invalid command specified to Xfcntl: %s", fcntlCmd(cmd));
			rc = -1;
			break;
	}

	return rc;
}

