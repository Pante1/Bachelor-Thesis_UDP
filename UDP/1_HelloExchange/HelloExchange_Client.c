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
	
#define PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_ADDR "192.168.178.47"

struct sockaddr_in servaddr;
char resvDataBuffer[BUFFER_SIZE];
int sockfd;
	
int createSocketFileDescriptor();
int initializeServerInfo();
int sendHelloMsg();
int receiveMsg();

int main()
{
	int result = -1;

	result = createSocketFileDescriptor();
	if (result == 0)
	{
		result = initializeServerInfo();
		if (result == 0)
		{
			result = sendHelloMsg();
			if (result == 0)
			{
				result = receiveMsg();
			}
		}
		close(sockfd);
	}
	
	if(result != 0)
	{
		exit(EXIT_FAILURE);
	}

	return 0;
}

int createSocketFileDescriptor()
{
	int errorFlag = -1;

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd == -1)
	{
		perror("Socket creation failed.\n");
	}
	else
	{
		errorFlag = 0;
		printf("Socket created successfully. File Descriptor: %d\n", sockfd);
	}

	return errorFlag;
}

int initializeServerInfo()
{
	int errorFlag = -1;
	int result = -1;

	memset(&servaddr, 0, sizeof(servaddr));
		
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(PORT);

	result = inet_pton(AF_INET, SERVER_ADDR, &servaddr.sin_addr);
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

	result = sendto(sockfd, (const char *)helloMsg, strlen(helloMsg), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));
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
	int len = sizeof(servaddr); 
		
	result = recvfrom(sockfd, (char *)resvDataBuffer, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr *) &servaddr, &len);
	if (result > 0)
	{
		resvDataBuffer[result] = '\0';
		printf("Server: %s\n", resvDataBuffer);
		errorFlag = 0;
	}
	else if (result == 0)
	{
		printf("Connection closed by the peer.\n");
	}
	else if (result == -1)
	{
		perror("recvfrom failed");
	}

	return errorFlag;
}