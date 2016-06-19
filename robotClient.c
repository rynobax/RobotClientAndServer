/* robotClient.c
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
#include <errno.h>      /* for errno and EINTR */
#include <signal.h>
#include <sys/time.h>
#include <string.h>


char *get_ip(char *host);

int keepWaiting = 1;

void timer_handler (int signum)
{
    keepWaiting = 0;
}

void dieWithError(const char *msg){
    perror(msg);
    exit(0);
}

#define TIMEOUT 5

void issueCommands(uint32_t *requestID, int iteration, int sock, struct sockaddr_in servAddr, char *robotID, int length, int n);
void doASide(double length, double radians, char *robotID, uint32_t *requestID, int iteration, int sock, struct sockaddr_in servAddr);
void sendMove(double length, char *robotID, uint32_t *requestID, int sock, struct sockaddr_in servAddr);
void getData(char *robotID, uint32_t *requestID, int iteration, int sock, struct sockaddr_in servAddr);
void sendTurn(double radians, char *robotID, uint32_t *requestID, int sock, struct sockaddr_in servAddr);
char *makeRequestString(char *requestString, char *robotID, uint32_t *requestID, char *message);
void sendData(char *requestMessage, char *robotID, uint32_t *requestID, int iteration, int sock, struct sockaddr_in servAddr);
void compareIDs(uint32_t requestID, uint32_t responseID);
void compareSeq(uint32_t requestSeq, uint32_t responseSeq);

/* For file naming */
char nstring[1];

int main(int argc, char *argv[])
{
	if (argc != 6)     /* Test for correct number of arguments */
    {
        fprintf(stderr, "Usage:  %s IP/host_name server_port robot_ID L N\n", argv[0]);
        exit(1);
    }

    char *IP = get_ip(argv[1]);

    int port = atoi(argv[2]);

    char *robotID = argv[3];

    int L = atoi(argv[4]);

    int N = atoi(argv[5]);
    if(N < 4 || N > 8) dieWithError("N must be between 4 and 8\n");

    int sock;                        /* Socket descriptor */
    struct sockaddr_in servAddr; /* Server address */

    /* Create a datagram/UDP socket */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        dieWithError("socket() failed");

    /* Construct the server address structure */
    memset(&servAddr, 0, sizeof(servAddr));    /* Zero out structure */
    servAddr.sin_family = AF_INET;                 /* Internet addr family */
    servAddr.sin_addr.s_addr = inet_addr(IP);  /* Server IP address */
    servAddr.sin_port   = htons(port);     /* Server port */

    uint32_t requestID = 0;
    int iteration = 0;

    sprintf(nstring, "%d", N);
    issueCommands(&requestID, iteration, sock, servAddr, robotID, L, N);

    sprintf(nstring, "%d", N-1);
    issueCommands(&requestID, iteration, sock, servAddr, robotID, L, N-1);

    return 0;
}

void issueCommands(uint32_t *requestID, int iteration, int sock, struct sockaddr_in servAddr, char *robotID, int length, int n){
	double degrees = 360 / n;
	double radians = degrees * (3.14159265358979323846 / 180);
    int i;
	for(i=n; i>0; i--){
		doASide(length, radians, robotID, requestID, i, sock, servAddr);
	}
}

void doASide(double length, double radians, char *robotID, uint32_t *requestID, int iteration, int sock, struct sockaddr_in servAddr){
  	sendMove(length, robotID, requestID, sock, servAddr);
    sendData("GET IMAGE", robotID, requestID, iteration, sock, servAddr);
    sendData("GET GPS", robotID, requestID, iteration, sock, servAddr);
    sendData("GET DGPS", robotID, requestID, iteration, sock, servAddr);
    sendData("GET LASERS", robotID, requestID, iteration, sock, servAddr);
  	sendTurn(radians, robotID, requestID, sock, servAddr);
}

void sendMove(double length, char *robotID, uint32_t *requestID, int sock, struct sockaddr_in servAddr){
    uint32_t u32;
    struct sockaddr_in fromAddr;     /* Source address of echo */
    unsigned int fromSize;

	/* Send move request, wait for robot to get there, send stop */
    double duration = 0;
    double speed = 0;
    if(length > 6){
        duration = length;
        speed = 1;
    }
    else{
        speed = length / 10;
        duration = length / speed;
    }

    int robotIDLength = strlen(robotID);

    char requestMessage[20];
    sprintf(requestMessage, "MOVE%9lf", speed);
    char responseBuffer[1001];      /* Buffer for response string */
    char requestString[1000]; /* Buffer for string that is sent */

    /* Make Request */
    makeRequestString(requestString, robotID, requestID, requestMessage);
    int requestMessageLength = strlen(requestMessage);
    uint32_t tempu32;
    memcpy(&tempu32, requestString, 4);
    int requestStringLength = 4 + robotIDLength + 1 + requestMessageLength;

    if (sendto(sock, requestString, requestStringLength, 0, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)
        dieWithError("sendto() reported and error");
    fromSize = sizeof(fromAddr);
    alarm(TIMEOUT);        /* Set the timeout */

    struct sigaction sa;
    struct itimerval timer;
    /* Install timer_handler as the signal handler for SIGVTALRM. */
    memset (&sa, 0, sizeof (sa));
    sa.sa_handler = &timer_handler;
    sigaction (SIGVTALRM, &sa, NULL);
    timer.it_value.tv_sec = duration;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    /* Start a virtual timer. It counts down whenever this process is
        executing. */
    setitimer (ITIMER_VIRTUAL, &timer, NULL);

    int respStringLen;
    while ((respStringLen = recvfrom(sock, responseBuffer, 1000, 0,
           (struct sockaddr *) &fromAddr, &fromSize)) < 0){ 
        if (errno == EINTR) dieWithError("No Response");
        else dieWithError("recvfrom() failed");
    }

    /* recvfrom() got something --  cancel the timeout */
    alarm(0);

    /* Check for ID match */
    uint32_t responseID;
    memcpy(&responseID, responseBuffer, 4);
    responseID = ntohl(responseID);
    compareIDs(*requestID, responseID);

    responseBuffer[respStringLen] = '\0';

    while(keepWaiting == 1){

    }
    keepWaiting = 1;

    alarm(TIMEOUT);        /* Set the timeout */

    speed = 0;
    sprintf(requestMessage, "MOVE%9lf", speed);
    requestMessageLength = strlen(requestMessage);

    /* Convert request id to network order and append it to request */
    //(*requestID)++;
    u32 = *requestID;
    u32 = htonl(u32);
    memcpy(requestString, (char*)&u32, 4);

    /* Make request string */
    makeRequestString(requestString, robotID, requestID, requestMessage);
    requestMessageLength = strlen(requestMessage);
    memcpy(&tempu32, requestString, 4);
    requestStringLength = 4 + robotIDLength + 1 + requestMessageLength;

    if (sendto(sock, requestString, requestStringLength, 0, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)
        dieWithError("sendto() reported and error");

    while ((respStringLen = recvfrom(sock, responseBuffer, 1000, 0,
           (struct sockaddr *) &fromAddr, &fromSize)) < 0){ 
        if (errno == EINTR) dieWithError("No Response");
        else dieWithError("recvfrom() failed");
    }
    alarm(0);

    /* Check for ID match */
    memcpy(&responseID, responseBuffer, 4);
    responseID = ntohl(responseID);
    compareIDs(*requestID, responseID);
}

void sendTurn(double radians, char *robotID, uint32_t *requestID, int sock, struct sockaddr_in servAddr){
    struct sockaddr_in fromAddr;     /* Source address of echo */
    unsigned int fromSize;

	/* Send turn request, wait for robot to finish turning, send stop */
    double duration = 0;
    double speed = 0;  
    if(radians > 6){
        duration = radians;
        speed = 1;
    }
    else{
    speed = radians / 10;
    duration = radians / speed;
    }

    int robotIDLength = strlen(robotID);

    char requestMessage[20];
    sprintf(requestMessage, "TURN%9lf", speed);
    int requestMessageLength = strlen(requestMessage);
    char responseBuffer[1001];      /* Buffer for response string */
    char requestString[1000]; /* Buffer for string that is sent */

    /* Make Request */
    makeRequestString(requestString, robotID, requestID, requestMessage);
    requestMessageLength = strlen(requestMessage);
    uint32_t tempu32;
    memcpy(&tempu32, requestString, 4);
    int requestStringLength = 4 + robotIDLength + 1 + requestMessageLength;

    if (sendto(sock, requestString, requestStringLength, 0, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)
        dieWithError("sendto() reported and error");
    fromSize = sizeof(fromAddr);
    alarm(TIMEOUT);        /* Set the timeout */

    struct sigaction sa;
    struct itimerval timer;
    /* Install timer_handler as the signal handler for SIGVTALRM. */
    memset (&sa, 0, sizeof (sa));
    sa.sa_handler = &timer_handler;
    sigaction (SIGVTALRM, &sa, NULL);
    timer.it_value.tv_sec = duration;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    /* Start a virtual timer. It counts down whenever this process is
        executing. */
    setitimer (ITIMER_VIRTUAL, &timer, NULL);

    int respStringLen;
    while ((respStringLen = recvfrom(sock, responseBuffer, 1000, 0,
           (struct sockaddr *) &fromAddr, &fromSize)) < 0){ 
        if (errno == EINTR) dieWithError("No Response");
        else dieWithError("recvfrom() failed");
    }

    /* recvfrom() got something --  cancel the timeout */
    alarm(0);

    uint32_t responseID;
    memcpy(&responseID, responseBuffer, 4);
    responseID = ntohl(responseID);
    compareIDs(*requestID, responseID);

    responseBuffer[respStringLen] = '\0';

    while(keepWaiting == 1){

    }
    keepWaiting = 1;

    //(*requestID)++;

    alarm(TIMEOUT);        /* Set the timeout */

    speed = 0;
    sprintf(requestMessage, "TURN%9lf", speed);
    requestMessageLength = strlen(requestMessage);

    /* Make Request */
    makeRequestString(requestString, robotID, requestID, requestMessage);
    memcpy(&tempu32, requestString, 4);
    requestStringLength = 4 + robotIDLength + 1 + requestMessageLength;

    if (sendto(sock, requestString, requestStringLength, 0, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)
        dieWithError("sendto() reported and error");

    while ((respStringLen = recvfrom(sock, responseBuffer, 1000, 0,
           (struct sockaddr *) &fromAddr, &fromSize)) < 0){ 
        if (errno == EINTR) dieWithError("No Response");
        else dieWithError("recvfrom() failed");
    }
    alarm(0);
    memcpy(&responseID, responseBuffer, 4);
    responseID = ntohl(responseID);
    compareIDs(*requestID, responseID);
}

void sendData(char *requestMessage, char *robotID, uint32_t *requestID, int iteration, int sock, struct sockaddr_in servAddr){
    struct sockaddr_in fromAddr;     /* Source address of echo */
    unsigned int fromSize;
    char iterationString[10];
    sprintf(iterationString, "%i", iteration);

    char requestIDString[20];
    sprintf(requestIDString, "%lu", (unsigned long) *requestID);

    int robotIDLength = strlen(robotID);
    char responseBuffer[1001];      /* Buffer for response string */
    char requestString[1000]; /* Buffer for string that is sent */

    /* Form request string */
    makeRequestString(requestString, robotID, requestID, requestMessage);
    int requestMessageLength = strlen(requestMessage);

    /* Get Image */
    /* name format: <reqID>_<image/position>_n<N>*/
    char fileName[100] = "ID-";
    
    strcat(fileName, requestIDString);
    strcat(fileName, "_n");
    strcat(fileName, nstring);
    strcat(fileName, "_");
    if(strstr(requestMessage, "IMAGE") != NULL){
        strcat(fileName, "image.jpeg");
    }else if(strstr(requestMessage, "DGPS")){
        strcat(fileName, "DGPS.txt");
    }else if(strstr(requestMessage, "GPS")){
        strcat(fileName, "GPS.txt");
    }else if(strstr(requestMessage, "LASERS")){
        strcat(fileName, "LASERS.txt");
    }

    FILE *File = fopen(fileName, "a");
    if (File == NULL)
    {
        dieWithError("Error opening file!\n");
    }

    //alarm(TIMEOUT);        /* Set the timeout */
    uint32_t u32temp;
    memcpy(&u32temp, requestString, 4);
    int requestStringLength = 4 + robotIDLength + 1 + requestMessageLength;

    if (sendto(sock, requestString, requestStringLength, 0, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)
        dieWithError("sendto() reported and error");

    /* Read the packets */
    int i;
    int packetCount = 100;
    for(i=0; i<packetCount; i++){
        /* Try to recieve packet */
        alarm(TIMEOUT);
        int respStringLen;
        memset(responseBuffer, 0, sizeof(responseBuffer));
        while ((respStringLen = recvfrom(sock, responseBuffer, 1000, 0,
           (struct sockaddr *) &fromAddr, &fromSize)) < 0){ 
        if (errno == EINTR) dieWithError("No Response");
        else dieWithError("recvfrom() failed");
        }
        alarm(0);

        /* Check response ID */
        uint32_t responseID;
        memcpy(&responseID, responseBuffer, 4);
        responseID = ntohl(responseID);

        compareIDs(*requestID, responseID);

        /* Set packet count */
        if(i==0){
            uint32_t packetCountBig;
            memcpy(&packetCountBig, responseBuffer+4, 4);
            packetCountBig = ntohl(packetCountBig);
            packetCount = (int)packetCountBig;
        }

        /* Check sequence number */
        uint32_t sequenceNumber;
        memcpy(&sequenceNumber, responseBuffer+4+4, 4);
        sequenceNumber = ntohl(sequenceNumber);
        compareSeq(i, sequenceNumber);

        if(strstr(requestMessage, "IMAGE") != NULL){
            /* If it's an image just copy data */
            int j;
            for(j=12;j<respStringLen;j++){
                fprintf(File, "%c", responseBuffer[j]);
            }
        }else{
            /* Otherwise print as string data so null char stop you */
            fprintf(File, "%s", responseBuffer+4+4+4);
            if(strstr(requestMessage, "DGPS")){
                printf("DGPS: %s\n", responseBuffer+4+4+4);
            }else if(strstr(requestMessage, "GPS")){
                printf("GPS: %s\n", responseBuffer+4+4+4);
            }
        }
    }

    fclose(File);
}

void compareIDs(uint32_t requestID, uint32_t responseID){
    if(requestID != responseID){
        printf("requestID: %lu\nresponseID: %lu\n", (unsigned long)requestID, (unsigned long)responseID);
        fflush(stdout);
        dieWithError("request/response ID's did not match!");
    }
}

void compareSeq(uint32_t requestSeq, uint32_t responseSeq){
    if(requestSeq != responseSeq){
        printf("requestSeq (i): %lu\nresponseSeq: %lu\n", (unsigned long)requestSeq, (unsigned long)responseSeq);
        fflush(stdout);
        dieWithError("request/response sequence numbers did not match!");
    }
}

char *makeRequestString(char *requestString, char *robotID, uint32_t *requestID, char *message){
    /* Convert request id to network order and append it to request */
    (*requestID)++;
    uint32_t u32 = *requestID;
    u32 = htonl(u32);
    memset(requestString, 0, sizeof(requestString));
    memcpy(requestString, &u32, 4);

    /* Append robot id and request messages to request */
    sprintf(requestString+4, "%s%c%s", robotID, '\0', message);

    return requestString;
}

/* Resolves IP of a host */
char *get_ip(char *host)
{  
  struct hostent *hent;
  int iplen = 15; //XXX.XXX.XXX.XXX
  char *ip = (char *)malloc(iplen+1);
  memset(ip, 0, iplen+1);
  if((hent = gethostbyname(host)) == NULL)
  {
    herror("Can't get IP");
    exit(1);
  }
  if(inet_ntop(AF_INET, (void *)hent->h_addr_list[0], ip, INET_ADDRSTRLEN) == NULL)
  {
    perror("Can't resolve host");
    exit(1);
  }
  return ip;
}
