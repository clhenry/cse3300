#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <time.h>
#include <netinet/in.h>
#include <netinet/udp.h>

#define RSERVERADDR     "137.99.11.9"
#define RSERVERPORT     3300
#define LABNUMBER       0x04
#define VERSION 		0x07
#define COURSENUM		3300
#define MSGTYPE			0
#define REQUEST			0
#define RESPONSE		1
#define SEEDED			1
#define	 UNSEEDED		0
#define MAXRETRIES		3
#define RETRYPERIOD		3	//In seconds

struct datagram {
	unsigned int messType : 1;
	unsigned int rFlag : 1;
	unsigned int course : 14;
	unsigned int lab : 8;
	unsigned int version : 8;
	unsigned int clientCookie : 32;
	unsigned int requestData : 32;
	unsigned int checksum : 16;
	unsigned int transactionOutcome : 1;
	unsigned int result : 15;
};

struct compactdatagram {
	unsigned short messType_rFlag_Course;
	unsigned short lab_version;
	unsigned int clientCookie;
	unsigned int requestData;
	unsigned short checksum;
	unsigned short transactionOutcome_Result;
};

void reverseFieldByteOrder(struct compactdatagram *payload) {
	payload->messType_rFlag_Course = htons(payload->messType_rFlag_Course);
	payload->lab_version = htons(payload->lab_version);
	payload->clientCookie = htonl(payload->clientCookie);
	payload->requestData = htonl(payload->requestData);
	payload->transactionOutcome_Result = htons(payload->transactionOutcome_Result);
};

struct compactdatagram datagramToCompactForm(struct datagram * datagram) {
	struct compactdatagram compact;
	compact.lab_version = (datagram->lab << 8) + datagram->version;
	compact.messType_rFlag_Course = (datagram->messType << 15) + (datagram->rFlag << 14) + datagram->course;
	compact.transactionOutcome_Result = (datagram->transactionOutcome << 15) + (datagram->result);
	compact.clientCookie = datagram->clientCookie;
	compact.requestData = datagram->requestData;
	return compact;
}

unsigned int calculateChecksum(struct compactdatagram *payload) {
	unsigned int checksum = 0;
	checksum +=  (payload->messType_rFlag_Course + payload->lab_version);
	checksum += ((payload->clientCookie >> 16) + (payload->clientCookie & 0x0000FFFF));
	checksum += ((payload->requestData >> 16) + (payload->requestData & 0x0000FFFF));
	//If the upper 16 bits are not all 0s then we have an overflow and need to add those overflow bits to the lower 16 bits
	if(checksum & 0xFFFF0000) checksum = ((checksum >> 16) + (checksum & 0x0000FFFF));
	checksum = ~(checksum); //Invert bits for ones complement
	return checksum;
};

void verifyChecksum(struct compactdatagram *payload) {
	unsigned int checksum = 0;
	checksum +=  (payload->messType_rFlag_Course + payload->lab_version);
	checksum += ((payload->clientCookie >> 16) + (payload->clientCookie & 0x0000FFFF));
	checksum += ((payload->requestData >> 16) + (payload->requestData & 0x0000FFFF));
	checksum +=payload->checksum;
	//If the upper 16 bits are not all 0s then we have an overflow and need to add those overflow bits to the lower 16 bits
	if(checksum & 0xFFFF0000) {
		checksum = ((checksum >> 16) + (checksum & 0x0000FFFF));
	}
	checksum = ~(checksum); //Invert bits for ones complement
};

void prepType0Rqst(struct datagram *payload, int SSN) {

	static int seed = UNSEEDED;

	if(seed == UNSEEDED ) {
		srand(time(NULL));
		seed = SEEDED;
	}

	payload->messType = MSGTYPE;
	payload->rFlag = REQUEST;
	payload->course = COURSENUM;
	payload->lab = LABNUMBER;
	payload->version = VERSION;
	payload->clientCookie = rand();
	payload->requestData = SSN;
	payload->checksum = 0;
	payload->transactionOutcome = 0;
	payload->result = 0;
};

int main(void) {
	int remoteServerFD;
	struct sockaddr_in remoteServerAddr, localClientAddr;
	socklen_t sizeOfRemoteServerAddr, sizeOfLocalClientAddr;
	size_t sizeOfIncomingData, sizeOfOutgoingData;
	struct datagram localData;
	struct compactdatagram incomingData, outgoingData;
	int retransmitCount = 0;
	struct timeval retransmitTimer;

	retransmitTimer.tv_sec = RETRYPERIOD;
	retransmitTimer.tv_usec = 0;

	//Social to be looked up;
	int SSN = -1;
	while(SSN < 0 || SSN > 999999999) {
		printf("Enter a 9-digit social security number: ");
		scanf("%d", &SSN);
	}

	sizeOfIncomingData = sizeof(incomingData);
	//zero 'incomingData' structure
	memset(&incomingData, 0, sizeOfIncomingData);

	sizeOfOutgoingData = sizeof(outgoingData);
	//zero 'outgoingData' structure
	memset(&outgoingData, 0, sizeOfOutgoingData);

	//create UDP socket to connect to remote server
	remoteServerFD = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	//zero structure containing address of remote server
	memset(&remoteServerAddr, 0, sizeof(remoteServerAddr));
	remoteServerAddr.sin_family = AF_INET;
	//convert IP string to network byte order representation
	inet_pton(AF_INET, RSERVERADDR, (void *)&remoteServerAddr.sin_addr.s_addr);
	//port that the remote server is expecting incoming communication on
	remoteServerAddr.sin_port = htons(RSERVERPORT);

	//configures the 'outgoingData' structure as a type 0 message with SSN
	prepType0Rqst(&localData, SSN);
	//transfer localData structure to a compacted datagram structure
	outgoingData = datagramToCompactForm(&localData);
	//reverse the byte order of the fields of the datagram
	reverseFieldByteOrder(&outgoingData);
	//calculate and populate checksum field
	outgoingData.checksum = calculateChecksum(&outgoingData);

	sizeOfRemoteServerAddr = sizeof(remoteServerAddr);
	sizeOfLocalClientAddr = sizeof(localClientAddr);

	//clear struct containing address of local client
	memset(&localClientAddr, 0, sizeof(localClientAddr));

	//continual try to send datagram and receive response until either successful or retransmissions have been exhausted
	while(retransmitCount < MAXRETRIES) {
		//transmit 'outgoingData' datagram to server
		if(sendto(remoteServerFD, &outgoingData, sizeOfOutgoingData, 0, (struct sockaddr *)&remoteServerAddr, sizeOfRemoteServerAddr) == -1) {
			printf("There was an issue transmitting datagram.\n");
		}

		//Setup a timer to break the blocking recvfrom() function in the event that there is trouble receiving the expected data
		if(setsockopt(remoteServerFD, SOL_SOCKET, SO_RCVTIMEO, (char *)&retransmitTimer, sizeof(retransmitTimer)) == -1) {
			printf("Unable to set receive timeout. Exiting.\n");
			exit(1);
		}

		//If the program received less data than expected or there was an error receiving, retry sending
		if(recvfrom(remoteServerFD, (void *)&incomingData, sizeOfIncomingData, 0, NULL, (socklen_t *)NULL) < (int)sizeOfOutgoingData) {
			printf("Error retransmitting datagram. Retrying.\n");
			retransmitCount++;
		}
		//break the loop if we have successfully received a response
		else break;
	}

	printf("P.O. Box: %d\n", ntohs(incomingData.transactionOutcome_Result) & 0x7FFF);

	return 0;
}
