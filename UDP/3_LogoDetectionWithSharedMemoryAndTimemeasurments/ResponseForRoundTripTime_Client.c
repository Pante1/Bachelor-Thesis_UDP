//*** UDP client-server model
//*** Client side implementation
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <stdbool.h>
//Time **************************************************************/
#define NUM_MEASUREMENTS 1002
static long gStartTimestamp_Sec = 0;
static long gStartTimestamp_Nsec = 0;
static long gEndTimestamp_Sec = 0;
static long gEndTimestamp_Nsec = 0;
static long roundTripTime_Sec = 0;
static long roundTripTime_Nsec = 0;
static int timeMeasurementIndex = 0;
FILE *csvFile = NULL;

long getTimestamp_Sec() {
	struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec;
}

long getTimestamp_Nsec(void) {
	struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_nsec;
}
//*******************************************************************/

//UDP ***************************************************************/	
#define PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_ADDR "192.168.178.47"
#define NUM_LOGOS 4

struct sockaddr_in gServerAddr;
char gResvDataBuffer[BUFFER_SIZE];
int gFd_Sock;
uint8_t gLogos[NUM_LOGOS];
	
int createSocketFileDescriptor();
int initializeServerInfo();
int sendHelloMsg();
int sendMsg(const char *message);
int receiveMsg();
int receiveLogos();
//*******************************************************************/

int main()
{
	if(createSocketFileDescriptor() != 0)
    {
        exit(EXIT_FAILURE);
    }

    if(initializeServerInfo() != 0)
    {
        exit(EXIT_FAILURE);
    }
    
    int timeout = 5 * 60;
    time_t lastLogosReceivedTime = time(NULL);

    char *RequestMessage = "Logos";

	csvFile = fopen("RTT_UDP.csv", "w");

    if (csvFile == NULL) {
    	perror("Failed to open file");
        exit(EXIT_FAILURE);
    }
	fprintf(csvFile, "Nanoseconds\n");

    while(timeMeasurementIndex < NUM_MEASUREMENTS) {
		gStartTimestamp_Sec = getTimestamp_Sec();
        gStartTimestamp_Nsec = getTimestamp_Nsec();
		if (sendMsg(RequestMessage) != 0) {
            break;
        }
		timeMeasurementIndex++;

        if(receiveLogos() == 0) {
			gEndTimestamp_Sec = getTimestamp_Sec();
		    gEndTimestamp_Nsec = getTimestamp_Nsec();

			roundTripTime_Sec = gEndTimestamp_Sec - gStartTimestamp_Sec;
        	roundTripTime_Nsec = gEndTimestamp_Nsec - gStartTimestamp_Nsec;

            if (roundTripTime_Nsec < 0) {
                roundTripTime_Sec -= 1;
                roundTripTime_Nsec += 1000000000;
            }

            fprintf(csvFile, "%ld\n", roundTripTime_Sec * 1000000000 + roundTripTime_Nsec);

            lastLogosReceivedTime = time(NULL);

            printf("Received logos array:\n");
            for (int i = 0; i < NUM_LOGOS; i++)
            {
                printf("logo[%d] = %d\n", i, gLogos[i]);
            }
            
        }
        else {
            printf("Error receiving logos. Exiting loop.\n");
            break;
        }

        if(time(NULL) - lastLogosReceivedTime >= timeout)
        {
            printf("No logos received for %d seconds. Exiting loop.\n", timeout);
            break;
        }
    }
				
	close(gFd_Sock);

	return 0;
}

int sendMsg( const char *message)
{
	int result = -1;
	int errorFlag = -1;

	result = sendto(gFd_Sock, (const char *)message, strlen(message), MSG_CONFIRM, (const struct sockaddr *) &gServerAddr, sizeof(gServerAddr));
	if(result == strlen(message))
	{
		printf("Message sent.\n");
		errorFlag = 0;
	}
	else
	{
		printf("Message not sent.\n");
	}
	
	return errorFlag;
}

int receiveLogos()
{
	int result = -1;
	int errorFlag = -1;
	int len = sizeof(gServerAddr); 
		
	result = recvfrom(gFd_Sock, (char *)gResvDataBuffer, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr *) &gServerAddr, &len);
	if (result > 0)
	{
        if (result == sizeof(uint8_t) * NUM_LOGOS)
        {
            memcpy(gLogos, gResvDataBuffer, sizeof(uint8_t) * NUM_LOGOS);
            errorFlag = 0;
        }
        else
        {
            printf("Received incorrect number of bytes.\n");
            errorFlag = -1;
        }
	}
	else if (result == 0)
	{
		printf("Connection closed by the peer.\n");
	}
	else
	{
		perror("Error recvfrom failed\n");
	}

	return errorFlag;
}

int createSocketFileDescriptor()
{
	int errorFlag = -1;

	gFd_Sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (gFd_Sock == -1)
	{
		perror("Socket creation failed.\n");
	}
	else
	{
		errorFlag = 0;
		printf("Socket created successfully. File Descriptor: %d\n", gFd_Sock);
	}

	return errorFlag;
}

int initializeServerInfo()
{
	int errorFlag = -1;
	int result = -1;

	memset(&gServerAddr, 0, sizeof(gServerAddr));
		
	gServerAddr.sin_family = AF_INET;
	gServerAddr.sin_port = htons(PORT);

	result = inet_pton(AF_INET, SERVER_ADDR, &gServerAddr.sin_addr);
	if (result == 1)
	{
		printf("Address converted successfully.\n");
		errorFlag = 0;
    }
	else if (result == 0)
	{
		fprintf(stderr, "Invalid address: %s\n", SERVER_ADDR);
	}
	else if (result == -1)
	{
		perror("Parameter af does not contain a valid address family.\n");
	}

	return errorFlag;
}

int sendHelloMsg()
{
	int result = -1;
	int errorFlag = -1;
	char *helloMsg = "Hello from client";

	result = sendto(gFd_Sock, (const char *)helloMsg, strlen(helloMsg), MSG_CONFIRM, (const struct sockaddr *) &gServerAddr, sizeof(gServerAddr));
	if(result == strlen(helloMsg))
	{
		printf("Hello message sent.\n");
		errorFlag = 0;
	}
	else
	{
		printf("Hello message not sent.\n");
	}
	
	return errorFlag;
}

int receiveMsg()
{
	int result = -1;
	int errorFlag = -1;
	int len = sizeof(gServerAddr); 
		
	result = recvfrom(gFd_Sock, (char *)gResvDataBuffer, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr *) &gServerAddr, &len);
	if (result > 0)
	{
		gResvDataBuffer[result] = '\0';
		printf("Server: %s\n", gResvDataBuffer);
		errorFlag = 0;
	}
	else if (result == 0)
	{
		printf("Connection closed by the peer.\n");
	}
	else
	{
		perror("Error recvfrom failed\n");
	}

	return errorFlag;
}