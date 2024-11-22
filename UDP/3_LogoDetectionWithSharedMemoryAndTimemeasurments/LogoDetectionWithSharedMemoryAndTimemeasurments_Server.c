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
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <signal.h>

const char* PathToObjectDetectionModel = "ObjectDetectionModel.py";

//Time **************************************************************/
#define NUM_MEASUREMENTS 250
static long gStartTimestamp_Sec;
static long gStartTimestamp_Nsec;
static long gEndTimestamp_Sec;
static long gEndTimestamp_Nsec;
static int timeMeasurementIndex = 0;

typedef struct {
	long seconds;
	long nanoseconds;
} gTimeMeasurement;

gTimeMeasurement gAccessTime_SharedMemory[NUM_MEASUREMENTS];
gTimeMeasurement gRoundTripTime_UDP[NUM_MEASUREMENTS];

long getTimestamp_Sec();
long getTimestamp_Nsec();
void calculateAndPrintAverage(gTimeMeasurement *timeArray, int numMeasurements);
//*******************************************************************/

//Shared Memory *****************************************************/
#define SHMKEY 4711
#define PIPMODE 0600

typedef struct {
	bool logosDetected;
    int logos[4];
    long timestampMemoryInsertion_Sec;
    long timestampMemoryInsertion_Nsec;
    } gLogoDetectionDataset;

gLogoDetectionDataset *gLogoDetection;

int gFd_FIFO;
int gShmId;
char gMsgBuffer_FIFO[16];
int gReadBytes_FIFO;//currently not used

int initializeFIFOAndSharedMemory();
void detachSharedMemoryAndClosePipe();
//*******************************************************************/

//UDP ***************************************************************/
#define PORT 8080
#define BUFFER_SIZE 1024

int gFd_Sock;
struct sockaddr_in gServerAddr, gClientAddr;
char gResvDataBuffer[BUFFER_SIZE];
char gSendDataBuffer[BUFFER_SIZE];

int createSocketFileDescriptor();
void initializeServerInfo();
int bindSocketToAdress();
int receiveMsg();
int sendHelloMsg();
//int detectLogo();
int sendMsg();
int sendLogos();
//*******************************************************************/

int main()
{
    pid_t parentProcessId = getpid();
    pid_t childProcessId = fork();

    if (childProcessId < 0)
	{
    	perror("Error with fork.\n");
    	exit(EXIT_FAILURE);
	}
    else if(childProcessId == 0)
    {
        if (execlp("python3", "python3", PathToObjectDetectionModel, (char*)NULL) == -1)
		{
			perror("Error executing Python script.\n");
			exit(EXIT_FAILURE);
		}
    }
	else
	{
        if (initializeFIFOAndSharedMemory() != 0)
        {
            printf("Error with initializeFIFOAndSharedMemory().\n");
            exit(EXIT_FAILURE);
        }

        ///////////////////////////////////////////////////////////////////
        if (createSocketFileDescriptor() != 0)
        {
            exit(EXIT_FAILURE);
        }

        initializeServerInfo();

        if(bindSocketToAdress() != 0)
        {
            exit(EXIT_FAILURE);
        }

        if(receiveMsg() != 0)//HelloMsg from Client
        {
            exit(EXIT_FAILURE);
        }

        if(sendHelloMsg() != 0)
        {
            exit(EXIT_FAILURE);
        }
        ///////////////////////////////////////////////////////////////////

		while(timeMeasurementIndex < NUM_MEASUREMENTS)
        {
            gReadBytes_FIFO = read(gFd_FIFO, gMsgBuffer_FIFO, sizeof(gMsgBuffer_FIFO));

            if (gMsgBuffer_FIFO[0] == 'D')
            {
				//Measures the difference between the time required by the
				//child process to write the results of the object recognition model
				//to the shared memory and the time until they are read out here
                gEndTimestamp_Sec = getTimestamp_Sec();
                gEndTimestamp_Nsec = getTimestamp_Nsec();

				gAccessTime_SharedMemory[timeMeasurementIndex].seconds = gEndTimestamp_Sec - gLogoDetection->timestampMemoryInsertion_Sec;
				gAccessTime_SharedMemory[timeMeasurementIndex].nanoseconds = gEndTimestamp_Nsec - gLogoDetection->timestampMemoryInsertion_Nsec;

				//the timestamps for the round trip time are located in the sendLogos and receiveMsg functions
                if(sendLogos() != 0)
                {
                    break;
                }

				if(receiveMsg() != 0)
        		{
            		break;
        		}

				gRoundTripTime_UDP[timeMeasurementIndex].seconds = gEndTimestamp_Sec - gStartTimestamp_Sec;
				gRoundTripTime_UDP[timeMeasurementIndex].nanoseconds = gEndTimestamp_Nsec - gStartTimestamp_Nsec;

				timeMeasurementIndex++;
            }
            else
            {
                break;
            }
        }
    
        detachSharedMemoryAndClosePipe();
        close(gFd_Sock);
		if (kill(childProcessId, SIGKILL) == -1)
		{
            perror("Error killing the process");
		}

		printf("\n%d measurements:\n", timeMeasurementIndex);
		printf("\nAccess Time for Shared Memory:n");
		calculateAndPrintAverage(gAccessTime_SharedMemory, timeMeasurementIndex);

		printf("\nRound Trip Time for UDP Connection:\n");
		calculateAndPrintAverage(gRoundTripTime_UDP, timeMeasurementIndex);
		
	    return 0;
	}
}

int receiveMsg()
{
	int result = -1;
	int errorFlag = -1;
	int len = sizeof(gClientAddr);
	
	result = recvfrom(gFd_Sock, (char *)gResvDataBuffer, BUFFER_SIZE, MSG_WAITALL, ( struct sockaddr *) &gClientAddr, &len);
	if (result > 0)
	{
		gEndTimestamp_Sec = getTimestamp_Sec();
		gEndTimestamp_Nsec = getTimestamp_Nsec();

		gResvDataBuffer[result] = '\0';
		printf("Client: %s\n", gResvDataBuffer);
		errorFlag = 0;
	}
	else if (result == 0)
	{
		printf("Error connection closed by the peer.\n");
	}
	else if (result == -1)
	{
		perror("Error recvfrom failed.\n");
	}

	return errorFlag;
}

int sendLogos()
{
	int result = -1;
	int errorFlag = -1;
	int len = sizeof(gClientAddr);

    if (gLogoDetection == NULL)
    {
        printf("Error: gLogoDetection is NULL.\n");
        return errorFlag;
    }

    memcpy(gSendDataBuffer, gLogoDetection->logos, sizeof(gLogoDetection->logos));

	gStartTimestamp_Sec = getTimestamp_Sec();
    gStartTimestamp_Nsec = getTimestamp_Nsec();
    result = sendto(gFd_Sock, (const char *)gSendDataBuffer, sizeof(gLogoDetection->logos), MSG_CONFIRM, (const struct sockaddr *) &gClientAddr, len);
	if(result == sizeof(gLogoDetection->logos))
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

void calculateAndPrintAverage(gTimeMeasurement *timeArray, int numMeasurements)
{
    unsigned long totalSeconds = 0;
    unsigned long totalNanoseconds = 0;

    for (int i = 0; i < numMeasurements; i++)
	{
        totalSeconds += timeArray[i].seconds;
        totalNanoseconds += timeArray[i].nanoseconds;
    }

    //Normalize nanoseconds if greater than or equal to 1 second
    while (totalNanoseconds >= 1000000000)
	{
        totalNanoseconds -= 1000000000;
        totalSeconds++;
    }

    long averageSeconds = totalSeconds / numMeasurements;
    long averageNanoseconds = totalNanoseconds / numMeasurements;

    printf("Total time: %lu seconds, %lu nanoseconds\n", totalSeconds, totalNanoseconds);
    printf("Average time: %ld seconds, %ld nanoseconds\n", averageSeconds, averageNanoseconds);
}

void detachSharedMemoryAndClosePipe()
{
    if (shmdt(gLogoDetection) == -1)
    {
        perror("Error detaching SHM.\n");
    }

    if (close(gFd_FIFO) == -1)
    {
        perror("Error closing FIFO.\n");
    }

    if (unlink("FIFO") == -1)
    {
        perror("Error deleting FIFO.\n");
    }
}

int initializeFIFOAndSharedMemory()
{
    int errorFlag = -1;

    //If the program is terminated with Ctrl+C
	shmdt(gLogoDetection);
    close(gFd_FIFO);
    unlink("/home/raspberry/Desktop/GitHub_UDP/FIFO");

    if (mkfifo("/home/raspberry/Desktop/GitHub_UDP/FIFO", PIPMODE) < 0)
    {
        perror("Error creating FIFO.\n");
    }
    else
    {
		/*Opening the read or write end of a FIFO blocks until the
       	other end is also opened (by another process or thread).*/
        if ((gFd_FIFO = open("/home/raspberry/Desktop/GitHub_UDP/FIFO", O_RDONLY, 0)) < 0)
        {
            perror("Error opening FIFO for reading.\n");
        }
        else
        {
            if ((gShmId = shmget(SHMKEY, sizeof (gLogoDetectionDataset), IPC_CREAT | PIPMODE)) < 0)
            {
                perror("Error requesting SHM.\n");
            }
            else
            {
                if ((gLogoDetection = (gLogoDetectionDataset*) shmat(gShmId, NULL, 0)) < (gLogoDetectionDataset*) NULL)
                {
                    perror("Error attaching SHM.\n");
                }
                else
                {
                    errorFlag = 0;
                }
            }
        }
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

void initializeServerInfo()
{
	memset(&gServerAddr, 0, sizeof(gServerAddr));
	memset(&gClientAddr, 0, sizeof(gClientAddr));
		
	gServerAddr.sin_family = AF_INET;
	gServerAddr.sin_addr.s_addr = INADDR_ANY;
	gServerAddr.sin_port = htons(PORT);
}

int bindSocketToAdress()
{
	int result = -1;
	int errorFlag = -1;

	result = bind(gFd_Sock, (const struct sockaddr *)&gServerAddr, sizeof(gServerAddr));
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

int sendHelloMsg()
{
	int result = -1;
	int errorFlag = -1;
	int len = sizeof(gClientAddr);
	char *helloMsg = "Hello from server.";

	result = sendto(gFd_Sock, (const char *)helloMsg, strlen(helloMsg), MSG_CONFIRM, (const struct sockaddr *) &gClientAddr, len);
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

/*
int detectLogo()
{
	int errorFlag = -1;

	FILE *read_fp;
	int chars_read;

	memset(gSendDataBuffer, '\0', sizeof(gSendDataBuffer));

	read_fp = popen("python3 /home/raspberry/Desktop/YOLO/Ultralytics_YOLOv8/LogoRecognitionFromPicture.py", "r");

	if(read_fp != NULL)
	{
		chars_read = fread(gSendDataBuffer, sizeof(char), BUFFER_SIZE - 1, read_fp);
		if (chars_read > 0)
		{
			gSendDataBuffer[chars_read] = '\0';
			printf("Data received is: %s", gSendDataBuffer);
			errorFlag = 0;
		}

		if (pclose(read_fp) == -1)
		{
			printf("pclose() failed.\n");
			errorFlag = -1;
		}
	}
	else
	{
		printf("popen() failed.\n");
	}
	return errorFlag;
}
*/

int sendMsg()
{
	int result = -1;
	int errorFlag = -1;
	int len = sizeof(gClientAddr);

	result = sendto(gFd_Sock, (const char *)gSendDataBuffer, strlen(gSendDataBuffer), MSG_CONFIRM, (const struct sockaddr *) &gClientAddr, len);
		if(result == strlen(gSendDataBuffer))
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

long getTimestamp_Sec()
{
	struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec;
}

long getTimestamp_Nsec(void)
{
	struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_nsec;
}