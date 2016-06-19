/* robotServer.c
    By Nathan Meade, Ankit Kajal, Daniel McLaughlin, Shafer LeMieux, Ryan Baxley */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <arpa/inet.h>
#include <errno.h>
#define BUFFERCONSTANT 1000 

#define MAX 255

char* castaraConnect(char* host, uint16_t port, char* path, int *retVal);

void dieWithError(const char *msg){
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[]) {
	//variables:
	char *robotHost, *robotID;
	int imageID;

    //UDP variables:
    int udpSock;
    struct sockaddr_in udpServAddr; /* Local address */
    struct sockaddr_in udpClntAddr; /* Client address */
    unsigned int cliAddrLen;         /* Length of incoming message */
    char udpBuffer[MAX];        /* Buffer for echo string */
    unsigned short udpPort;
    int recvMsgSize;                 /* Size of received message */

	// parse for port number for UDP connection with client
	if (argc != 5)    /* Test for correct number of arguments */
    {
        dieWithError("Usage: robotServer <UDP Server Port> <Robot Hostname> <Robot ID> <Image ID>\n");
    }

    udpPort = atoi(argv[1]);
    robotHost = argv[2];
    robotID = argv[3];
    imageID = atoi(argv[4]);

	/* Create socket for sending/receiving datagrams */
    if ((udpSock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        dieWithError("socket() failed");

    /* Construct local address structure */
    memset(&udpServAddr, 0, sizeof(udpServAddr));   /* Zero out structure */
    udpServAddr.sin_family = AF_INET;                /* Internet address family */
    udpServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    udpServAddr.sin_port = htons(udpPort);      /* Local port */

    /* Bind to the local address */
    printf("UDPEchoServer: About to bind to port %d\n", udpPort);    
    if (bind(udpSock, (struct sockaddr *) &udpServAddr, sizeof(udpServAddr)) < 0)
        dieWithError("bind() failed");
  
    for (;;) /* Run forever */
    {
        /* Set the size of the in-out parameter */
        cliAddrLen = sizeof(udpClntAddr);

        /* Block until receive message from a client */
        if ((recvMsgSize = recvfrom(udpSock, udpBuffer, MAX, 0,
            (struct sockaddr *) &udpClntAddr, &cliAddrLen)) < 0)
            dieWithError("recvfrom() failed");

        printf("Handling client %s\n", inet_ntoa(udpClntAddr.sin_addr));

        //variables again
        char robotID2[100], requestString[100];

        char* result = NULL;
        int resultSize = -1;
        char buffer2[512];
        buffer2[0] = 0;
        

        uint32_t requestID;
        memcpy(&requestID,udpBuffer,4);

        strcpy(robotID2,udpBuffer+4);
        if(strcmp(robotID2, robotID) != 0) dieWithError("robot ID's do not match");
        int padding = 4+strlen(robotID2)+1;
        strcpy(requestString, udpBuffer+padding);
        printf("requestString: %s\n", requestString);

        if (strncmp(requestString, "GET IMAGE", 9) == 0)
        {
            sprintf(buffer2, "/snapshot?topic=/robot_%d/image?width=600?height=500", imageID);
            result = castaraConnect(robotHost, 8081, buffer2, &resultSize);
        }
        else if (strncmp(requestString, "MOVE", 4) == 0)
        {
            double velocity;
            sscanf(requestString+4, "%lf", &velocity);
            sprintf(buffer2, "/twist?id=%s&lx=%lf", robotID2, velocity);
            result = castaraConnect(robotHost, 8082, buffer2, &resultSize);
        }
        else if (strncmp(requestString, "TURN", 4) == 0)
        {
            double velocity;
            sscanf(requestString+4, "%lf", &velocity);
            sprintf(buffer2, "/twist?id=%s&az=%lf", robotID2, velocity);
            result = castaraConnect(robotHost, 8082, buffer2, &resultSize);
        }
        else if (strncmp(requestString, "GET LASERS", 10) == 0)
        {
            sprintf(buffer2, "/state?id=%s", robotID2);
            result = castaraConnect(robotHost, 8083, buffer2, &resultSize);
        }
        else if (strncmp(requestString, "GET GPS", 7) == 0)
        {
            sprintf(buffer2, "/state?id=%s", robotID2);
            result = castaraConnect(robotHost, 8082, buffer2, &resultSize);
        }
        else if (strncmp(requestString, "GET DGPS", 8) == 0)
        {
            sprintf(buffer2, "/state?id=%s", robotID2);
            result = castaraConnect(robotHost, 8084, buffer2, &resultSize);
        }
        else if (strncmp(requestString, "STOP", 4) == 0)
        {
            sprintf(buffer2, "/twist?id=%s&lx=0", robotID2);
            result = castaraConnect(robotHost, 8082, buffer2, &resultSize);
        }
        else
        {
            printf("bad request");
            *result = '\0';
        }
        printf("sending %s out", buffer2);

        char responseBuffer[1000];
        memcpy(responseBuffer, (char*)&requestID, 4);

        uint32_t messageCount = (resultSize/988)+1;
        messageCount = htonl(messageCount);
        memcpy(responseBuffer+4, (char*)&messageCount, 4);

        int responseOffset = 0;
        int responseLeft = resultSize;
        int responseSize;
        int i;

        for(i=0;i<(int)ntohl(messageCount);i++){
            uint32_t sequenceNum = i;
            sequenceNum = htonl(sequenceNum);
            memcpy(responseBuffer+8, (char*)&sequenceNum, 4);

            if(responseLeft >= 988){
                responseSize = 988;
            }else{
                responseSize = responseLeft;
            }
            memcpy(responseBuffer+12, result+responseOffset, responseSize);

            /* Send received datagram back to the client */
            if (sendto(udpSock, responseBuffer, responseSize+12, 0, 
                 (struct sockaddr *) &udpClntAddr, sizeof(udpClntAddr)) < 0){
                printf("sendto() sent a different number of bytes than expected\n");
                exit(1);
            }
            responseLeft -= responseSize;
            responseOffset += responseSize;
        }
    }
    return 0;

}

char* castaraConnect(char* host, uint16_t port, char* path, int *retVal){

    int sock;
    struct sockaddr_in servAddr;
    struct hostent *thehost;
    unsigned short servPort;
    char *servIP;
    char *getRequest;
    unsigned int getRequestLen;
    char* buffer = malloc(BUFFERCONSTANT);
    char* buffer3 = malloc(50000);
    servPort = port;
    servIP = host;

    getRequest = malloc(BUFFERCONSTANT);
    sprintf(getRequest, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path,servIP);

    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
        printf("socket failed\n");
        exit(1);
    }

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family      = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(servIP);
    servAddr.sin_port        = htons(servPort);

    if (servAddr.sin_addr.s_addr == -1) {
        thehost = gethostbyname(servIP);
            servAddr.sin_addr.s_addr = *((unsigned long *) thehost->h_addr_list[0]);
    }

    if (connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0){
        printf("connect failed\n");
        exit(1);
    }


    getRequestLen = strlen(getRequest);

    if(send(sock, getRequest, getRequestLen, 0) != getRequestLen){
        printf("send failed\n");
        exit(1);
    }
    
    free(getRequest);

    int msgLen;
    int offset = 0;
    while( (msgLen = recv(sock, buffer, BUFFERCONSTANT, 0) ) > 0){
        memcpy(buffer3+offset, buffer, msgLen);
        memset(buffer, 0, sizeof(buffer));
        offset+=msgLen;
    }
    buffer3[offset] = '\0';

    if(port != 8081){
        /* Not an image */
        buffer3 = strstr(buffer3, "Server");
        buffer3 = strstr(buffer3, "\r\n\r\n")+4;
    }else{
        /* An image */
        buffer3 = strstr(buffer3, "Timestamp");
        buffer3 = strstr(buffer3, "\r\n\r\n")+4;
    }

    (*retVal) = offset; 
    close(sock);

    return buffer3;
}
