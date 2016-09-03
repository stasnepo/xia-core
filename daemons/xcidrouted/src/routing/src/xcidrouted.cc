#include "xcidrouted.h"

using namespace std;

static int ttl = -1;
static char *ident = NULL;
static char* hostname = NULL;

static XIARouter xr;
static RouteState routeState;

#if defined(LOG) || defined(EVENT_LOG)
static Logger* logger;
#endif

// should CID routing handle topology changes?
// 
// routing uses hard state advertisement to avoid advertisement
// scalability issues. But the use of hard state also means that 
// if in-network state fails, the routing would also fail to achieve
// its purpose. More concretely, topology impact routing decisions and if 
// topology changes, routing decision would be stale. So the best we can
// do is when this happens, CID routing should start over (using soft
// state).

HelloMessage::HelloMessage(){};
HelloMessage::~HelloMessage(){};

string HelloMessage::serialize() const{
	string result;

	result += this->AD + "^";
	result += this->HID + "^";
	result += this->SID + "^";

	return result;
}

void HelloMessage::deserialize(string data) {
	size_t found, start;
	string msg;

	start = 0;
	msg = data;

	found=msg.find("^", start);
  	if (found!=string::npos) {
  		this->AD = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

	found=msg.find("^", start);
  	if (found!=string::npos) {
  		this->HID = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

  	found=msg.find("^", start);
  	if (found!=string::npos) {
  		this->SID = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}
}

void HelloMessage::print() const{
	printf("HelloMessage: \n");
	printf("\tAD: %s HID %s SID %s\n", this->AD.c_str(), this->HID.c_str(), this->SID.c_str());
}

int HelloMessage::send(){
	char buffer[HELLO_MAX_BUF_SIZE];
	int buflen;
	bzero(buffer, HELLO_MAX_BUF_SIZE);

	string msgStr = serialize();
	strcpy (buffer, msgStr.c_str());
	buflen = strlen(buffer);

	// send the broadcast
	if(Xsendto(routeState.helloSock, buffer, buflen, 0, (struct sockaddr*)&routeState.ddag, sizeof(sockaddr_x)) < 0){
		printf("HelloMessage: Xsendto broadcast failed\n");
		return -1;
	}

	return 1;
}

int HelloMessage::recv(){
	int n;
	char recvMessage[HELLO_MAX_BUF_SIZE];
    sockaddr_x theirDAG;
    socklen_t dlen;

    dlen = sizeof(sockaddr_x);
	n = Xrecvfrom(routeState.helloSock, recvMessage, HELLO_MAX_BUF_SIZE, 0, (struct sockaddr*)&theirDAG, &dlen);	
	if (n < 0) {
		printf("Xrecvfrom failed\n");
		return -1;
	} else {
		deserialize(recvMessage);
	}

	return 1;
}

AdvertisementMessage::AdvertisementMessage(){};
AdvertisementMessage::~AdvertisementMessage(){};

string AdvertisementMessage::serialize() const{
	string result;

	result += this->senderHID + "^";
	result += to_string(this->seq) + "^";
	result += to_string(this->ttl) + "^";
	result += to_string(this->distance) + "^";
	
	result += to_string(this->newCIDs.size()) + "^";
	for(auto it = this->newCIDs.begin(); it != this->newCIDs.end(); it++){
		result += *it + "^";
	}

	result += to_string(this->delCIDs.size()) + "^";
	for(auto it = this->delCIDs.begin(); it != this->delCIDs.end(); it++){
		result += *it + "^";
	}

	return result;
}

void AdvertisementMessage::deserialize(string data){
	size_t found, start;
	string msg, senderHID, seq_str, ttl_str, distance_str, num_cids_str, cid_str;

	start = 0;
	msg = data;

	found = msg.find("^", start);
  	if (found != string::npos) {
  		this->senderHID = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

  	found = msg.find("^", start);
  	if (found != string::npos) {
  		seq_str = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  		
  		this->seq = atoi(seq_str.c_str());
  	}

  	found = msg.find("^", start);
  	if (found != string::npos) {
  		ttl_str = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  		
  		this->ttl = atoi(ttl_str.c_str());
  	}

  	found = msg.find("^", start);
  	if (found != string::npos) {
  		distance_str = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  		
  		this->distance = atoi(distance_str.c_str());
  	}

  	found = msg.find("^", start);
  	if (found != string::npos) {
  		num_cids_str = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  		
  		uint32_t num_cids = atoi(num_cids_str.c_str());
  		for (unsigned i = 0; i < num_cids; ++i) {
  			found = msg.find("^", start);
  			if (found != string::npos) {
  				cid_str = msg.substr(start, found-start);
  				start = found+1;  // forward the search point
  			  
  				this->newCIDs.insert(cid_str);
  			}
  		}
  	}

  	found = msg.find("^", start);
  	if (found != string::npos) {
  		num_cids_str = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  		
  		uint32_t num_cids = atoi(num_cids_str.c_str());
  		for (unsigned i = 0; i < num_cids; ++i) {
  			found = msg.find("^", start);
  			if (found != string::npos) {
  				cid_str = msg.substr(start, found-start);
  				start = found+1;  // forward the search point

  				this->delCIDs.insert(cid_str);
  			}
  		}
  	}
}

void AdvertisementMessage::print() const{
	printf("AdvertisementMessage: \n");
	printf("\tsenderHID: %s\n", this->senderHID.c_str());
	printf("\tseq: %d\n", this->seq);
	printf("\tttl: %d\n", this->ttl);
	printf("\tdistance: %d\n", this->distance);

	printf("\tnewCIDs:\n");
	for(auto it = this->newCIDs.begin(); it != this->newCIDs.end(); it++){
		printf("\t\t%s\n", it->c_str());
	}

	printf("\tdelCIDs:\n");
	for(auto it = this->delCIDs.begin(); it != this->delCIDs.end(); it++){
		printf("\t\t%s\n", it->c_str());
	}
}

int AdvertisementMessage::send(int sock){
	if(sock < 0){
		printf("CID advertisement send failed: sock < 0\n");
		return -1;
	}

	printf("sending CID advertisement...\n");

	string advertisement = serialize();
	int sent = -1;
	size_t remaining = strlen(advertisement.c_str()), offset = 0;
	char start[remaining];
	size_t length = htonl(remaining);
	strcpy(start, advertisement.c_str());

	// first send the size of the message
	sent = Xsend(sock, (char*)&length, sizeof(size_t), 0);
	if(sent < 0){
		printf("Xsend send size failed\n");
		return -1;
	}

	// then send the actual message
	while(remaining > 0){
		sent = Xsend(sock, start + offset, remaining, 0);
		if (sent < 0) {
			printf("Xsend failed\n");
			return -1;
		}

		remaining -= sent;
		offset += sent;
	}

	printf("sending raw CID advertisement: %s\n", start);
	printf("sending CID advertisement:\n");
	print();

#ifdef STATS_LOG
	string logStr = "send " + to_string(this->newCIDs.size() + this->delCIDs.size());
	logger->log(logStr.c_str());
#endif

	printf("sent CID advertisement\n");

	return 1;
}

int AdvertisementMessage::recv(int sock){
	int n, to_recv;
	size_t remaining, size;
	char buf[IO_BUF_SIZE];
	string data;

	printf("receiving CID advertisement...\n");

	n = Xrecv(sock, (char*)&remaining, sizeof(size_t), 0);
	if (n < 0) {
		printf("Xrecv failed\n");
		cleanup(0);
	}

	if(remaining <= 0){
		printf("received size have invalid size. Exit\n");
		cleanup(0);
	}

	remaining = ntohl(remaining);
	size = remaining;

	while (remaining > 0) {
		to_recv = remaining > IO_BUF_SIZE ? IO_BUF_SIZE : remaining;

		n = Xrecv(sock, buf, to_recv, 0);
		if (n < 0) {
			printf("Xrecv failed\n");
			return -1;
		} else if (n == 0) {
			break;
		}

		remaining -= n;
		string temp(buf, n);

		data += temp;
	}

	if(data.size() == 0 || data.size() != size){
		printf("received data have invalid size. Exit\n");
		cleanup(0);
	}

	printf("received a raw advertisement message:\n");
	printf("remaining size: %lu, actual received size: %lu\n", size, data.size());
	for(int i = 0; i < (int)data.size(); i++){
		printf("%c", data[i]);
	}
	printf("\n");
	
	deserialize(data);
	print();

#ifdef STATS_LOG
	// log the number of advertised CIDs received.
	string logStr = "recv " + to_string(this->newCIDs.size() + this->delCIDs.size());
	logger->log(logStr.c_str());
#endif

	printf("received CID advertisement\n");

	return 1;
}

NeighborInfo::NeighborInfo(){};
NeighborInfo::~NeighborInfo(){};

void help(const char *name) {
	printf("\nusage: %s [-l level] [-v] [-t] [-h hostname] [-t TTL]\n", name);
	printf("where:\n");
	printf(" -l level    : syslog logging level 0 = LOG_EMERG ... 7 = LOG_DEBUG (default=3:LOG_ERR)\n");
	printf(" -v          : log to the console as well as syslog\n");
	printf(" -h hostname : click device name (default=router0)\n");
	printf(" -t TTL    	 : TTL for the CID advertisement, default is 1\n");
	printf("\n");
	exit(0);
}

void config(int argc, char** argv) {
	int c;
	unsigned level = 3;
	int verbose = 0;

	opterr = 0;

	while ((c = getopt(argc, argv, "h:l:v:t:")) != -1) {
		switch (c) {
			case 'h':
				hostname = strdup(optarg);
				break;
			case 'l':
				level = MIN(atoi(optarg), LOG_DEBUG);
				break;
			case 'v':
				verbose = LOG_PERROR;
				break;
			case 't':
				ttl = atoi(optarg);
				break;
			case '?':
			default:
				// Help Me!
				help(basename(argv[0]));
				break;
		}
	}

	if (!hostname){
		hostname = strdup(DEFAULT_NAME);
	}

	if(ttl == -1 || ttl > MAX_TTL){
		ttl = MAX_TTL;
	}

	printf("ttl is: %d\n", ttl);

	// note: ident must exist for the life of the app
	ident = (char *)calloc(strlen(hostname) + strlen (APPNAME) + 4, 1);
	sprintf(ident, "%s:%s", APPNAME, hostname);
	openlog(ident, LOG_CONS|LOG_NDELAY|verbose, LOG_LOCAL4);
	setlogmask(LOG_UPTO(level));
}

void cleanup(int) {
#if defined(LOG) || defined(EVENT_LOG)
	logger->end();
	delete logger;
#endif

	routeState.mtx.lock();
	for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); it++){
		Xclose(it->second.recvSock);
	}
	routeState.mtx.unlock();

	exit(1);
}

double nextWaitTimeInSecond(double ratePerSecond){
	double currRand = (double)rand()/(double)RAND_MAX;
	double nextTime = -1*log(currRand)/ratePerSecond;	// next time in second
	return nextTime;	
}

int interfaceNumber(string xidType, string xid) {
	int rc;
	vector<XIARouteEntry> routes;
	if ((rc = xr.getRoutes(xidType, routes)) > 0) {
		vector<XIARouteEntry>::iterator ir;
		for (ir = routes.begin(); ir < routes.end(); ir++) {
			XIARouteEntry r = *ir;
			if ((r.xid).compare(xid) == 0) {
				return (int)(r.port);
			}
		}
	}
	return -1;
}

void getRouteEntries(string xidType, vector<XIARouteEntry> & result){
	if(result.size() != 0){
		result.clear();
	}

	int rc;
	vector<XIARouteEntry> routes;
	if ((rc = xr.getRoutes(xidType, routes)) > 0) {
		vector<XIARouteEntry>::iterator ir;
		for (ir = routes.begin(); ir < routes.end(); ir++) {
			XIARouteEntry r = *ir;
			if(strcmp(r.xid.c_str(), ROUTE_XID_DEFAULT) != 0){
				result.push_back(r);
			}
		}
	} else {
		syslog(LOG_ALERT, "unable to get routes from click (%d) %s", rc, xr.cserror());
	}
}

void CIDAdvertiseTimer(){
	thread([](){
    	// sleep for INIT_WAIT_TIME_SEC seconds for hello message to propagate
		this_thread::sleep_for(chrono::seconds(INIT_WAIT_TIME_SEC));
        while (true) {
            double nextTimeInSecond = nextWaitTimeInSecond(CID_ADVERT_UPDATE_RATE_PER_SEC);
            int nextTimeMilisecond = (int) ceil(nextTimeInSecond * 1000);

            advertiseCIDs();

            this_thread::sleep_for(chrono::milliseconds(nextTimeMilisecond));
        }
    }).detach();
}

void cleanCIDRoutes(){
	set<string> toRemove;
#ifdef FILTER
	for(auto it = routeState.CIDRoutesWithFilter.begin(); it != routeState.CIDRoutesWithFilter.end(); it++){
		if(routeState.localCIDs.find(it->first) != routeState.localCIDs.end()){
			toRemove.insert(it->first);
		}
	}

	for(auto it = toRemove.begin(); it != toRemove.end(); it++){
		routeState.CIDRoutesWithFilter.erase(*it);
	}
#else
	for(auto it = routeState.CIDRoutes.begin(); it != routeState.CIDRoutes.end(); it++){
		if(routeState.localCIDs.find(it->first) != routeState.localCIDs.end()){
			toRemove.insert(it->first);
		}
	}

	for(auto it = toRemove.begin(); it != toRemove.end(); it++){
		routeState.CIDRoutes.erase(*it);
	}
#endif
}

void advertiseCIDs(){
	printf("advertising CIDs...\n");

	AdvertisementMessage msg;
	msg.senderHID = routeState.myHID;
	msg.seq = routeState.lsaSeq;
	msg.ttl = ttl;
	msg.distance = 0;

	// C++ socket is generally not thread safe. So talking to click here means we 
	// need to lock it.
	routeState.mtx.lock();
	vector<XIARouteEntry> routeEntries;
	getRouteEntries("CID", routeEntries);
	routeState.mtx.unlock();

	set<string> currLocalCIDs;
	for(unsigned i = 0; i < routeEntries.size(); i++){
		if(routeEntries[i].port == (unsigned short)DESTINED_FOR_LOCALHOST){
			currLocalCIDs.insert(routeEntries[i].xid);
		}
	}
	
	// then find the deleted local CIDs
	for(auto it = routeState.localCIDs.begin(); it != routeState.localCIDs.end(); it++){
		if(currLocalCIDs.find(*it) == currLocalCIDs.end()){
			msg.delCIDs.insert(*it);
		}
	}

	// find all the new local CIDs first
	for(auto it = currLocalCIDs.begin(); it != currLocalCIDs.end(); it++){
		if(routeState.localCIDs.find(*it) == routeState.localCIDs.end()){
			msg.newCIDs.insert(*it);
		}
	}

	routeState.mtx.lock();

#ifdef EVENT_LOG
	if(currLocalCIDs.size() > 0){
		logger->log("Inside advertise CID: ");
		logger->log("Local CIDs: ");
		for(auto it = currLocalCIDs.begin(); it != currLocalCIDs.end(); it++){
			logger->log(*it);
		}
	}
#endif	

	routeState.localCIDs = currLocalCIDs;
	cleanCIDRoutes();
	routeState.mtx.unlock();

#ifdef STATS_LOG
	//log the number of advertised CIDs received.
	string logStr = "local " + to_string(currLocalCIDs.size());
	logger->log(logStr.c_str());
#endif

	// start advertise to each of my neighbors
	if(msg.delCIDs.size() > 0 || msg.newCIDs.size() > 0){
		routeState.mtx.lock();

#ifdef EVENT_LOG
		logger->log("Broadcast TTL: " + to_string(ttl));

		if(msg.newCIDs.size() > 0){
			logger->log("Broadcast newCIDs");
			for(auto it = msg.newCIDs.begin(); it != msg.newCIDs.end(); it++){
				logger->log(*it);
			}
		}
		
		if(msg.delCIDs.size() > 0){
			logger->log("Broadcast delCIDs");
			for(auto it = msg.delCIDs.begin(); it != msg.delCIDs.end(); it++){
				logger->log(*it);
			}
		}
#endif

		for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); it++){

#ifdef EVENT_LOG
			logger->log("Broadcast to neighbor: " + it->second.HID);
#endif
			msg.send(it->second.sendSock);
		}

		routeState.mtx.unlock();

		routeState.lsaSeq = (routeState.lsaSeq + 1) % MAX_SEQNUM;
	}

	printf("done\n");
}

void registerReceiver() {
	int sock;

	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0){
		printf("Unable to create the listening socket\n");
		exit(-1);
	}

	if(XmakeNewSID(routeState.mySID, sizeof(routeState.mySID))) {
		printf("Unable to create a temporary SID\n");
		exit(-1);
	}

	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, routeState.mySID, NULL, &ai) != 0){
		printf("getaddrinfo failure!\n");
		exit(-1);
	}

	sockaddr_x *dag = (sockaddr_x*)ai->ai_addr;

	if (Xbind(sock, (struct sockaddr*)dag, sizeof(dag)) < 0) {
		Xclose(sock);
		printf("xbind failure!\n");
		exit(-1);	
	}

	Xlisten(sock, 5);

	routeState.masterSock = sock;
}

void initRouteState(){
    char cdag[MAX_DAG_SIZE];

	routeState.helloSock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if (routeState.helloSock < 0) {
   		printf("Unable to create a socket\n");
   		exit(-1);
   	}

   	// read the localhost AD and HID
    if (XreadLocalHostAddr(routeState.helloSock, cdag, MAX_DAG_SIZE, routeState.my4ID, MAX_XID_SIZE) < 0 ){
        printf("Reading localhost address\n");
    	exit(0);
    }

    Graph g_localhost(cdag);
    strcpy(routeState.myAD, g_localhost.intent_AD_str().c_str());
    strcpy(routeState.myHID, g_localhost.intent_HID_str().c_str());

	// make the src DAG (the one the routing process listens on)
	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, SID_XCIDROUTE, NULL, &ai) != 0) {
		syslog(LOG_ALERT, "unable to create source DAG");
		exit(-1);
	}
	memcpy(&routeState.sdag, ai->ai_addr, sizeof(sockaddr_x));

	// bind to the src DAG
   	if (Xbind(routeState.helloSock, (struct sockaddr*)&routeState.sdag, sizeof(sockaddr_x)) < 0) {
   		Graph g(&routeState.sdag);
   		syslog(LOG_ALERT, "unable to bind to local DAG : %s", g.dag_string().c_str());
		Xclose(routeState.helloSock);
   		exit(-1);
   	}

   	srand(time(NULL));
   	routeState.lsaSeq = 0;

   	Graph g = Node() * Node(BHID) * Node(SID_XCIDROUTE);
	g.fill_sockaddr(&routeState.ddag);


	// remove any previously set route and start fresh.
	vector<XIARouteEntry> routeEntries;
	getRouteEntries("CID", routeEntries);
	for(unsigned i = 0; i < routeEntries.size(); i++){
		if(routeEntries[i].port != (unsigned short)DESTINED_FOR_LOCALHOST){
			xr.delRouteCIDRouting(routeEntries[i].xid);
		}
	}
}

int connectToNeighbor(string AD, string HID, string SID){
	cout << "connect to neighbor with AD, HID and SID " << AD << " " << HID << " " << SID << endl;

	int sock = -1;
	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0){
		printf("Unable to create the listening socket\n");
		return -1;
	}

	sockaddr_x dag;
	socklen_t daglen = sizeof(sockaddr_x);

	Graph g = Node() * Node(AD) * Node(HID) * Node(SID);
	g.fill_sockaddr(&dag);

	if (Xconnect(sock, (struct sockaddr*)&dag, daglen) < 0) {
		Xclose(sock);
		printf("Unable to connect to the neighbor dag: %s\n", g.dag_string().c_str());
		return -1;
	}

	cout << "Xconnect sent to connect to" << g.dag_string() << endl;

	return sock;
}

void printNeighborInfo(){
	cout << "neighbor info: " << endl;
	for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); it++){
		cout << "\t AD:" << it->second.AD << endl;
		cout << "\t HID:" << it->second.HID << endl;
		cout << "\t port:" << it->second.port << endl;
		cout << "\t sendSock: " << it->second.sendSock << endl;
		cout << "\t recvSock: " << it->second.recvSock << endl;
	}
}

void processHelloMessage(){
	HelloMessage msg;
	msg.recv();

	string key = msg.AD + msg.HID;

	if(routeState.neighbors[key].sendSock == -1) {
		int sock = connectToNeighbor(msg.AD, msg.HID, msg.SID);
		if(sock == -1){
			return;
		}

		routeState.mtx.lock();
		int interface = interfaceNumber("HID", msg.HID);
		routeState.neighbors[key].AD = msg.AD;
		routeState.neighbors[key].HID = msg.HID;
		routeState.neighbors[key].port = interface;
		routeState.neighbors[key].sendSock = sock;
		routeState.mtx.unlock();
	}
}

void processNeighborJoin(){
	int32_t acceptSock = -1;
	string AD, HID;
	sockaddr_x addr;
	socklen_t daglen;
	daglen = sizeof(addr);

	if ((acceptSock = Xaccept(routeState.masterSock, (struct sockaddr*)&addr, &daglen)) < 0){
		printf("Xaccept failed\n");
		return;
	}

	Graph g(&addr);
	// find the AD and HID of the peer
	for(int i = 0; i < g.num_nodes(); i++){
		Node currNode = g.get_node(i);

		if(currNode.type_string() == Node::XID_TYPE_AD_STRING){
            AD = currNode.to_string();
        } else if (currNode.type_string() == Node::XID_TYPE_HID_STRING){
            HID = currNode.to_string();
        }
	}

	string key = AD + HID;

	routeState.mtx.lock();
	int	interface = interfaceNumber("HID", HID);
	routeState.neighbors[key].recvSock = acceptSock;
	routeState.neighbors[key].AD = AD;
	routeState.neighbors[key].HID = HID;
	routeState.neighbors[key].port = interface;
	routeState.mtx.unlock();

	// TODO: handle topology changes
}

void processNeighborLeave(const NeighborInfo &neighbor){
	string key = neighbor.AD + neighbor.HID;
	
	routeState.mtx.lock();
	routeState.neighbors.erase(key);
	routeState.mtx.unlock();

	// TODO: handle topology changes
}

bool checkSequenceAndTTL(const AdvertisementMessage & msg){
	// need to check both the sequence number and ttl since message with lower
	// sequence number but higher ttl need to be propagated further and 
	// we could receive message with lower ttl first.
	if(routeState.HID2Seq.find(msg.senderHID) != routeState.HID2Seq.end() &&
		msg.seq <= routeState.HID2Seq[msg.senderHID] && routeState.HID2Seq[msg.senderHID] - msg.seq < 1000000){	
		// we must have seen this sequence number before since all messages 
		// are sent in order
		if(routeState.HID2Seq[msg.senderHID] - msg.seq >= MAX_SEQ_DIFF 
			|| routeState.HID2Seq2TTL[msg.senderHID][msg.seq] >= msg.ttl){
			return false;
		} else {
			routeState.HID2Seq2TTL[msg.senderHID][msg.seq] = msg.ttl;
		}
	} else {
		routeState.HID2Seq[msg.senderHID] = msg.seq;
		routeState.HID2Seq2TTL[msg.senderHID][msg.seq] = msg.ttl;
	}

	return true;
}

/**
 * Tradeoff between filtering and not-filtering
 * 	filtering: less traffic to propagate especially when redundancy between upstream and downstream router
 * 		is high. But lose valuable information: router might want to keep information of multiple routes
 * 	 	to reach CID
 *
 * 	not-filtering: more traffic; more information available at each router. During route eviction, router can 
 * 		use these information to replace with another longer/equal cost route.
 */

#ifdef FILTER

set<string> deleteCIDRoutesWithFilter(const AdvertisementMessage & msg){
#ifdef EVENT_LOG
	logger->log("Inside deleteCIDRoutesWithFilter");	
#endif

	set<string> routeDeletion;
	for(auto it = msg.delCIDs.begin(); it != msg.delCIDs.end(); it++){
		string currDelCID = *it;

		if(routeState.CIDRoutesWithFilter.find(currDelCID) != routeState.CIDRoutesWithFilter.end()){
			CIDRouteEntry currEntry = routeState.CIDRoutesWithFilter[currDelCID];

			// remove an entry only if it is from the same host and follows the same path
			if(currEntry.dest == msg.senderHID && currEntry.cost == msg.distance){
				routeDeletion.insert(currDelCID);

#ifdef EVENT_LOG
				logger->log("del route for CID: " + currDelCID);
#endif

				routeState.CIDRoutesWithFilter.erase(currDelCID);
				xr.delRouteCIDRouting(currDelCID);
			}
		}

	}

	return routeDeletion;
}

set<string> setCIDRoutesWithFilter(const AdvertisementMessage & msg, const NeighborInfo &neighbor){
#ifdef EVENT_LOG
	logger->log("Inside setCIDRoutesWithFilter");	
#endif

	set<string> routeAddition;
	for(auto it = msg.newCIDs.begin(); it != msg.newCIDs.end(); it++){
		string currNewCID = *it;

		// if the CID is not stored locally
		if(routeState.localCIDs.find(currNewCID) == routeState.localCIDs.end()){
			// and the CID is not encountered before or it is encountered before 
			// but the distance is longer then
			if(routeState.CIDRoutesWithFilter.find(currNewCID) == routeState.CIDRoutesWithFilter.end() 
				|| routeState.CIDRoutesWithFilter[currNewCID].cost > msg.distance){
				routeAddition.insert(currNewCID);

#ifdef EVENT_LOG
				logger->log("set route for CID: " + currNewCID);
				logger->log("set route with new cost: " + to_string(msg.distance) + " new port: " + to_string(neighbor.port) + " nextHop: " + neighbor.HID + 
							" dest: " + msg.senderHID);	
				if(routeState.CIDRoutesWithFilter.find(currNewCID) != routeState.CIDRoutesWithFilter.end()){
					CIDRouteEntry entry = routeState.CIDRoutesWithFilter[currNewCID];
					logger->log("old cost: " + to_string(entry.cost));
					logger->log("old port: " + to_string(entry.port));
					logger->log("old nextHop: " + entry.nextHop);
					logger->log("old dest: " + entry.dest);
				}
#endif	

				// set corresponding CID route entries
				routeState.CIDRoutesWithFilter[currNewCID].cost = msg.distance;
				routeState.CIDRoutesWithFilter[currNewCID].port = neighbor.port;
				routeState.CIDRoutesWithFilter[currNewCID].nextHop = neighbor.HID;
				routeState.CIDRoutesWithFilter[currNewCID].dest = msg.senderHID;
				xr.setRouteCIDRouting(currNewCID, neighbor.port, neighbor.HID, 0xffff);
			}
		}
	}
	return routeAddition;
}

#else

// non-filtering code
void deleteCIDRoutes(const AdvertisementMessage & msg){
	// for each CID routes in the message that would need to be removed
	for(auto it = msg.delCIDs.begin(); it != msg.delCIDs.end(); it++){
		string currDelCID = *it;

		// if CID route entries has this CID from this message sender
		if(routeState.CIDRoutes.find(currDelCID) != routeState.CIDRoutes.end() && 
			routeState.CIDRoutes[currDelCID].find(msg.senderHID) != routeState.CIDRoutes[currDelCID].end()){
			CIDRouteEntry currEntry = routeState.CIDRoutes[currDelCID][msg.senderHID];

			// remove the accounting since this seneder just send a delete message
			// for this CID
			routeState.CIDRoutes[currDelCID].erase(msg.senderHID);

			// if no more CID routes left for this CID from other sender, just remove the CID routes
			if(routeState.CIDRoutes[currDelCID].size() == 0){
				xr.delRouteCIDRouting(currDelCID);
			} else {
				// if there are CID routes left for this CID from other sender, just pick one with the minimum 
				// distance and set it. We can do it since we haven't received eviction message from other sender
				uint32_t minCost = INT_MAX;
				CIDRouteEntry minCostEntry;

				for(auto jt = routeState.CIDRoutes[currDelCID].begin(); jt != routeState.CIDRoutes[currDelCID].end(); jt++){
					if(jt->second.cost < minCost){
						minCost = jt->second.cost;
						minCostEntry = jt->second;
					}
				}

				xr.setRouteCIDRouting(currDelCID, minCostEntry.port, minCostEntry.nextHop, 0xffff);
			}
		}
	}
}

void setCIDRoutes(const AdvertisementMessage & msg, const NeighborInfo &neighbor){
	// then check for each CID if it is the closest for the current router
	for(auto it = msg.newCIDs.begin(); it != msg.newCIDs.end(); it++){
		string currNewCID = *it;

		// if the CID is not stored locally
		if(routeState.localCIDs.find(currNewCID) == routeState.localCIDs.end()){
			// and the CID is not encountered before or it is encountered before 
			// but the distance is longer then
			if(routeState.CIDRoutes.find(currNewCID) == routeState.CIDRoutes.end()) {
				// set corresponding CID route entries
				routeState.CIDRoutes[currNewCID][msg.senderHID].cost = msg.distance;
				routeState.CIDRoutes[currNewCID][msg.senderHID].port = neighbor.port;
				routeState.CIDRoutes[currNewCID][msg.senderHID].nextHop = neighbor.HID;
				routeState.CIDRoutes[currNewCID][msg.senderHID].dest = msg.senderHID;
				xr.setRouteCIDRouting(currNewCID, neighbor.port, neighbor.HID, 0xffff);
			} else {
				uint32_t minDist = INT_MAX;
				for(auto jt = routeState.CIDRoutes[currNewCID].begin(); jt != routeState.CIDRoutes[currNewCID].end(); jt++){
					if(jt->second.cost < minDist){
						minDist = jt->second.cost;
					}
				}

				// only set the routes if current message is the shortest to reach the CID
				if(minDist > msg.distance){
					xr.setRouteCIDRouting(currNewCID, neighbor.port, neighbor.HID, 0xffff);
				}

				if(routeState.CIDRoutes[currNewCID].find(msg.senderHID) == routeState.CIDRoutes[currNewCID].end() 
					|| routeState.CIDRoutes[currNewCID][msg.senderHID].cost > msg.distance){
					routeState.CIDRoutes[currNewCID][msg.senderHID].cost = msg.distance;
					routeState.CIDRoutes[currNewCID][msg.senderHID].port = neighbor.port;
					routeState.CIDRoutes[currNewCID][msg.senderHID].nextHop = neighbor.HID;
					routeState.CIDRoutes[currNewCID][msg.senderHID].dest = msg.senderHID;
				}
			}
		}
	}
}

#endif

void processNeighborMessage(const NeighborInfo &neighbor){
	printf("receive from neighbor AD: %s HID: %s\n", neighbor.AD.c_str(), neighbor.HID.c_str());

	AdvertisementMessage msg;
	int status = msg.recv(neighbor.recvSock);
	if(status < 0){
		printf("message receive failed, remove neighbor\n");
		return;
	}
	
#ifdef EVENT_LOG
	routeState.mtx.lock();

	string logStr = "Received advertisement from neighbor: " + neighbor.HID;
	logger->log(logStr.c_str());

	logStr = "seq: " + to_string(msg.seq) + " ttl: " + to_string(msg.ttl) + " distance: " + to_string(msg.distance);
	logger->log(logStr.c_str());

	logStr = "original sender: " + msg.senderHID;
	logger->log(logStr.c_str());

	logger->log("new cids:");
	for(auto it = msg.newCIDs.begin(); it != msg.newCIDs.end(); it++){
		logger->log((*it).c_str());
	}

	logger->log("del cids:");
	for(auto it = msg.delCIDs.begin(); it != msg.delCIDs.end(); it++){
		logger->log((*it).c_str());
	}

	routeState.mtx.unlock();
#endif

	// check if 
	// 	message is higher sequence number 
	// 	OR lower sequence number but have higher TTL
	if(!checkSequenceAndTTL(msg)){
		return;
	}	

	// check that we are not the originator of this message
	if(msg.senderHID == routeState.myHID){
		return;
	}

	// our communication to XIA writes to single socket and is
	// not thread safe.
	routeState.mtx.lock();

#ifdef EVENT_LOG
	logger->log("passed sequence number check and self loop check");
#endif 	

#ifdef FILTER
	set<string> routeAddition = setCIDRoutesWithFilter(msg, neighbor);
	set<string> routeDeletion = deleteCIDRoutesWithFilter(msg);
#else
	deleteCIDRoutes(msg);
	setCIDRoutes(msg, neighbor);
#endif
	
	routeState.mtx.unlock();

	// update the message and broadcast to other neighbor
	// 	iff there are something meaningful to broadcast
	// 	AND ttl is not going to be zero
#ifdef FILTER
	if(msg.ttl - 1 > 0 && (routeAddition.size() > 0 || routeDeletion.size() > 0)){
#else
	if(msg.ttl - 1 > 0 && (msg.newCIDs.size() > 0 || msg.delCIDs.size() > 0)){
#endif
		AdvertisementMessage msg2Others;
		msg2Others.senderHID = msg.senderHID;
		msg2Others.seq = msg.seq;
		msg2Others.ttl = msg.ttl - 1;
		msg2Others.distance = msg.distance + 1;

#ifdef FILTER
		msg2Others.newCIDs = routeAddition;
		msg2Others.delCIDs = routeDeletion;
#else
		msg2Others.newCIDs = msg.newCIDs;
		msg2Others.delCIDs = msg.delCIDs;
#endif

		routeState.mtx.lock();

#ifdef EVENT_LOG
		logger->log("Relaying message to neighbor");
		string logStr = "seq: " + to_string(msg2Others.seq) + " ttl: " + to_string(msg2Others.ttl) + " distance: " + to_string(msg2Others.distance);
		logger->log(logStr);

		logger->log("new cids:");
		for(auto it = msg2Others.newCIDs.begin(); it != msg2Others.newCIDs.end(); it++){
			logger->log(*it);
		}

		logger->log("del cids:");
		for(auto it = msg2Others.delCIDs.begin(); it != msg2Others.delCIDs.end(); it++){
			logger->log(*it);
		}
#endif

		for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); it++){
			if(it->second.HID != neighbor.HID){

#ifdef EVENT_LOG
				logger->log("relay to neighbor: " + it->second.HID);
#endif

				msg2Others.send(it->second.sendSock);
			}
		}
		routeState.mtx.unlock();
	}
}

int main(int argc, char *argv[]) {
	int rc, selectRetVal, iteration = 0;
	int32_t highSock;
    struct timeval timeoutval;
    fd_set socks;

	// config helper
	config(argc, argv);

	// connect to the click route engine
	if ((rc = xr.connect()) != 0) {
		printf("unable to connect to click (%d)", rc);
		return -1;
	}
	xr.setRouter(hostname);

	initRouteState();
   	registerReceiver();

#if defined(LOG) || defined(EVENT_LOG)
   	logger = new Logger(hostname);
#endif

	// broadcast hello first
   	HelloMessage msg;
	msg.AD = routeState.myAD;
	msg.HID = routeState.myHID;
	msg.SID = routeState.mySID;
	msg.send();

	// start cid advertisement timer
	CIDAdvertiseTimer();

	// clean up resources
	(void) signal(SIGINT, cleanup);

	while(1){
		iteration++;
		if(iteration % 400 == 0){
			msg.send();
		}

		FD_ZERO(&socks);
		FD_SET(routeState.helloSock, &socks);
		FD_SET(routeState.masterSock, &socks);

		highSock = max(routeState.helloSock, routeState.masterSock);
		for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); it++){
			if(it->second.recvSock > highSock){
				highSock = it->second.recvSock;
			}

			if(it->second.recvSock != -1){
				FD_SET(it->second.recvSock, &socks);
			}
		}

		timeoutval.tv_sec = 0;
		timeoutval.tv_usec = 2000;

		selectRetVal = Xselect(highSock+1, &socks, NULL, NULL, &timeoutval);
		if (selectRetVal > 0) {
			if (FD_ISSET(routeState.helloSock, &socks)) {
				// if we received a hello packet from neighbor, check if it is a new neighbor
				// and then establish connection state with them
				processHelloMessage();
			} else if (FD_ISSET(routeState.masterSock, &socks)) {
				// if a new neighbor connects, add to the recv list
				processNeighborJoin();
			} else {
				for(auto it = routeState.neighbors.begin(); it != routeState.neighbors.end(); it++){
					// if we recv a message from one of our established neighbor, procees the message
					if(it->second.recvSock != -1 && FD_ISSET(it->second.recvSock, &socks)){
						processNeighborMessage(it->second);
					}
				}
			}
		}
	}

	return 0;
}