/*Kevin Ngo
 *Assignment Final - Server/Client File Explorer (Client)
 *Completed April 28th, 2019
 */

/*
This program is the client portion of a basic file transfer system. The server is "mftpserve.c".
The program, when compiled with the Makefile, can be run doing
	./client <serverIP>
where <serverIP> is the IP address of the server.
Localhost can be used for <serverIP> if the server is the same as the client.

There are a variety of commands available for the client.
CLIENT COMMANDS
-exit
	-this lets the server know the client is shutting down, and then shuts down the client.
-cd <pathname>
	-change the local current working directory
	-errors if path doesn't exist or not readable
-rcd <pathname>
	-change the server's working directory
	-error if path doesn't exist or not readable.
-ls
	-executes "ls -l" on local directory, displaying 20 lines at a time
	-displays the 20 more lines upon receiving a space character
-rls
	-executes "ls -l" on server directory, displaying 20 lines at a time
	-displays 20 more lines upon receiving a space character
-get <pathname>
	-goes and gets file from pathname, storing the file on the local directory.
	-error if path doesn't exist, not a regular file, or is not readable
	-error if file cannot be opened/created in client's directory
-show <pathname>
	-retrieve contents of file from pathname, printing it 20 lines at a time
	-displays 20 more lines upon receiving a space character
-put <pathname>
	-goes and puts file from pathname onto the currect directory on the server
	-error if file doesn't exist
*/

#include "mftp.h"

int main(int argc, char *argv[]){
	if(argc < 2){	//check if there wasn't enough arguments
		printf("Error: Not enough arguments\n");
		return 1;
	}
	if(argc > 4){	//check if there were too many arguments
		printf("Error: Too many arguments\n");
		return 1;
	}
	int comSocketfd;	//socket file descriptor for sending commands and receiving A/E and data sockets
	comSocketfd = socket(AF_INET, SOCK_STREAM, 0);
	if(comSocketfd < 0){
		fprintf(stderr, "Error with command/control socket: %s\n", strerror(errno));
		return errno;
	}
	int succErr = connectSock(&comSocketfd, MY_PORT_NUMBER, argv[1]);	//connects socket with function
	if(succErr == -1){	//only occurs if connectSock had an error
		printf("Error connecting\n");
		return 2;
	}
	printf("Connected to host '%s', ready for a command.\n", argv[1]);

	while(1){	//infinite loop, keeps reprompting until exit or fatal error
		char input[BUF_SIZE];
		printf("Input > ");
		fgets(input, BUF_SIZE, stdin);	//grabs input from user
		char* token = strtok(input, " ");	//tokenize input for easier parsing
		if(DEBUG) printf("Received input: '%s'", input);
		if(DEBUG) printf("\n");	//because above will print the inputted new line
		//---Exit/quit command
		if(strcmp("exit\n", token) == 0){
			printf("Exiting from server, shutting down client...\n");	//notify user
			if(write(comSocketfd, "Q\n", 2) == -1){	//send quit message to server
				fprintf(stderr, "Error sending 'exit' command to server: %s\n", strerror(errno));
				return errno;
			}
			char socketBuf[BUF_SIZE];
			if(read(comSocketfd, socketBuf, BUF_SIZE) == -1){	//wait for server response
				fprintf(stderr, "Error waiting for server response: %s\n", strerror(errno));
				return errno;
			}
			if(socketBuf[0] == 'A'){
				if(DEBUG) printf("Server acknowledged 'exit' command\n");
			}
			else{
				printf("Server had an error with 'exit' command: %s\n", socketBuf);
			}
			break;	//break from while loop to go close comSocketfd
		}
		//---Local 'cd' command
		else if(strcmp("cd", token) == 0){	//local cd command
			if(DEBUG) printf("\n'cd' command received\n");
			if((token = strtok(NULL, " \n")) != NULL){	//grab next token if available
				if(DEBUG) printf("attempting to cd to '%s'...\n", token);
				int accErr = access(token, F_OK);	//see if given directory exists
				if(accErr == -1){	//directory doesn't exist
					fprintf(stderr, "Error accessing given directory: '%s'\n", strerror(errno));
					continue;
				}
				int chdErr = chdir(token);	//since it exists, change the current directory to it
				if(chdErr == -1){	//failed changing to directory
					fprintf(stderr, "Error changing directory: '%s'\n", strerror(errno));
					continue;
				}
				if(DEBUG) printf("Directory changed to '%s'\n", token);	//notify finished local cd
			}
			else{	//missing input
				printf("Error: missing directory parameter.\n");
			}

		}
		//---Server 'cd' command
		else if(strcmp("rcd", token) == 0){	//server cd command
			if(DEBUG) printf("\n'rcd' command received\n");
			if((token = strtok(NULL, " \n")) != NULL){	//grab next token if available
				if(DEBUG) printf("attempting to rcd to '%s'...\n", token);
				int writeSize = 2 + strlen(token);
				if(DEBUG) printf("writeSize %d\n", writeSize);
				char toWrite[writeSize + 2];
				strcpy(toWrite, "C");
				strcat(toWrite, token);
				strcat(toWrite, "\n");
				if(DEBUG) printf("Sending directory '%s' to server\n", toWrite);
				if(write(comSocketfd, toWrite, writeSize) == -1){	//send directory to server
					fprintf(stderr, "Error sending directory to server: %s\n", strerror(errno));
					return errno;
				}
				char res[1];
				int firstLet = 0;
				while((read(comSocketfd, res, 1)) > 0){
					if(firstLet == 0){	//ignore first character
						firstLet = 1;
						continue;
					}
					write(1, res, 1);
					if(res[0] == '\n'){
						break;
					}
				}
				if(DEBUG) printf("Finished changing server directory to '%s'\n", token);	//notify finished server cd
			}
			else{
				printf("Error: missing directory parameter.\n");
			}
		}
		//---Local 'ls' command
		else if(strcmp("ls\n", token) == 0){	//local ls command
			printf("'ls' command received\n");
			errno = 0;	//reset errno, to prevent bleedover
			int forkErr = 0;
			if(forkErr = fork()){	//parent
				if(forkErr == -1){
					fprintf(stderr, "Error while forking: %s\n", strerror(errno));
					return errno;
				}
				wait(NULL);	//wait for children to finish
				if(DEBUG) printf("Local 'ls' finished.\n");
				
			}
			else{	//child
				int fd[2];	//holds pipe FD's
				if(pipe(fd) < 0){	//pipe for communicating between 'ls' and 'more'
					fprintf(stderr, "Error while piping: %s\n", strerror(errno));
					exit(errno);
				}
				int fdRead = fd[0];
				int fdWrite = fd[1];
				if(forkErr = fork()){	//if parent, read
					wait(NULL);	//wait for ls to finish executing in the child
					if((close(fdWrite)) == -1){	//not writing, close it
						fprintf(stderr, "Error parent close: %s\n", strerror(errno));
					}
					if((dup2(fdRead, 0)) == -1){
						fprintf(stderr, "Error parent dup: %s\n", strerror(errno));
						exit(errno);
					}
					if((execlp("more", "more", "-20", (char *)NULL)) == -1){	//exec 'more -20'
						fprintf(stderr, "Error exec more in ls: %s\n", strerror(errno));
						exit(errno);
					}
					close(fdRead);	//done, close reader
					exit(1);
				}
				if(errno != 0){
					fprintf(stderr, "Error while forking: %s\n", strerror(errno));
					exit(errno);
				}
				else{	//if child, write
					if((close(fdRead)) == -1){	//not reading. close it
						fprintf(stderr, "Error child close: %s\n", strerror(errno));
						exit(errno);
					}
					if((dup2(fdWrite, 1)) == -1){
						fprintf(stderr, "Error child dup: %s\n", strerror(errno));
						exit(errno);
					}
					if((execlp("ls", "ls", "-l", (char *)NULL)) == -1){	//exec 'ls -l'
						fprintf(stderr, "Error exec child: %s\n", strerror(errno));
						exit(errno);
					}
					close(fdWrite);	//done, close writer
				}
			}
		} 
		//---Server 'ls' command
		else if(strcmp("rls\n", token) == 0){	//server ls command
			if(DEBUG) printf("'rls' command recieved\n");
			if(write(comSocketfd, "D\n", 2) == -1){	//establish data connection
				fprintf(stderr, "Error sending request for data socket to server: %s\n", strerror(errno));
				return errno;
			}
			char socketBuf[BUF_SIZE];	//hold return result from server, either error or port number
			if(read(comSocketfd, socketBuf, BUF_SIZE) == -1){	//read server response
				fprintf(stderr, "Error reading response from server: %s\n", strerror(errno));
				return errno;
			}
			if(socketBuf[0] != 'E'){
				if(DEBUG) printf("Server returned %s", socketBuf);
				char temp[BUF_SIZE];
				int i = 0;
				while(socketBuf[i+1] != '\0'){
					temp[i] = socketBuf[i+1];
					i++;
				}
				int dataSocketfd;
				dataSocketfd = socket(AF_INET, SOCK_STREAM, 0);
				if(dataSocketfd < 0){
					fprintf(stderr, "Error with socket: %s\n", strerror(errno));
					exit(errno);
				}
				int portNum = atoi(temp);	//convert string to int
				if(DEBUG) printf("Converted port number: %d\n", portNum);
				int err = connectSock(&dataSocketfd, portNum, argv[1]);	//connect datasocket to given port
				if(err == -1){
					printf("connectSock had an error\n");
					exit(-1);
				}
				if(write(comSocketfd, "L\n", 2) == -1){	//tell server to start sending data down
					fprintf(stderr, "Error sending 'rls' command to server: %s\n", strerror(errno));
					return errno;
				}
				char socketBuf2[BUF_SIZE] = {0};
				if(read(comSocketfd, socketBuf2, BUF_SIZE) == -1){	//read response
					fprintf(stderr, "Error reading response from server: %s\n", strerror(errno));
					return errno;
				}
				if(socketBuf2[0] == 'E'){	//server gave error
					printf("Server returned an error: %s", socketBuf2);
					continue;
				}
				if(DEBUG) printf("server returned 'A'\n");
				int forkErr;
				if(forkErr = fork()){	//parent
					if(forkErr == -1){
						fprintf(stderr, "Error while forking: %s\n", strerror(errno));
						return errno;
					}
					wait(NULL);	//wait on child
					if(DEBUG) printf("'rls' finished.\n");
					if(close(dataSocketfd) < 0){
						fprintf(stderr, "Error closing a data socket: %s\n", strerror(errno));
						exit(errno);
					}
				}
				else{	//child
					if((close(0)) == -1){	//not reading from stdin. close it
						fprintf(stderr, "Error child rls close: %s\n", strerror(errno));
						exit(errno);
					}
					if((dup2(dataSocketfd, 0)) == -1){
						fprintf(stderr, "Error rls dup: %s\n", strerror(errno));
						exit(errno);
					}
					if((execlp("more", "more", "-20", (char *)NULL)) == -1){	//execute 'more -20' on server return
						fprintf(stderr, "Error exec more in rls: %s\n", strerror(errno));
						exit(errno);
					}
				}
			}
			else{	//server had an error
				char temp[BUF_SIZE];
				int i = 0;
				while(socketBuf[i+1] != '\0'){
					temp[i] = socketBuf[i+1];
				}
				printf("Server returned an error while creating data socket: %s\n", temp);
				exit(-1);
			}
		}
		//--Server to client file transfer
		else if(strcmp("get", token) == 0){	//get file from server
			if(DEBUG) printf("\n'get' command received\n");
			if((token = strtok(NULL, " \n")) != NULL){	//grab next token if available
				char temp[BUF_SIZE];
				strncpy(temp, token, sizeof(temp) - 1);
				char fileName[BUF_SIZE] = {0};
				int counter = 0;
				if(DEBUG) printf("Given filepath: '%s'\n", temp);
				for(int i = 0; i < sizeof(temp); i++){
					if(temp[i] == '/'){
						counter = 0;
					}
					if(temp[i] == '\0' || temp[i] == '\n'){
						break;
					}
					fileName[counter] = temp[i];
					counter++;
				}
				if(DEBUG) printf("Filename from filepath: '%s'\n", fileName);
				int accErr = access(fileName, F_OK);	//see if given file exists in local directory
				if(accErr == 0){	//file does already exist
					printf("File already exists in current directory\n");
					continue;
				}
				else{	//file doesn't already exist in local directory, continue on
					write(comSocketfd, "D\n", 2);	//establish data connection
					char socketBuf[BUF_SIZE];
					read(comSocketfd, socketBuf, BUF_SIZE);	//read port number from server
					if(socketBuf[0] != 'E'){	//if the server didn't return an error
						printf("Server returned %s", socketBuf);
						char temp[BUF_SIZE];	//parse the input to get only the port number
						int i = 0;
						while(socketBuf[i+1] != '\0'){
							temp[i] = socketBuf[i+1];
							i++;
						}
						int dataSocketfd;	//start creating the data socket
						dataSocketfd = socket(AF_INET, SOCK_STREAM, 0);
						if(dataSocketfd < 0){
							fprintf(stderr, "Error with socket: %s\n", strerror(errno));
							exit(errno);
						}
						int portNum = atoi(temp);	//convert input to an int
						if(DEBUG) printf("Converted port number: '%d'\n", portNum);
						int err = connectSock(&dataSocketfd, portNum, argv[1]);	//connect datasocket to given port
						if(err == -1){
							printf("connectSock had an error\n");
							exit(-1);
						}
						if(DEBUG) printf("Attempting to get file from '%s'...\n", token);
						int writeSize = 2 + strlen(token);
						char toWrite[writeSize + 2];
						strcpy(toWrite, "G");
						strcat(toWrite, token);
						strcat(toWrite, "\n");
						if(DEBUG) printf("Sending filepath '%s' to server\n", toWrite);
						write(comSocketfd, toWrite, writeSize);	//write to server 'G<pathname>\n', telling it to start sending info
						char socketBuf2[BUF_SIZE] = {0};
						read(comSocketfd, socketBuf2, BUF_SIZE);
						if(socketBuf2[0] == 'E'){
							printf("Server returned an error: '%s'", socketBuf2);
							continue;
						}
						if(DEBUG) printf("server returned 'A'\n");
						int newFileD = open(fileName, O_CREAT | O_WRONLY, 0700);
						char buf[1];
						while(read(dataSocketfd, buf, 1) > 0){
							write(newFileD, buf, 1);
						}
						close(newFileD);	//close the new retrieved file
						if(close(dataSocketfd) < 0){
							fprintf(stderr, "Error closing a data socket: %s\n", strerror(errno));
							exit(errno);
						}
						if(DEBUG) printf("'get' finished\n");
					}
				}
			}
			else{	//strtok had an error
				printf("Error: missing directory parameter.\n");			
			}
		}
		//---Server gets and sends file to be printed out to stdout
		else if(strcmp("show", token) == 0){	//read file from server
			if(DEBUG) printf("'show' command received\n");
			write(comSocketfd, "D\n", 2);	//establish data connection
			char socketBuf[BUF_SIZE];
			read(comSocketfd, socketBuf, BUF_SIZE);
			if(socketBuf[0] != 'E'){
				if(DEBUG) printf("Server returned %s", socketBuf);
				char temp[BUF_SIZE];
				int i = 0;
				while(socketBuf[i+1] != '\0'){
					temp[i] = socketBuf[i+1];
					i++;
				}
				int dataSocketfd;
				dataSocketfd = socket(AF_INET, SOCK_STREAM, 0);
				if(dataSocketfd < 0){
					fprintf(stderr, "Error with socket: %s\n", strerror(errno));
					exit(errno);
				}
				int portNum = atoi(temp);
				if(DEBUG) printf("Converted port number: %d\n", portNum);
				int err = connectSock(&dataSocketfd, portNum, argv[1]);	//connect datasocket to given port
				if(err == -1){
					printf("connectSock had an error\n");
					exit(-1);
				}
				if((token = strtok(NULL, " \n")) != NULL){	//grab next token if available
					if(DEBUG) printf("Attempting to show to '%s'...\n", token);
					int writeSize = 2 + strlen(token);
					char toWrite[writeSize + 2];
					strcpy(toWrite, "G");
					strcat(toWrite, token);
					strcat(toWrite, "\n");
					if(DEBUG) printf("Sending filepath '%s' to server\n", toWrite);
					write(comSocketfd, toWrite, writeSize);	//write to server 'G<pathname>\n', telling it to start sending info
					char socketBuf2[BUF_SIZE] = {0};
					read(comSocketfd, socketBuf2, BUF_SIZE);
					if(socketBuf2[0] == 'E'){
						printf("Server returned an error: %s", socketBuf2);
						continue;
					}
					if(fork()){	//parent
						wait(NULL);	//wait on child
						printf("'show' finished\n");
						if(close(dataSocketfd) < 0){
							fprintf(stderr, "Error closing a data socket: %s\n", strerror(errno));
							exit(errno);
						}
					}
					else{	//child
						if((close(0)) == -1){	//not reading from stdin. close it
							fprintf(stderr, "Error child rls close: %s\n", strerror(errno));
							exit(errno);
						}
						if((dup2(dataSocketfd, 0)) == -1){
							fprintf(stderr, "Error rls dup: %s\n", strerror(errno));
							exit(errno);
						}
						if((execlp("more", "more", "-20", (char *)NULL)) == -1){
							fprintf(stderr, "Error exec more in rls: %s\n", strerror(errno));
							exit(errno);
						}
					}
				}
				else{ //strtok error
					printf("Error: missing directory parameter.\n");
				}
			}
			else{	//server had an error
				char temp[BUF_SIZE];
				int i = 0;
				while(socketBuf[i+1] != '\0'){
					temp[i] = socketBuf[i+1];
				}
				printf("Server returned an error while creating data socket: %s\n", temp);
				continue;
			}
		}
		//---Client to server file transfer
		else if(strcmp("put", token) == 0){	//put file onto server
			if(DEBUG) printf("'put' command received\n");
			if((token = strtok(NULL, " \n")) != NULL){	//grab next token if available
				int accErr = access(token, R_OK);	//see if given file at filepath exists and is readable
				if(accErr == -1){	//file doesn't exist or is not readable
					printf("File '%s' doesn't exist/is not readable in given directory\n", token);
					continue;
				}
				else{	//file doesn't exist, continue on
					struct stat fileInfo;	//check if file is directory or actual file
					lstat(token, &fileInfo);	//grabs lstat info of filepath
					int res = S_ISREG(fileInfo.st_mode);	//this does the comparison to see if it's a file
					if(res == 0){	//0 means it fails S_ISREG
						printf("Local file '%s' is not a file, ignoring command\n", token);
						continue;
					}
					write(comSocketfd, "D\n", 2);	//establish data connection
					char socketBuf[BUF_SIZE];
					read(comSocketfd, socketBuf, BUF_SIZE);
					if(socketBuf[0] != 'E'){	//if the server didn't return an error
						if(DEBUG) printf("Server returned %s", socketBuf);
						char temp[BUF_SIZE];	//parse the input to get only the port number
						int i = 0;
						while(socketBuf[i+1] != '\0'){
							temp[i] = socketBuf[i+1];
							i++;
						}
						int dataSocketfd;	//start creating the data socket
						dataSocketfd = socket(AF_INET, SOCK_STREAM, 0);
						if(dataSocketfd < 0){
							fprintf(stderr, "Error with socket: %s\n", strerror(errno));
							exit(errno);
						}
						int portNum = atoi(temp);	//convert input to an int
						if(DEBUG) printf("Converted port number: %d\n", portNum);
						int err = connectSock(&dataSocketfd, portNum, argv[1]); //connect data socket to given port
						if(err == -1){
							printf("connectSock had an error\n");
							exit(-1);
						}
						if(DEBUG) printf("Attempting to put file from '%s'...\n", token);
						int writeSize = 2 + strlen(token);
						char toWrite[writeSize + 2];
						strcpy(toWrite, "P");
						strcat(toWrite, token);
						strcat(toWrite, "\n");
						if(DEBUG) printf("Sending filepath '%s' to server\n", toWrite);
						write(comSocketfd, toWrite, writeSize);	//write to server 'P<pathname>\n', telling it to start reading info
						char socketBuf2[BUF_SIZE] = {0};
						read(comSocketfd, socketBuf2, BUF_SIZE);
						if(socketBuf2[0] == 'E'){
							printf("Server returned an error: %s", socketBuf2);
							continue;
						}
						int putFileD = open(token, O_RDONLY);	//open given file in READ ONLY mode
						char buf[1];
						while(read(putFileD, buf, 1) > 0){	//write one character at a time
							write(dataSocketfd, buf, 1);
						}
						close(putFileD);	//close file
						if(close(dataSocketfd) < 0){
							fprintf(stderr, "Error closing a data socket: %s\n", strerror(errno));
							exit(errno);
						}
						if(DEBUG) printf("'put' finished\n");
					}
				}
			}
			else{	//strtok had an error
				printf("Error: missing directory parameter.\n");
			}
		}
		else{	//invalid command and/or improper input
			printf("Invalid command received, try reformatting or looking through available commands.\n");
		}
	}
	if(close(comSocketfd) < 0){	//close command/control socket
		fprintf(stderr, "Error closing connectfd: %s\n", strerror(errno));
		return errno;
	}
	return 0;
}

int connectSock(int *sock, int portNum, char *hostName){	//connects to a given port number, returns either 0 success or -1 for error
	printf("%d\t%s\n", portNum, hostName);
	struct sockaddr_in servAddr;
	struct hostent* hostEntry;
	struct in_addr **pptr;
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(portNum);
	hostEntry = gethostbyname(hostName);	//from first given argument
	//magic ahead given by slides
	pptr = (struct in_addr **) hostEntry->h_addr_list;
	memcpy(&servAddr.sin_addr, *pptr, sizeof(struct in_addr));
	int err = connect(*sock, (struct sockaddr *) &servAddr, sizeof(servAddr));	//equivalent to open()
	if(err < 0){
		fprintf(stderr, "Error connecting to socket: %s\n", strerror(errno));
		return (-1);
	}
	return 0;
}
