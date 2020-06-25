#include "router.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>

// function that let one router send Init packet to network emulator
static int sendInitPkt(unsigned int router_id, int routerFileDescriptor, struct sockaddr_in *host_addr, FILE *logFile) {
  // create INIT packet
  struct pkt_INIT initPkt = {router_id};
  fprintf(logFile, "R%u sends an INIT: router_id %u\n", router_id, router_id);
  // send to network emulator
	return sendto(routerFileDescriptor, &initPkt, sizeof(initPkt), 0, (struct sockaddr*) host_addr, sizeof(struct sockaddr_in));
}

// function that let one router send Hello packet to other router given link_id
static int sendHelloPkt(unsigned int router_id, unsigned int link_id, int routerFileDescriptor, struct sockaddr_in *host_addr, FILE *logFile) {
  // create HELLO packet with specific link_id
  struct pkt_HELLO helloPkt = {router_id, link_id};
  fprintf(logFile, "R%u sends a HELLO: router_id %u, link_id %u\n", router_id, router_id, link_id);
  // send to network emulator
	return sendto(routerFileDescriptor, &helloPkt, sizeof(helloPkt), 0, (struct sockaddr*) host_addr, sizeof(struct sockaddr_in));
}

// function that let one router send it's link state pdus to other router given link_id
static void sendLSPDUs(unsigned int router_id, unsigned int link_id, int routerFileDescriptor, struct circuit_DB database[NBR_ROUTER], struct sockaddr_in *host_addr, FILE *logFile) {
  // create LSPDU packet
  struct pkt_LSPDU lspduPkt;
  lspduPkt.sender = router_id; // set the sender field

  // loop through each link state information in the current sender's database
  for (unsigned int i = 0; i < database[router_id - 1].nbr_link; i++) {
    lspduPkt.via = database[router_id - 1].linkcost[i].link;
    // if the current LS id matches the target, send all LS PDUs
    if (lspduPkt.via == link_id) {
      for (unsigned int j = 0; j < NBR_ROUTER; j++) {
        lspduPkt.router_id = j + 1;
        for (unsigned int m = 0; m < database[j].nbr_link; m++) {
          struct link_cost currentLink = database[j].linkcost[m];
          lspduPkt.link_id = currentLink.link;
          lspduPkt.cost = currentLink.cost;
          fprintf(logFile, "R%u sends a LS PDU: sender %u, router_id %u, link_id %u, cost %d, via %u\n", 
            router_id, lspduPkt.sender, lspduPkt.router_id, lspduPkt.link_id, lspduPkt.cost, lspduPkt.via);
          sendto(routerFileDescriptor, &lspduPkt, sizeof(lspduPkt), 0, (struct sockaddr*) host_addr, sizeof(struct sockaddr_in));
        }
      }
    }
  }
}

// help function that turns the current topology database into a set of adjacency list for each router (represented by 2d array)
// Each row of the adjacency matrix and cost matrix corresponds to one router.
// Adjacency matrix tells neighbours infomation and cost matrix stores the link cost
// This is to help simplify the Dijkstra's Algorithm implementation
static void getAdjancecyMatrix(struct circuit_DB database[NBR_ROUTER], bool adjacency[NBR_ROUTER][NBR_ROUTER], unsigned int cost[NBR_ROUTER][NBR_ROUTER]) {
  int link[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1}; // maximum 10 links as maximum 5 routers
	int currentLinkID, targetRouter; // local variable

  // initialize adjancecy matrix and cost matrix to zero
	for (unsigned int i = 0; i < NBR_ROUTER; i++) {
		for (unsigned int j = 0; j < NBR_ROUTER; j++) {
			adjacency[i][j] = false;
			cost[i][j] = 0;
		}
	}
	
  // update two matrices
	for (unsigned int i = 0; i < NBR_ROUTER; i++) {
		for (unsigned int j = 0; j < database[i].nbr_link; j++) {
			currentLinkID = database[i].linkcost[j].link - 1;
			targetRouter = link[currentLinkID];
			if (targetRouter != -1) { // if the link is found before, we update information
				adjacency[i][targetRouter] = adjacency[targetRouter][i] = true;
				cost[i][targetRouter] = cost[targetRouter][i] = database[i].linkcost[j].cost;
			} else { // never saw this link before, set one end of this link to current router id
				link[currentLinkID] = i;
			}
		}
	}
}

// implementation of shortest Path algorithm(Dijkstra's algorithm)
static void shortestPath(unsigned int router_id, struct circuit_DB database[NBR_ROUTER], FILE *logFile) {
	unsigned int distance[NBR_ROUTER] = {0};
	int previous[NBR_ROUTER] = {-1, -1, -1, -1, -1};
	// instead of directly using the topology database, we use a converted adjacency matrix to simplify the code
	bool adjacency[NBR_ROUTER][NBR_ROUTER];
	unsigned int cost[NBR_ROUTER][NBR_ROUTER];
	getAdjancecyMatrix(database, adjacency, cost);

  // initialize distance and previous array based on current links
	for (unsigned int i = 0; i < NBR_ROUTER; i++) {
		if (adjacency[router_id - 1][i]) {
			distance[i] = cost[router_id - 1][i];
			previous[i] = router_id - 1;
		} else {
			distance[i] = INF;
		}
	}
	distance[router_id - 1] = 0; // set local distance to 0

	int existing = NBR_ROUTER; // available routers
	bool erased[NBR_ROUTER] = {false}; // c
	
	while (existing > 0) {
		unsigned int minDistance = INF;
		int next = -1; // the router that has the smallest distance from current router

		// find the path that has the minimum distance
		for (unsigned int i = 0; i < NBR_ROUTER; i++) {
			if (distance[i] <= minDistance && !erased[i]) {
				next = i;
				minDistance = distance[i];
			}
		}
		existing--;
		if (next == -1) break; // no path found any more, break loop
		erased[next] = true;

    // update subpath
		for (unsigned int i = 0; i < NBR_ROUTER; i++) {
			if (adjacency[i][next] && !erased[i]) {
			  unsigned int alt = distance[next] + cost[i][next];
				if (alt < distance[i]) {
					distance[i] = alt;
					previous[i] = next;
				}
			}
		}
	}

	// print RIB
	fprintf(logFile, "# RIB\n");
	for (unsigned int i = 0; i < NBR_ROUTER; i++) {
		if (i == router_id - 1) { // print local information
			fprintf(logFile, "R%u -> R%u -> (Local), 0\n", router_id, router_id);
		} else {
      if (distance[i] != INF) { // normal print, need to find second router in the path
        int curr = i, prev = i;
			  while (true) {
				  curr = previous[curr];
				  if ((unsigned int)curr == router_id - 1 || curr == -1) break; // find the router, break
				  prev = curr;
			  }
        fprintf(logFile, "R%d -> R%d -> R%d, %d\n", router_id, i + 1, prev + 1, distance[i]);
      } else { // print infinite distance information
        fprintf(logFile, "R%u -> R%u -> INF, INF\n", router_id, i + 1);
      }
		}
	}
	fprintf(logFile, "\n");
}

// check if the topology database should update based on the receiving LSPDU packet
// return true if needs update(update is done in the function), otherwise return false
static bool updateDatabase(struct circuit_DB database[NBR_ROUTER], struct pkt_LSPDU *lspduPkt) {
	struct circuit_DB *curDatabase = &database[lspduPkt->router_id - 1];
	// if current database has such link info already, no need to update
	for (unsigned int i = 0; i < curDatabase->nbr_link; i++) {
		if (curDatabase->linkcost[i].link == lspduPkt->link_id && curDatabase->linkcost[i].cost == lspduPkt->cost) {
			return false;
		}
	}
  // otherwise, add the link into the database
  unsigned index = curDatabase->nbr_link;
  curDatabase->nbr_link++;
	curDatabase->linkcost[index].link = lspduPkt->link_id;
	curDatabase->linkcost[index].cost = lspduPkt->cost;
	return true;
}

// help function that add timeout functionality to recvfrom function
static int recvfromWithTimeout(int socketFileDescriptor, void *data, int length) {
  fd_set socks;
  struct timeval t;
  FD_ZERO(&socks);
  FD_SET(socketFileDescriptor, &socks);
  t.tv_sec = 10; // timeout value set to 10 seconds
  if (select(socketFileDescriptor + 1, &socks, NULL, NULL, &t)) {
    return recvfrom(socketFileDescriptor, data, length, 0, NULL, NULL); // normal receive
  }
  return -1; // timeout
}

// function that update database from receiving packets and update shortest path
static int createTable(unsigned int router_id, struct circuit_DB myDatabase, int routerFileDescriptor, struct sockaddr_in *host_addr, FILE *logFile) {
  int result; // check return value of packet to determine packet type
  char buffer[256] = {0}; // used to store received information
  struct circuit_DB database[NBR_ROUTER];
  memset(database, 0, sizeof(database)); // set all values to 0;
  database[router_id - 1] = myDatabase;
  // print database information
  fprintf(logFile, "\n# Topology Database\n");
  for (unsigned int i = 0; i < NBR_ROUTER; i++) {
    if (database[i].nbr_link > 0) { // only print routers that have links
      fprintf(logFile, "R%u -> R%u nbr link %u\n", router_id, i + 1, database[i].nbr_link);
      for (unsigned int j = 0; j < database[i].nbr_link; j++) {
        struct link_cost currentLink = database[i].linkcost[j];
        fprintf(logFile, "R%u -> R%u link %u cost %u\n", router_id, i + 1, currentLink.link, currentLink.cost);
      }
    }
  }
  fprintf(logFile, "\n");

  // keep receiving packets
  while (true) {
    result = recvfromWithTimeout(routerFileDescriptor, &buffer, 256);
    if (result == -1) { // timeout, terminate the program
      return 1;
    } else if (result == sizeof(struct pkt_HELLO)) { // receive a Hello packet
      struct pkt_HELLO *helloPkt = (struct pkt_HELLO*)buffer; // get the hello packet received
      fprintf(logFile, "R%u receives a HELLO: router_id %u, link_id %u\n", router_id, helloPkt->router_id, helloPkt->link_id);
      sendLSPDUs(router_id, helloPkt->link_id, routerFileDescriptor, database, host_addr, logFile); // send LS PDU packets to the hello sender
    } else if (result == sizeof(struct pkt_LSPDU)) { // receive a LSPDU packet
      struct pkt_LSPDU *lspduPkt = (struct pkt_LSPDU*)buffer; // get the LSPDU packet received
      if (lspduPkt->router_id != router_id) {
        fprintf(logFile, "R%u receives a LS PDU: sender R%u, router_id %u, link_id %u, cost %u, via %u\n", router_id, lspduPkt->sender, lspduPkt->router_id, lspduPkt->link_id, lspduPkt->cost, lspduPkt->via);
        // if the database has updated
        if (updateDatabase(database, lspduPkt)) {
          // print updated database information
          fprintf(logFile, "\n# Topology Database\n");
          for (unsigned int i = 0; i < NBR_ROUTER; i++) {
            if (database[i].nbr_link > 0) { // only print routers that have links
              fprintf(logFile, "R%u -> R%u nbr link %u\n", router_id, i + 1, database[i].nbr_link);
              for (unsigned int j = 0; j < database[i].nbr_link; j++) {
                struct link_cost currentLink = database[i].linkcost[j];
                fprintf(logFile, "R%u -> R%u link %u cost %u\n", router_id, i + 1, currentLink.link, currentLink.cost);
              }
            }
          }
          fprintf(logFile, "\n");
          // update shortest path
          shortestPath(router_id, database, logFile);
          // send updated link state info to all neighbours
          for (unsigned int i = 0; i < database[router_id - 1].nbr_link; i++) {
            sendLSPDUs(router_id, database[router_id - 1].linkcost[i].link, routerFileDescriptor, database, host_addr, logFile);
          }
        }
      }
    }
  }
}

// function that create variables necessary (e.g. logfile, database) and start sending packets
int createRoutingTable(unsigned int router_id, int routerFileDescriptor, struct sockaddr_in *host_addr) {
  struct circuit_DB myTable; // routing table database for current router
  FILE *myLogFile; // log file for current router
  int result; // check for errors
  char name[256] = {0}; // used for creating log file name
  snprintf(name, 256, "router%u.log", router_id); // creating log file name
  myLogFile = fopen(name, "w"); // open log file
  setbuf(myLogFile, NULL);

  // send Init packet and check errors
  result = sendInitPkt(router_id, routerFileDescriptor, host_addr, myLogFile);
  if ( result < 0 ) return -1;

  // receive initial database from network emulator and check errors
  result = recvfrom(routerFileDescriptor, &myTable, sizeof(myTable), 0, NULL, NULL);
  if ( result == -1 || result != sizeof(myTable)) return -1;
  fprintf(myLogFile, "R%u receives a CIRCUIT_DB: nbr_link %u\n", router_id, myTable.nbr_link);

  // send Hello packet to all neighbours and check errors
  for (unsigned int i = 0; i < myTable.nbr_link; i++) {
    result = sendHelloPkt(router_id, myTable.linkcost[i].link, routerFileDescriptor, host_addr, myLogFile);
    if ( result < 0 ) return -1;
  }

  // create the actual table for the router
  result = createTable(router_id, myTable, routerFileDescriptor, host_addr, myLogFile);
  fclose(myLogFile);
  return result;
}

int main(int argc, char * argv[]) {
  unsigned int routerId;
  int hostPort, routerPort, routerFileDescriptor;
  struct hostent* hostName;
  struct sockaddr_in host_addr, router_addr;
  // check if the number of commandline inputs is correct
  if ( argc != 5 ) {
    fprintf(stderr, "Incorrect number of arguments\n");
    exit(EXIT_FAILURE);
  }

  // read commandline inputs and check for errors
  routerId = atoi(argv[1]);
  hostName = gethostbyname(argv[2]);
  if (hostName == NULL) {
    fprintf(stderr, "Host not found\n");
    exit(EXIT_FAILURE);
  }
  hostPort = atoi(argv[3]);
  routerPort = atoi(argv[4]);
  assert(routerId < 0xffff && hostPort < 0xfffd && routerPort < 0xffff);

  // create UDP socket
  routerFileDescriptor = socket(AF_INET, SOCK_DGRAM, 0);
  if (routerFileDescriptor < 0) {
    fprintf(stderr, "Cannot create UDP socket.\n");
    exit(EXIT_FAILURE);
  }

  // bind router address with the UDP socket
  router_addr.sin_family = AF_INET;
	router_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  router_addr.sin_port = htons(routerPort);
  if (bind(routerFileDescriptor, (struct sockaddr*) &router_addr, sizeof(router_addr)) < 0) {
		close(routerFileDescriptor);
		fprintf(stderr, "Cannot bind with the UDP socket.\n");
    exit(EXIT_FAILURE);
	}

  // set host address
  host_addr.sin_family = AF_INET;
  memcpy(&host_addr.sin_addr, hostName->h_addr_list[0], hostName->h_length);
	host_addr.sin_port = htons(hostPort);

  // create routing table and terminate if error happends
  if (createRoutingTable(routerId, routerFileDescriptor, &host_addr) < 0) {
		fprintf(stderr, "Cannot make the routing table.\n");
		close(routerFileDescriptor);
		return -1;
	}
}
