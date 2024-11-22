//*** UDP client-server model
//*** Server side implementation
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

struct sockaddr_in servaddr, cliaddr;
char resvDataBuffer[BUFFER_SIZE];
int sockfd;

int createSocketFileDescriptor();
void initializeServerInfo();
int bindSocketToAdress();
int receiveMsg();
int sendHelloMsg();

int main()
{
	int result = -1;

	result = createSocketFileDescriptor();
	if (result == 0)
	{
		initializeServerInfo();
		result = bindSocketToAdress();
		if (result == 0)
		{
			result = receiveMsg();
			if (result == 0)
			{
				result = sendHelloMsg();
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

void initializeServerInfo()
{
	memset(&servaddr, 0, sizeof(servaddr));
	memset(&cliaddr, 0, sizeof(cliaddr));
		
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(PORT);
}

int bindSocketToAdress()
{
	int result = -1;
	int errorFlag = -1;

	result = bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));
	if (result == -1)
	{
		perror("Bind failed.\n");
	}
	else if (result == 0)
	{
		printf("Bind socket to adress successfully.\n");
		errorFlag = 0;
	}

	return errorFlag;
}

int receiveMsg()
{
	int result = -1;
	int errorFlag = -1;
	int len = sizeof(cliaddr);
	
	result = recvfrom(sockfd, (char *)resvDataBuffer, BUFFER_SIZE, MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);
	if (result > 0)
	{
		resvDataBuffer[result] = '\0';
		printf("Client: %s\n", resvDataBuffer);
		errorFlag = 0;
	}
	else if (result == 0)
	{
		printf("Connection closed by the peer.\n");
	}
	else if (result == -1)
	{
		perror("Error recvfrom failed.\n");
	}

	return errorFlag;
}

int sendHelloMsg()
{
	int result = -1;
	int errorFlag = -1;
	int len = sizeof(cliaddr);
	char *helloMsg = "Hello from server.";

	result = sendto(sockfd, (const char *)helloMsg, strlen(helloMsg), MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);
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
