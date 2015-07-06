/* ts=4 */
/*
** Copyright 2011 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
/*!
 @file XupdateAD.c
 @brief Implements XreadLocalHostAddr()
*/
#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

#define MAX_RV_DAG_SIZE 1024

int XupdateAD(int sockfd, int interface, char *newnetwork, char *new4id) {
  int rc;

  if (!newnetwork) {
    LOG("new ad is NULL!");
    errno = EFAULT;
    return -1;
  }

  if (getSocketType(sockfd) == XSOCK_INVALID) {
    LOG("The socket is not a valid Xsocket");
    errno = EBADF;
    return -1;
  }

  xia::XSocketMsg xsm;
  xsm.set_type(xia::XCHANGEAD);
  unsigned seq = seqNo(sockfd);
  xsm.set_sequence(seq);

  xia::X_Changead_Msg *x_changead_msg = xsm.mutable_x_changead();
  x_changead_msg->set_interface(interface);
  x_changead_msg->set_dag(newnetwork);
  x_changead_msg->set_ip4id(new4id);

  if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
  }

  // process the reply from click
  if ((rc = click_status(sockfd, seq)) < 0) {
    LOGF("Error getting status from Click: %s", strerror(errno));
    return -1;
  }

  return 0;
}

int XupdateRV(int sockfd)
{
	int rc;
	char rvdag[MAX_RV_DAG_SIZE];
	if(XreadRVServerControlAddr(rvdag, MAX_RV_DAG_SIZE)) {
		// Silently skip rendezvous server update if there is no RV DAG
		//LOG("No rendezvous address, skipping update");
		return 0;
	}

	LOGF("Rendezvous location:%s", rvdag);

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XUPDATERV);

	xia::X_Updaterv_Msg *x_updaterv_msg = xsm.mutable_x_updaterv();
	x_updaterv_msg->set_rvdag(rvdag);

	if((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error asking Click transport to update RV: %s", strerror(errno));
		return -1;
	}
	return 0;
}

/*!
** @brief retrieve the AD and HID associated with this socket.
**
** The HID and AD are assigned by the XIA stack. This call retrieves them
** so that they can be used for creating DAGs or for other purposes in user
** applications.
**
** @param sockfd an Xsocket (may be of any type XSOCK_STREAM, etc...)
** @param localhostNetworkDAG buffer to receive the AD for this host
** @param lenDAG size of the localhostNetworkDAG buffer
** @param localhostHID buffer to receive the HID for this host
** @param lenHID size of the localhostHID buffer
**
** @returns 0 on success
** @returns -1 on failure with errno set
**
*/
int XreadLocalHostAddr(int sockfd, char *localhostNetworkDAG, unsigned lenDAG, char *localhostHID, unsigned lenHID, char *local4ID, unsigned len4ID) {
  	int rc;

 	if (getSocketType(sockfd) == XSOCK_INVALID) {
   	 	LOG("The socket is not a valid Xsocket");
   	 	errno = EBADF;
  		return -1;
 	}

	if (localhostNetworkDAG == NULL || localhostHID == NULL || local4ID == NULL) {
		LOG("NULL pointer!");
		errno = EINVAL;
		return -1;
	}

 	xia::XSocketMsg xsm;
  	xsm.set_type(xia::XREADLOCALHOSTADDR);
  	unsigned seq = seqNo(sockfd);
	xsm.set_sequence(seq);

  	if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
  	}

	xia::XSocketMsg xsm1;
	if ((rc = click_reply(sockfd, seq, &xsm1)) < 0) {
		LOGF("Error retrieving status from Click: %s", strerror(errno));
		return -1;
	}

	if (xsm1.type() == xia::XREADLOCALHOSTADDR) {
		xia::X_ReadLocalHostAddr_Msg *_msg = xsm1.mutable_x_readlocalhostaddr();
		strncpy(localhostNetworkDAG, (_msg->ndag()).c_str(), lenDAG);
		strncpy(localhostHID, (_msg->hid()).c_str(), lenHID);
		strncpy(local4ID, (_msg->ip4id()).c_str(), len4ID);
		// put in null terminators in case buffers were too short
		localhostNetworkDAG[lenDAG - 1] = 0;
		localhostHID[lenHID - 1] = 0;
		local4ID[len4ID - 1] = 0;
		rc = 0;
	} else {
		LOG("XreadlocalHostAddr: ERROR: Invalid response for XREADLOCALHOSTADDR request");
		rc = -1;
	}
	return rc;

}


/*!
** @tell if this node is an XIA-IPv4 dual-stack router
**
**
** @param sockfd an Xsocket (may be of any type XSOCK_STREAM, etc...)
**
** @returns 1 if this is an XIA-IPv4 dual-stack router
** @returns 0 if this is an XIA router
** @returns -1 on failure with errno set
**
*/
int XisDualStackRouter(int sockfd) {
  	int rc;

 	if (getSocketType(sockfd) == XSOCK_INVALID) {
   	 	LOG("The socket is not a valid Xsocket");
   	 	errno = EBADF;
  		return -1;
 	}

 	xia::XSocketMsg xsm;
  	xsm.set_type(xia::XISDUALSTACKROUTER);
  	unsigned seq = seqNo(sockfd);
  	xsm.set_sequence(seq);

  	if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
  	}

	xia::XSocketMsg xsm1;
	if ((rc = click_reply(sockfd, seq, &xsm1)) < 0) {
		LOGF("Error retrieving status from Click: %s", strerror(errno));
		return -1;
	}

	if (xsm1.type() == xia::XISDUALSTACKROUTER) {
		xia::X_IsDualStackRouter_Msg *_msg = xsm1.mutable_x_isdualstackrouter();
		rc = _msg->flag();
	} else {
		rc = -1;
	}
	return rc;

}
