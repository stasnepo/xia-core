#include "stage_utils.h"

using namespace std;

char fin[32]; //name of file to be retrived by client
int percent = 0; //percentage of chunks predicted correct
int chunknum = 0;
double percentage = 0;
char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];
char ftpServAD[MAX_XID_SIZE];
char ftpServHID[MAX_XID_SIZE];

int stageServerSock;
ofstream stageServerTime("stageServerTime.log");//log file

int chunkSock;
XcacheHandle xcache;

void getConfig(int argc, char** argv)
{
        int c;
        opterr = 0;

        while ((c = getopt(argc, argv, "f:p:")) != -1) {
                switch (c) {
                        //file name
                        case 'f':
                            strcpy(fin, optarg);
                            break;
                        //percentage
                        case 'p':
                            if ((percent = atoi(optarg)) != 0) {
                                percentage = (double)percent / 100;
                            }
                            break;    
                        default:
                            break;
                }
        }
}

void predic_fetch(int sock)
{
    char cmd[XIA_MAX_BUF];
    char reply[XIA_MAX_BUF];
<<<<<<< HEAD
    vector<string> CIDs;

	int n;
    int status = -1;
    char send_to_server_[XIA_MAX_BUF];
    memset(send_to_server_, '\0', strlen(send_to_server_));
    sprintf(send_to_server_, "finish receiveing the current CIDs.");
=======

    vector<string> CIDs;

>>>>>>> 49123213b986a5d5038e8ee0dfcb8b0bb2e99e7d
    // send the file request to the xftp server
    sprintf(cmd, "get %s", fin);
    sendStreamCmd(sock, cmd);
    
    // receive the CID list from xftp server
    while (1) {
        say("In getFile, receive CID list from xftp server.\n");
        memset(reply, '\0', XIA_MAX_BUF);
        if ((n = Xrecv(sock, reply, sizeof(reply), 0)) < 0) {
            Xclose(sock);
            die(-1, "Unable to communicate with the server\n");
        }
        if (strncmp(reply, "cont", 4) == 0) {
            say("Receiving partial chunk list\n");
            char reply_arr[strlen(reply)];
            //char reply_arr[strlen(reply+5)];
            strcpy(reply_arr, reply + 5);
            char *cid;
            cid = strtok(reply_arr, " ");
            CIDs.push_back(cid);
            say("---------CID:%s\n",cid);
            // register CID
            while ((cid = strtok(NULL, " ")) != NULL) {
                CIDs.push_back(cid);
            say("---------CID:%s\n",cid);
            }
        }
        else if (strncmp(reply, "done", 4) == 0) {
            say("Finish receiving all the CIDs\n");
            break;
        }
        sayHello(sock, send_to_server_);
    }

    //fetch the chunk(XCF_CACHE flag will cache the chunk locally)
<<<<<<< HEAD
    for(int i=0; i <= CIDs.size() * percentage;++i){
        sockaddr_x addr;
        Graph g(CIDs[i]);
        g.fill_sockaddr(&addr);
		int ret=0;
=======
    for(int i=0; i <= CIDs.size() * percentage){
        sockaddr_x addr;
        Graph g(CIDs[i]);
        g.fill_sockaddr(&addr);

>>>>>>> 49123213b986a5d5038e8ee0dfcb8b0bb2e99e7d
        if ((ret = XfetchChunk(&xcache, NULL, XCF_BLOCK | XCF_CACHE, &addr, sizeof(addr))) < 0) {
            die(-1, "XfetchChunk Failed\n");
        }
    }
    return;
}
int main()
{
    XcacheHandleInit(&xcache);
    if ((chunkSock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
        die(-1, "unable to create chunk socket\n");
    }
    //stageServerSock = registerStreamReceiver(getStageServiceName(), myAD, myHID, my4ID);
<<<<<<< HEAD
    stageServerSock = initStreamClient(getXftpName(), myAD, myHID, ftpServAD, ftpServHID);	
=======
    ftpSock = initStreamClient(getXftpName(), myAD, myHID, ftpServAD, ftpServHID);	
>>>>>>> 49123213b986a5d5038e8ee0dfcb8b0bb2e99e7d
    say("The current stageServerSock is %d\n", stageServerSock);
    predic_fetch(stageServerSock);

    return 0;
}