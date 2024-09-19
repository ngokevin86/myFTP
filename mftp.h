/*Kevin Ngo - CS 360
 *Assignment Final - Server/Client File Explorer (.h file)
 *Professor Ben McCamish
 *Due April 28th, 2019
 */

#ifndef MFTP_H
#define MFTP_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MY_PORT_NUMBER 49999
#define BUF_SIZE 1024
#define DEBUG 1	//set to 1 to show debug info

int connectSock(int *sock, int portNum, char *hostName);

#endif