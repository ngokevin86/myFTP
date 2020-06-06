/*Kevin Ngo
 *Assignment Final - Server/Client File Explorer (Server)
 *Completed April 28th, 2019
 */

/*
This program is the server portion of a basic file transfer system. The client is "mftp.c".
The program, when compiled with the Makefile, can be run doing
	./server
	
This server is set to listen (connect) up to 4 client connections, but could theoretically handle more (untested).
Both client and server have two socket connections
	-One socket (command) is for sending commands and acknowledgements
	-One secket (data) is for sending actual info (files, datapaths, etc)
		-Data socket is created upon a connection which sends/receives data, and destroyed/ended upon finishing

SERVER COMMAND SYNTAX
	-No leading spaces
	-Single Character Command Specifier
	-Optional Param (which follows the command specifier, no whitespace)
	-Command terminator is either newline ('\n') or EOF
	
	Command Specifiers:
		-"D" Establish data connection
			-Server acknowledges by sending an ASCII code decimal integer, representing the data port

		-"C<pathname>" Change directory
			-Server responds by executing a chdir() sys call w/ given path

		-"L" List directory
			-Server responds by initiating a child process to execute "ls -l" and sending the result to the client
		
		-"G<pathname>" Get a file
			-Server gets a filepath from the client, and then attempts to open and send the file contents to the client
			-Server will reject if file does not exist, is not readable, or is a directory

		-"P<pathname>" Put a file
			-Server will get a filepath, creating a file with the name of the last item of the filepath
			-Server will reject if file of the same name already exists in the directory

		-"Q" Quit
			-Ends the child process

	-All commands are logged by server (to stdout), along with arguments and if it succeeded or failed
	
SERVER RESPONSE SYNTAX
	-(Control) Acknowledgment or error response
		-Error responses start with 'E' + error message (which is to be printed by client) + newline character
		-Acknowledgment start with 'A' + optional ASCII-code decimal int + newline character
*/

#include "mftp.h"

int main(){
	struct sockaddr_in servAddress;
	int listenfd;	//listen socket
	int status;
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if(listenfd < 0){
		fprintf(stderr, "Error with socket: %s\n", strerror(errno));
		return errno;
	}
	memset(&servAddress, 0, sizeof(servAddress));
	servAddress.sin_family = AF_INET;
	servAddress.sin_port = htons(MY_PORT_NUMBER);
	servAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(listenfd, (struct sockaddr *) &servAddress, sizeof(servAddress)) < 0){
		fprintf(stderr, "Error with bind: %s\n", strerror(errno));
		return errno;
	}
	if((listen(listenfd, 4)) < 0){	//number of allowed connections, currently set to four
		fprintf(stderr, "Error with listen: %s\n", strerror(errno));
		return errno;
	}
	int length = sizeof(struct sockaddr_in);
	struct sockaddr_in clientAddress;
	int comSocketfd;
	while(1){	//Keep looking for connections
		printf("Looking for connections...\n");
		comSocketfd = accept(listenfd, (struct sockaddr *) &clientAddress, &length);	//waits until connection from client, gives new fd
		if(comSocketfd < 0){
			fprintf(stderr, "Error getting client fd: %s\n", strerror(errno));
			return errno;
		}
		if(fork()){	//parent
			if(close(comSocketfd) < 0){
				fprintf(stderr, "Error closing comSocketfd: %s\n", strerror(errno));
				return errno;
			}
			if((waitpid(-1, &status, 0)) < 0){	//clean zombies
				fprintf(stderr, "Error with waitpid: %s\n", strerror(errno));
				return errno;
			}
			continue;
		}
		if(errno != 0){
			fprintf(stderr, "Error forking upon connection: %s\n", strerror(errno));
			return errno;
		}
		else{	//child, retrieve client info
			if(fork()){	//double fork magic
				exit(0);
			}
			else{
				int childPID = getpid();	//grabs pid for tracking
				printf("Child PID '%d' started\n", childPID);
				struct hostent* hostEntry;
				char* hostName;
				hostEntry = gethostbyaddr(&(clientAddress.sin_addr), sizeof(struct in_addr), AF_INET);
				hostName = hostEntry->h_name;
				printf("PID %d > Connected to client name: '%s'\n", childPID, hostName);
				int datafd;	//holds data socket fd, if created
				while(1){
					char buf[1];
					char first[2] = {0};	//holds command specifier
					int firstFlag = 0;
					char filePath[BUF_SIZE] = {0};	//holds filepath, if given
					int curr = 0;
					if(DEBUG) printf("PID %d > Waiting for next command.\n", childPID);
					int check = 0;
					while((check = read(comSocketfd, buf, 1)) > 0){
						if(buf[0] == '\n'){
							break;
						}
						if(firstFlag == 0){	//copy command letter
							first[0] = buf[0];
							firstFlag = 1;
							first[1] = '\0';
						}
						else{
							filePath[curr] = buf[0];
							curr++;
						}
					}
					if(check == -1){	//handles if client randomly dies
						printf("Client unexpectedly died\n");
						exit(0);
					}
					if(DEBUG) printf("Command: %s, filePath: %s\n", first, filePath);
					//---Exit/quit
					if(first[0] == 'Q'){
						printf("PID %d > Request to exit received\n", childPID);
						write(comSocketfd, "A\n", 2);
						close(comSocketfd);
						exit(0);
					}
					//---Data connection
					else if(first[0] == 'D'){
						if(DEBUG) printf("PID %d > Requested to open a data socket\n", childPID);
						struct sockaddr_in newAddr;
						int newdatafd = socket(AF_INET, SOCK_STREAM, 0);
						if(newdatafd < 0){
							fprintf(stderr, "Error with socket: %s\n", strerror(errno));
							return errno;
						}
						memset(&newAddr, 0, sizeof(newAddr));
						newAddr.sin_family = AF_INET;
						newAddr.sin_port = htons(0);
						newAddr.sin_addr.s_addr = htonl(INADDR_ANY);
						bind(newdatafd, (struct sockaddr *) &newAddr, sizeof(newAddr));
						struct sockaddr_in newSock;
						int temp = sizeof(newSock);
						getsockname(newdatafd, (struct sockaddr *) &newSock, &temp);
						int sock = ntohs(newSock.sin_port);
						if(DEBUG) printf("PID %d > Sending data socket port number '%d'\n", childPID, sock);
						char sockRes[10] = {0};
						sockRes[0] = 'A';
						char sockStr[10];
						sprintf(sockStr, "%d", sock);	//convert from integer to string
						strncat(sockRes, sockStr, sizeof(sockStr));
						sockRes[1 + sizeof(sockStr)] = '\n';
						write(comSocketfd, sockRes, (2 + sizeof(sockStr)));	//send acknowledgement and port number
						listen(newdatafd, 1);
						datafd = accept(newdatafd, (struct sockaddr *) &newAddr, &length);	//accept connection to data socket
						if(DEBUG) printf("PID %d > Finished sending data socket port number\n", childPID);
					}
					//---Change directory
					else if(first[0] == 'C'){
						if(DEBUG) printf("PID %d > Requested to change directory\n", childPID);
						int err = chdir(filePath);	//change local directory
						if(err == -1){
							char errorMsg[BUF_SIZE] = {0};
							errorMsg[0] = 'E';
							int errLen = strlen(strerror(errno));
							strncat(errorMsg, strerror(errno), sizeof(errorMsg) - 1);
							errorMsg[1 + errLen] = '\n';
							errorMsg[BUF_SIZE - 1] = '\0';
							write(comSocketfd, errorMsg, (errLen + 2));
							fprintf(stderr, "PID %d > Error attempting to change directory: %s", childPID, errorMsg);
						}
						else{
							write(comSocketfd, "A\n", 2);
							printf("PID %d > Directory successfully changed to %s\n", childPID, filePath);
						}
					}
					//---List directory
					else if(first[0] == 'L'){
						if(DEBUG) printf("PID %d > Requested to list directory\n", childPID);
						if(fork()){	//parent
							wait(NULL);	//wait on child
							if(DEBUG) printf("PID %d > Finished sending ls\n", childPID);
							write(comSocketfd, "A\n", 2);
							close(datafd);	//close data socket
						}
						else{	//child
							if((close(0)) == -1){	//not reading. close it
								fprintf(stderr, "Error child close: %s\n", strerror(errno));
								exit(errno);
							}
							if((dup2(datafd, 1)) == -1){	//redirect to data socket
								fprintf(stderr, "Error child dup: %s\n", strerror(errno));
								exit(errno);
							}
							if((execlp("ls", "ls", "-l", (char *)NULL)) == -1){	//execute 'ls -l'
								fprintf(stderr, "Error exec child: %s\n", strerror(errno));
								exit(errno);
							}
						}
					}
					//--Get file
					else if(first[0] == 'G'){
						if(DEBUG) printf("PID %d > Requested to get file '%s'\n", childPID, filePath);
						int accErr = access(filePath, R_OK);	//see if given file at filepath is readable & exists
						if(accErr == -1){	//filepath doesn't exist, send error to client
							char errorMsg[BUF_SIZE] = {0};
							errorMsg[0] = 'E';
							int errLen = strlen(strerror(errno));
							strncat(errorMsg, strerror(errno), sizeof(errorMsg) - 1);
							errorMsg[1 + errLen] = '\n';
							errorMsg[BUF_SIZE - 1] = '\0';
							write(comSocketfd, errorMsg, (errLen + 2));
							if(DEBUG) printf("PID %d > Error attempting to get file: %s", childPID, errorMsg);
							close(datafd);
							continue;
						}
						else{	//filepath exists, continue on
							struct stat fileInfo;
							lstat(filePath, &fileInfo);	//grabs lstat info of filepath
							int res = S_ISREG(fileInfo.st_mode);	//see if it's a file or dir
							if(res == 0){	//0 means it fails S_ISREG
								write(comSocketfd, "ENot a file\n", 12);
								if(DEBUG) printf("PID %d > Error attempting to get a file: Not a file\n", childPID);
								close(datafd);	//close data socket
								continue;
							}
							printf("PID %d > Attempting to get file from '%s'\n", childPID, filePath);
							int putFileD = open(filePath, O_RDONLY);	//open file for read only
							write(comSocketfd, "A\n", 2);	//tell client to get ready
							char buf[1];
							while(read(putFileD, buf, 1) > 0){
								if(write(datafd, buf, 1) < 0){
									if(DEBUG) printf("PID %d > Write error: %s\n", childPID, strerror(errno));
									break;
								}
							}
							close(putFileD);	//close opened file
							if(close(datafd) < 0){	//close data socket
								fprintf(stderr, "Error closing a data socket: %s\n", strerror(errno));
								exit(errno);
							}
							if(DEBUG) printf("PID %d > 'get' finished\n", childPID);
						}
					}
					//--Put file
					else if(first[0] == 'P'){
						char temp[BUF_SIZE];
						strncpy(temp, filePath, sizeof(temp) - 1);
						char fileName[BUF_SIZE] = {0};
						int counter = 0;
						if(DEBUG) printf("PID %d > Requested to put file '%s'\n", childPID, filePath);
						for(int i = 0; i < sizeof(temp); i++){	//copies last item in filepath
							if(temp[i] == '/'){
								counter = 0;
							}
							if(temp[i] == '\0' || temp[i] == '\n'){
								break;
							}
							fileName[counter] = temp[i];
							counter++;
						}
						if(DEBUG) printf("PID %d > Filename: %s\n", childPID, fileName);
						int accErr = access(fileName, F_OK);	//see if given file exists
						if(accErr == 0){	//file does exist
							write(comSocketfd, "EFile already exists\n", 21);
							if(DEBUG) printf("PID %d > File already exists\n", childPID);
							close(datafd);
							continue;
						}
						else{	//file doesn't exist, continue on
							printf("PID %d > Creating file %s\n", childPID, fileName);
							int newFileD = open(fileName, O_CREAT | O_WRONLY, 0700);
							char buf[1];
							write(comSocketfd, "A\n", 2);
							while(read(datafd, buf, 1) > 0){
								if(write(newFileD, buf, 1) < 0){
									fprintf(stderr, "PID %d > write error: %s\n", childPID, strerror(errno));
									break;
								}
							}
							close(newFileD);	//close new file
							if(close(datafd) < 0){	//close data socket
								fprintf(stderr, "Error closing a data socket: %s\n", strerror(errno));
								exit(errno);
							}
							if(DEBUG) printf("PID %d > 'put' finished\n", childPID);
						}
					}
					else{
						if(DEBUG) printf("PID %d > Invalid request from client, notifying...\n", childPID);
						char* errormsg;
						strcpy(errormsg, "EInvalid command received\n");
						write(comSocketfd, errormsg, strlen(errormsg));
					}
				}	//end of while(1) loop
			}
		}	
	}
	return 0;
}
