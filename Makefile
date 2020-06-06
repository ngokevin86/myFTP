#compiles server executable as "server" and client executable as "client"
all: mftpserve.c mftp.c
	gcc -o server mftpserve.c
	gcc -o client mftp.c

#
run: server
	./server

#clean server + client executables, as well as any .o files potentially created
clean:
	rm -f client server *.o