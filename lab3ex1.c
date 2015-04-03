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
#define RQST					"EX1"
#define USRNUM				4523
#define USRNAME			"C.L.Henry"
#define IPLENGTH			16
#define MSGSZ				300


int main(void) {
	int remoteServerFD, localListenerFD, localServerFD;
	int serverNum, newServerNum, localListenerPort, localServerPort, remoteServerPort;
	int tokenCount;
	struct sockaddr_in localClientAddr, localListenerAddr, localServerAddr, remoteServerAddr, remoteClientAddr;
	socklen_t sizeOfLocalClientAddr, sizeOfLocalListenerAddr, sizeOfLocalServerAddr, sizeOfRemoteServerAddr, sizeOfRemoteClientAddr;
	char incomingMsgBuf[MSGSZ], outgoingMsgBuf[MSGSZ], remoteIP[IPLENGTH], localIP[IPLENGTH], localListenerIP[IPLENGTH], *token;
	struct addrinfo *remoteServerAddrInfo, remoteServerAddrHints;

	//Create TCP socket file descriptor
	remoteServerFD = socket(AF_INET, SOCK_STREAM, 0);
	localServerFD = socket(AF_INET, SOCK_STREAM, 0);

	//This file descriptor will listen for incoming connections and will be passed to accept()
	localListenerFD = socket(AF_INET, SOCK_STREAM, 0);

	/****************************************************************************************
	* When listen(2) is called on an unbound socket, the socket is automatically
	* bound to a random free port with the local address set to INADDR_ANY.
	 ***************************************************************************************/
	listen(localListenerFD, 5);

	sizeOfLocalListenerAddr = sizeof(localListenerAddr);
	//Get the address information associated with the local listener. Specifically interested in the port
	getsockname(localListenerFD, (struct sockaddr *)(&localListenerAddr), &sizeOfLocalListenerAddr);

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
	//Copy remote IP into remoteIP
	strcpy(remoteIP, inet_ntoa(remoteServerAddr.sin_addr));

	//Release memory allocated to linked list
	freeaddrinfo(remoteServerAddrInfo);

	sizeOfRemoteServerAddr = sizeof(remoteServerAddr);
	//Connect to remote server
	connect(remoteServerFD, (struct sockaddr *)(&remoteServerAddr), sizeOfRemoteServerAddr);

	sizeOfLocalClientAddr = sizeof(localClientAddr);
	//From connection to server obtain local IP and port information
	getsockname(remoteServerFD, (struct sockaddr *)(&localClientAddr), &sizeOfLocalClientAddr);

	//Copy local IP in W.X.Y.Z format into localIP
	strcpy(localIP, inet_ntoa(localClientAddr.sin_addr));
	//And get port number for from the listen socket
	localListenerPort = ntohs(localListenerAddr.sin_port);

	//Format output string and store it into outgoingMsgBuf
	sprintf(outgoingMsgBuf, "%s %s-%d %s-%d %d %s\n", RQST, remoteIP, RMT_SVR_PORT, localIP, localListenerPort, USRNUM, USRNAME);
	printf("Sent:\n%s", outgoingMsgBuf);

	//Write string to file descriptor associated with socket
	send(remoteServerFD, outgoingMsgBuf, strlen(outgoingMsgBuf), 0);

	//Store server response in char array
	memset(incomingMsgBuf, 0, sizeof(incomingMsgBuf));
	recv(remoteServerFD, incomingMsgBuf, sizeof(incomingMsgBuf), 0);
	printf("\nReceived:\n%s", incomingMsgBuf);

	//Check received message for "OK" string
	token = strtok(incomingMsgBuf, "OK");
	if(token == NULL) {
		printf("Failed to receive correct response from server\n");
	}
	//Break rcvd string in two
	token = strtok(NULL, "OK");
	//Focus on second half
	token = strtok(token, " ");
	tokenCount = 0;
	//Using " " as a delimiter we can extract servernum
	while(tokenCount < 2) {
		token = strtok(NULL, " ");
		tokenCount++;
	}

	//Convert integer string to integer
	serverNum = atoi(token);
	serverNum++;

	sizeOfRemoteClientAddr = sizeof(remoteClientAddr);
	//Accept any incoming connections on our listening port
	localServerFD = accept(localListenerFD, (struct sockaddr *)(&remoteClientAddr), &sizeOfRemoteClientAddr);

	//Store and print received message
	memset(incomingMsgBuf, 0, sizeof(incomingMsgBuf));
	recv(localServerFD, incomingMsgBuf, sizeof(incomingMsgBuf), 0);
	printf("\nReceived:\n%s", incomingMsgBuf);

	//If received message contains "CSE 3300 server calling" then it is the expected message
	if(strstr(incomingMsgBuf, "CSE 3300 server calling") != NULL) {
		tokenCount = 0;
		//Seek to the 4th whitepace
		token = strtok(incomingMsgBuf, " \n");
		while(tokenCount < 4) {
			token = strtok(NULL, " \n");
			tokenCount++;
		}
	}
	else printf("Failed to receive expected response\n");

	//newServerNum to int
	newServerNum = atoi(token);
	newServerNum++;

	//Format msg to be sent
	sprintf(outgoingMsgBuf, "%d %d\n", serverNum, newServerNum);

	printf("\nSent:\n%s", outgoingMsgBuf);
	//Send message on local server socket
	send(localServerFD, outgoingMsgBuf, strlen(outgoingMsgBuf), 0);

	close(localServerFD);
	close(localListenerFD);

	//Clear buffer
	memset(incomingMsgBuf, 0, sizeof(incomingMsgBuf));
	//Receive response to previous message on original connect
	recv(remoteServerFD, incomingMsgBuf, sizeof(incomingMsgBuf), 0);

	printf("\nReceived:\n%s", incomingMsgBuf);

	if(strstr(incomingMsgBuf, "OK") == NULL) printf("Failed to receive correct response from server\n");

	//Close initial socket between local client and remote server
	close(remoteServerFD);
	return EXIT_SUCCESS;
}
