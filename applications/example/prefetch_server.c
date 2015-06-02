#include "prefetch_utils.h"

using namespace std;

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];

char prefetchProfileAD[MAX_XID_SIZE];
char prefetchProfileHID[MAX_XID_SIZE];

char prefetchPredAD[MAX_XID_SIZE];
char prefetchPredHID[MAX_XID_SIZE];

char prefetchExecAD[MAX_XID_SIZE];
char prefetchExecHID[MAX_XID_SIZE];

char ftpServAD[MAX_XID_SIZE];
char ftpServHID[MAX_XID_SIZE];

char prefetch_profile_name[] = "www_s.profile.prefetch.aaa.xia";
char ftp_name[] = "www_s.ftp.advanced.aaa.xia";


// TODO: optimize sleep(1)
// TODO: netMon function to update prefetching window
// Question: how many chunks can API support to get in one time? 12; can we stopping retransmit request in application

int profileServerSock, ftpSock;

pthread_mutex_t profileLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t windowLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t bufLock = PTHREAD_MUTEX_INITIALIZER;

#define BLANK 0	// initilized
#define REQ 1 	// requested by router
#define DONE 2	// available in cache

struct chunkStatus {
	string CID;
	long timestamp; // msec
	bool fetch;
	int prefetch;
};

map<string, vector<chunkStatus> > profile;
map<string, int> window; // prefetching window
map<string, vector<string> > buf; // chunk buffer to prefetch

int prefetch_chunk_num = 3; // default number of chunks to prefetch

void reg_handler(int sock, char *cmd) {
	// format: reg cid_num cid1 cid2 ...
	char cmd_arr[strlen(cmd)];
	strcpy(cmd_arr, cmd);
	string SID = XgetRemoteSID(sock); // get client ftp's SID
	cerr<<"Peer SID: "<<SID<<endl; 	
	cout<<cmd_arr<<endl;
	strtok(cmd_arr, " "); // skip the "reg"
	int cid_num = atoi(strtok(NULL, " "));
	vector<chunkStatus> css;
	for (int i = 0; i < cid_num; i++) {
		chunkStatus cs;
		cs.CID = strtok(NULL, " ");
		cs.timestamp = now_msec(); 
		cs.fetch = false;
		cs.prefetch = BLANK;
		css.push_back(cs);
	}
	pthread_mutex_lock(&profileLock);	
	profile[SID] = css;
	pthread_mutex_unlock(&profileLock);

	say("\nPrint profile table in reg_handler\n");

	for (map<string, vector<chunkStatus> >::iterator I = profile.begin(); I != profile.end(); ++I) {
		for (vector<chunkStatus>::iterator J = (*I).second.begin(); J != (*I).second.end(); ++J) {
			cerr<<(*I).first<<"\t"<<(*J).CID<<"\t"<<(*J).timestamp<<"\t"<<(*J).fetch<<"\t"<<(*J).prefetch<<endl;
		}
	}	
	say("\n");

	pthread_mutex_lock(&windowLock);	
	window[SID] = prefetch_chunk_num;
	pthread_mutex_unlock(&windowLock);

	vector<string> CIDs;

	pthread_mutex_lock(&windowLock);	
	buf[SID] = CIDs;
	pthread_mutex_unlock(&windowLock);

	cerr<<"Finish reg"<<endl;
	return;
}

void poll_handler(int sock, char *cmd) {

	char cmd_arr[strlen(cmd)];
	strcpy(cmd_arr, cmd);
	string SID = XgetRemoteSID(sock);
	cerr<<"Peer SID: "<<SID<<endl; 
	strtok(cmd_arr, " "); // skip the "fetch"
	string CID = strtok(NULL, " ");
	cerr<<CID<<endl;
	char reply[XIA_MAX_BUF] = "available";
	cerr<<"Reply msg is initialized: "<<reply<<endl;
	// reply format: available cid1 cid2, ...
	pthread_mutex_lock(&profileLock);
	say("Lock the profile\n");	

	unsigned int p;
	// return the CIDs which chunks are available 
	for (p = 0; p < profile[SID].size(); p++) {
		if (profile[SID][p].CID == CID) 
			break;
	}
	cerr<<p<<endl;
	unsigned int c = 0;

	for (unsigned i = p; i < profile[SID].size(); i++) {
		// mark fetch as "true" when the chunk is available after probe				
		if (profile[SID][i].prefetch == DONE) {
			profile[SID][i].fetch = true;
			strcat(reply, " ");										
			strcat(reply, string2char(profile[SID][i].CID));
			c++;
		}
	}

	if (c == 0) 
		strcat(reply, " none");	

	cerr<<reply<<endl;

	pthread_mutex_lock(&bufLock);
	say("Lock the buf\n");

	for (unsigned int p = 0; p < profile[SID].size(); p++) {
		// find the first one not fetched by client yet		
		if (profile[SID][p].fetch == false) 
			break;
	}

	for (unsigned int i = p; i < p + window[SID]; i++) {
		if (profile[SID][i].prefetch == 0) { 
			buf[SID].push_back(profile[SID][i].CID);
			profile[SID][i].prefetch = REQ; // marked as requested by router
		}
	}

	say("\nPrint profile table in poll_handler\n");
	for (map<string, vector<chunkStatus> >::iterator I = profile.begin(); I != profile.end(); ++I) {
		for (vector<chunkStatus>::iterator J = (*I).second.begin(); J != (*I).second.end(); ++J) {
			cerr<<(*I).first<<"\t"<<(*J).CID<<"\t"<<(*J).timestamp<<"\t"<<(*J).fetch<<"\t"<<(*J).prefetch<<endl;
		}
	}	
	say("\n");

	cerr<<SID<<": the chunks are pushed into the queue:\n";
	for (unsigned i = 0; i < buf[SID].size(); i++)
		cerr<<buf[SID][i]<<endl;

	pthread_mutex_unlock(&bufLock);	
	say("Unlock the buf\n");
	pthread_mutex_unlock(&profileLock);
	say("Unlock the profile\n");
	sendCmd(sock, reply);
	return;
}

void *clientReqCmd (void *socketid) {

	char cmd[XIA_MAXBUF];
	char reply[XIA_MAXBUF];
	int sock = *((int*)socketid);
	int n;

	while (1) {
		memset(cmd, '\0', strlen(cmd));
		memset(reply, '\0', strlen(reply));
		if ((n = Xrecv(sock, cmd, sizeof(cmd), 0))  < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		}
		// reg msg from xftp client
		if (strncmp(cmd, "reg", 3) == 0) {
			say("Receive a reg message\n");
			reg_handler(sock, cmd);
			/*
			say("\nPrint profile table after reg_handler\n");
			for (map<string, vector<chunkStatus> >::iterator I = profile.begin(); I != profile.end(); ++I) {
				for (vector<chunkStatus>::iterator J = (*I).second.begin(); J != (*I).second.end(); ++J) {
					cerr<<(*I).first<<"\t"<<(*J).CID<<"\t"<<(*J).timestamp<<"\t"<<(*J).fetch<<"\t"<<(*J).prefetch<<endl;
				}
			}	
			say("\n");*/
		}
		// fetch probe from xftp client: fetch CID
		else if (strncmp(cmd, "poll", 4) == 0) {
			say("Receive a polling message\n");
			poll_handler(sock, cmd);
			/*
			say("\nPrint after poll_handler\n");
			for (map<string, vector<chunkStatus> >::iterator I = profile.begin(); I != profile.end(); ++I) {
				for (vector<chunkStatus>::iterator J = (*I).second.begin(); J != (*I).second.end(); ++J) {
					cerr<<(*I).first<<"\t"<<(*J).CID<<"\t"<<(*J).timestamp<<"\t"<<(*J).fetch<<"\t"<<(*J).prefetch<<endl;
				}
			}
			say("\n"); */
		}	
	}
	Xclose(sock);
	say("Socket closed\n");
	pthread_exit(NULL);
}

// TODO: netMon
void *prefetchPred(void *) {
	// go through all the SID
	while (1) {
		prefetch_chunk_num = 3;		
	}
	pthread_exit(NULL);
}

void *prefetchExec(void *) {

	while (1) {
		say("Pulling the chunks from buf...\n");
		// pop out the chunks for prefetching 
		pthread_mutex_lock(&bufLock);	

		for (map<string, vector<string> >::iterator I = buf.begin(); I != buf.end(); ++I) {
			if ((*I).second.size() > 0) {
				string SID = (*I).first;
				cerr<<"SID: "<<SID<<endl;
				say("Here are the chunks in the buf\n");
				for (unsigned int i = 0; i < (*I).second.size(); i++) 
					cerr<<(*I).second[i]<<endl;

				int chunkSock;

				if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
					die(-1, "unable to create chunk socket\n");

				ChunkStatus cs[(*I).second.size()];
				char data[XIA_MAXCHUNK];
				int len;
				int status;
				int n = (*I).second.size();

				for (unsigned int i = 0; i < (*I).second.size(); i++) {
					char *dag = (char *)malloc(512);
					sprintf(dag, "RE ( %s %s ) CID:%s", ftpServAD, ftpServHID, string2char((*I).second[i]));
					cs[i].cidLen = strlen(dag);
					cs[i].cid = dag; // cs[i].cid is a DAG, not just the CID
				}

				unsigned ctr = 0;

				while (1) {

					// Retransmit chunks request every REREQUEST seconds if not ready
					if (ctr % REREQUEST == 0) {
						// bring the list of chunks local
						say("%srequesting list of %d chunks\n", (ctr == 0 ? "" : "re-"), n);
						if (XrequestChunks(chunkSock, cs, n) < 0) {
							say("unable to request chunks\n");
							pthread_exit(NULL); // TODO: check again
						}
						say("checking chunk status\n");
					}
					ctr++;

					status = XgetChunkStatuses(chunkSock, cs, n);

					if (status == READY_TO_READ)
						break;
					else if (status < 0) 
						die(-1, "error getting chunk status\n"); 
					else if (status & WAITING_FOR_CHUNK) 
						say("waiting... one or more chunks aren't ready yet\n");
					else if (status & INVALID_HASH) 
						die(-1, "one or more chunks has an invalid hash");
					else if (status & REQUEST_FAILED)
						die(-1, "no chunks found\n");
					else 
						say("unexpected result\n");

					sleep(1); 
				}

				// update DONE to profile
	  		pthread_mutex_lock(&profileLock);

				for (unsigned int i = 0; i < (*I).second.size(); i++) {
					if ((len = XreadChunk(chunkSock, data, sizeof(data), 0, cs[i].cid, cs[i].cidLen)) < 0) {
						say("error getting chunk\n");
						pthread_exit(NULL); // TODO: check again
					}
					else {
						for (unsigned int j = 0; j < profile[SID].size(); j++) {
							if (profile[SID][j].CID == (*I).second[i]) 
								profile[SID][j].prefetch = DONE;
						}							
					}
				}

	 	 		pthread_mutex_unlock(&profileLock);

				(*I).second.clear();
				// TODO: timeout the chunks by free(cs[i].cid); cs[j].cid = NULL; cs[j].cidLen = 0;			
			}
		}
		pthread_mutex_unlock(&bufLock);
		sleep(1);
	}
	pthread_exit(NULL);
}

int main() {

	pthread_t thread_pred, thread_exec;
	//pthread_create(&thread_pred, NULL, prefetchPred, NULL);	// TODO: 
	pthread_create(&thread_exec, NULL, prefetchExec, NULL); // dequeue, prefetch and update profile

	ftpSock = initializeClient(ftp_name, myAD, myHID, ftpServAD, ftpServHID); // purpose is to get ftpServAD and ftpServHID for building chunk request

	profileServerSock = registerStreamReceiver(prefetch_profile_name, myAD, myHID, my4ID);
	blockListener((void *)&profileServerSock, clientReqCmd);
	return 0;	

}
