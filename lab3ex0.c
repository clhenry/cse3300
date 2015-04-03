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



#define RMT_SVR			"tao.ite.uconn.edu"
#define RMT_SVR_PORT	3300
#define RQST			"EX0"
#define USRNUM			4523
#define USRNAME			"C.L.Henry"
#define IPLENGTH		16
#define MSG_SZ			300


int main(void) {
	int remoteServerFD;
	struct sockaddr_in localClientAddr, remoteServerAddr;
	socklen_t sizeOfLocalClientAddr, sizeOfRemoteServerAddr;
	char incomingMsgBuf[MSG_SZ], outgoingMsgBuf[MSG_SZ];
	struct addrinfo *remoteServerAddrInfo, remoteServerAddrHints;
	char remoteIP[IPLENGTH], localIP[IPLENGTH];

	//Create TCP socket file descriptor
	remoteServerFD = socket(AF_INET, SOCK_STREAM, 0);

	//Clear remoteServerAddrHints
	memset(&remoteServerAddrHints, 0, sizeof(remoteServerAddrHints));
	//Hints for getaddrinfo() to better resolve the correct IP of the remote server
	remoteServerAddrHints.ai_socktype = SOCK_STREAM;
	remoteServerAddrHints.ai_family = AF_INET;

	/*
	 * Using the hint provided by remoteServerAddrHints, information is obtained about
	 * the RMT_SRV and stored in the linked list remoteServerAddrInfo
	 */
	getaddrinfo(RMT_SVR, NULL, &remoteServerAddrHints, &remoteServerAddrInfo);

	/*
	 * Assuming the first item in the linked list is what we are looking for then it should
	 * contain the correct IP address
	 */
	remoteServerAddr = *(struct sockaddr_in *)(remoteServerAddrInfo->ai_addr);

	//Convert remote port to expected network byte order
	remoteServerAddr.sin_port = htons(RMT_SVR_PORT);


	strcpy(remoteIP, inet_ntoa(remoteServerAddr.sin_addr));

	//Release memory allocated to linked list
	freeaddrinfo(remoteServerAddrInfo);

	sizeOfRemoteServerAddr = sizeof(remoteServerAddr);
	//Connect to remote server
	connect(remoteServerFD, (struct sockaddr *)(&remoteServerAddr), sizeOfRemoteServerAddr);

	sizeOfLocalClientAddr = sizeof(localClientAddr);
	//From connection to server obtain local IP and port information
	getsockname(remoteServerFD, (struct sockaddr *)(&localClientAddr), &sizeOfLocalClientAddr);

	//Copy local IP in W.X.Y.Z format
	strcpy(localIP, inet_ntoa(localClientAddr.sin_addr));
	int localPort = ntohs(localClientAddr.sin_port);

	//Format output string and store it into outgoingMsgBuf
	sprintf(outgoingMsgBuf, "%s %s-%d %s-%d %d %s\n", RQST, remoteIP, RMT_SVR_PORT, localIP, localPort, USRNUM, USRNAME);
	printf("\nSent:\n%s", outgoingMsgBuf);

	//Write string to file descriptor associated with socket
	send(remoteServerFD, outgoingMsgBuf, strlen(outgoingMsgBuf), 0);

	//Store server response in char array
	memset(incomingMsgBuf, 0, sizeof(incomingMsgBuf));
	recv(remoteServerFD, incomingMsgBuf, sizeof(incomingMsgBuf), 0);
	printf("\nReceived:\n%s", incomingMsgBuf);

	//Check received message for "OK" string
	char *token = strtok(incomingMsgBuf, "OK");
	if(token == NULL) {
		printf("Failed to receive correct response from server\n");
	}
	//Break rcvd string in two
	token = strtok(NULL, "OK");
	//Focus on second half
	token = strtok(token, " ");
	int tokenCount = 0;
	//Using " " as a delimiter should be able to scan all the way up till server number
	while(tokenCount < 2) {
		token = strtok(NULL, " ");
		tokenCount++;
	}

	//Convert number to string
	int serverNum = atoi(token);
	serverNum++;

	//Format string response to server
	sprintf(outgoingMsgBuf, "%s %d %d\n", RQST, (USRNUM + 2), serverNum);
	printf("\nSent:\n%s", outgoingMsgBuf);

	//Send reply and rcv response
	send(remoteServerFD, outgoingMsgBuf, strlen(outgoingMsgBuf), 0);
		
	memset(incomingMsgBuf, 0, sizeof(incomingMsgBuf));
	recv(remoteServerFD, incomingMsgBuf, sizeof(incomingMsgBuf), 0);
	printf("\nReceived:\n%s", incomingMsgBuf);

	//Check to make sure server replied with "OK"
	if(strstr(incomingMsgBuf, "OK") == NULL) printf("Failed to receive correct response from server\n");

	//Close socket
	close(remoteServerFD);
	return EXIT_SUCCESS;
}
