#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <regex.h>
#include <math.h>
#include <limits.h>
#include <sys/stat.h>

#include "mtqueue.h"

#define RESPONDER_THREADS	6
#define SERVICE_PORT		8080
#define REQUEST_MAX_SIZE	4096

char fillInResponse[] =
"HTTP/1.1 %d %s\r\n\
Server:Paul McHugh's Simple HTTP Server\r\n\
Content-Length: %d\r\n\
Content-Type: text/html; charset=utf-8\r\n\
\r\n\
%s";

//global variables
pthread_t responders[RESPONDER_THREADS];
pthread_t connection;
MTQueue *gClientQueue;
int gServerSock;
bool gShutdownServer = false;
regex_t gHasEndOfHeader;
regex_t gParentPath;

//forward declarations
void* responder_start(void* inPtr);
void* connection_start(void* inPtr);
char *generateResponse(const char* content, size_t contentLength, int statusCode, char* statusDesc, size_t descLength, size_t* bufferLen);
bool extractRequestedFromHeader(char* header, char** rName);
bool readCheck(char* filename);


int main(int argc, char* argv[])
{
	//compile the regular expressions
	regcomp(&gHasEndOfHeader, ".*\r\n\r\n.*", REG_EXTENDED|REG_NOSUB);
	regcomp(&gParentPath, ".*/(\\.\\.|~)/.*", REG_EXTENDED|REG_NOSUB);

	//create the server socket
	gServerSock = socket(AF_INET,SOCK_STREAM,0);
	if(gServerSock<0)
	{
		fprintf(stderr,"Error: could not create socket errno#%d (%s)\n",errno,strerror(errno));
		exit(1);
	}

	//bind socket to the appropriate port
	struct sockaddr_in addr = { .sin_family = PF_INET, .sin_port=htons(SERVICE_PORT), .sin_addr.s_addr = htonl(INADDR_ANY)};
	if(bind(gServerSock,(struct sockaddr *)&addr,sizeof(addr)))
	{
		//something went wrong binding the socket
		//close the socket and fail
		fprintf(stderr,"Error: could bind socket errno#%d (%s)\n",errno,strerror(errno));
		close(gServerSock);
		exit(1);
	}
	//start threading
	//first setup the mt_queue
	gClientQueue = mtQueueCreate();
	//now start the responder threads
	for (int i = 0; i< sizeof(responders); i++)
	{
		pthread_create(&responders[i],NULL,&responder_start,NULL);
	}

	//connection_start(NULL);
	//now start the connection thread;
	pthread_create(&connection,NULL,&connection_start,NULL);

	//this
	while (!gShutdownServer)
	{
		//promt the user to shutdown the server
		printf("To shutdown the server type enter \'shutdown\':\n");
		char input[9];
		scanf("%8s",input);
		if (strcmp("shutdown", input) == 0)
		{
			gShutdownServer = true;
			printf("Shutting Down Server\n");
		}
		else
		{
			printf("Server Running\n");
		}
	}

	//tear down socket and work semaphore
	shutdown(gServerSock,0);
	//shutdown server
	pthread_join(connection, NULL);
	//join responders
	for(int i = 0; i < sizeof(responders); i++)
	{
		pthread_join(responders[i],NULL);
	}
	//destroy mt_queue
	mtQueueDestroy(gClientQueue,NULL);

	return 0;
}

void* responder_start(void* inPtr)
{
	//setup buffers
	char* buffer	= malloc(REQUEST_MAX_SIZE);
	char* request	= malloc(REQUEST_MAX_SIZE+1);
	if(buffer==(void*)-1||request==(void*)-1)
	{
		//This shouldn't happen
		fprintf(stderr,"Error: Failed to allocate buffer. errno#%d (%s)\n",errno,strerror(errno));
		exit(1);
	}
	//main loop where we service the requests
	while (!gShutdownServer)
	{
		//get an item off the mtQueue if one is available
		struct timespec max_wait = {.tv_sec=0,.tv_nsec=100000000};
		nanosleep(&max_wait,NULL);
		void* value = dequeueHead(gClientQueue);
		if (value!=NULL)
		{
			//ok we got an actual socket back
			int sock = (int) ((unsigned long long)value);
			size_t totalBytesRead = 0;
			int coa = 0;//coa=course of action,0=continue reading,1=do response,2=error message,3=too long error message
			do
			{
				//read the input to the buffer
				ssize_t bytes_rcvd = recv(sock,buffer,REQUEST_MAX_SIZE,0);
				if (bytes_rcvd<0&&errno!=EINTR)//we have a serious error
				{
					coa = 2;
				}
				else if (totalBytesRead+bytes_rcvd>REQUEST_MAX_SIZE)//the request is too long
				{
					coa = 3;
				}
				else//append to buffer and decide if we have the full request header
				{
					memcpy(request+totalBytesRead,buffer,(size_t)bytes_rcvd);
					totalBytesRead+=bytes_rcvd;
					request[totalBytesRead] = 0;
					coa = (strstr(request,"\r\n\r\n")!=NULL)?1:0;//if we have the end of the header then we are ready to parse it.
				}
			}
			while (coa==0);
			//error responses
			//if the coa was two then we dont do anything b/c the request was probably timed out/otherwise failed
			if(coa==3)//request too long
			{
				size_t length;
				char* response = generateResponse("Request too Long", 16, 400, "Request too Long", 16, &length);
				send(sock,response,length,0);
				free(response);
			}
			else if(coa==1)
			{
				//parse and respond to the request
				char* reqPath;
				bool is404 =false;
				if (extractRequestedFromHeader(request,&reqPath))//make sure to free reqPath if this function succeeds
				{
					//make sure the path is not malicious
					if(regexec(&gParentPath,reqPath,0,NULL,0)!=0)
					{
						if (!readCheck(reqPath))
							is404=true;
					}
					else
					{
						is404=true;
						free(reqPath);
					}

				}
				else is404=true;
				if (is404)
				{
					//error message 404
					size_t length;
					char* response = generateResponse("<html><body>404 Not Found</body></html>", 39, 404, "Not Found", 9, &length);
					send(sock,response,strlen(response),0);
					free(response);
				}
				else
				{
					size_t HTTPPacketLength;
					struct stat fData;
					//open the target file and read it into a buffer
					stat(reqPath, &fData);
					size_t fileLength = (size_t)fData.st_size;
					FILE* sendFile = fopen(reqPath,"r");
					char* fileBuffer = malloc(fileLength+1);
					fread(fileBuffer,1,fileLength,sendFile);
					fclose(sendFile);//close the open file
					//craft the response and send it
					char* response = generateResponse(fileBuffer, fileLength, 200, "OK", 2, &HTTPPacketLength);
					send(sock,response,strlen(response),0);
					//clean up
					free(fileBuffer);
					free(response);
					free(reqPath);
				}

			}
			//end request handler/clean up resources
			shutdown(sock,0);
		}
		//try again
	}
	//clean up buffers
	free(buffer);
	free(request);
	return NULL;
}

void* connection_start(void* inPtr)
{
	//start listening
	listen(gServerSock,3);
	//put the accepted requests in the processing queue
	while (!gShutdownServer)
	{
		int connSock = accept(gServerSock, NULL, NULL);
		//put the fd of the connection socket in the value field.  It is not a pointer, but this works
		enqueueTail(gClientQueue, (void*)((unsigned long long)connSock));
	}
	return NULL;
}

char *generateResponse(const char *content, size_t contentLength, int statusCode, char *statusDesc, size_t descLength, size_t *bufferLen)
{
	*bufferLen = sizeof(fillInResponse)+contentLength+descLength+(int)ceil(log(statusCode))+(int)ceil(log(contentLength));
	char* responseBuffer = malloc(*bufferLen);
	sprintf(responseBuffer,fillInResponse,statusCode,statusDesc,contentLength,content);
	return responseBuffer;
}

bool extractRequestedFromHeader(char* header, char** rName)
{
	regex_t headerFirstLine;
	regmatch_t matches[2];
	bool rv;
	regcomp(&headerFirstLine, "GET[[:blank:]]*([a-zA-Z$_.+!*'(),/-]*)[[:blank:]]*HTTP/[[:digit:]]\\.[[:digit:]]", REG_EXTENDED);
	if(regexec(&headerFirstLine,header,2,matches,0)==0)
	{
		//get the sise of the resource string
		size_t rSize = (size_t)matches[1].rm_eo-matches[1].rm_so;
		*rName = malloc(rSize);
		(*rName)[0] = '.';
		memcpy(*rName,header+matches[1].rm_so+1,rSize);
		(*rName)[rSize-1] = '\0';
		rv=true;
	}
	else
	{
		rv=false;
	}
	regfree(&headerFirstLine);
	return rv;
}

//Checks if the file can be read from and is a regular file
bool readCheck(char* filename)
{
	if (access(filename,R_OK)==0)
	{
		//check if it is a normal directory
		struct stat fileData;
		stat(filename,&fileData);
		return S_ISREG(fileData.st_mode);
	}
	else return false;
}
