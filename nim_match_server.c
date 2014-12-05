/************************************************************************\
* 			  INET_RSTREAM.C       			         *
* Test of TCP/IP. Set up a socket for establishing a connection at       *
* some free port on the local host. After accepting a connection, speak  *
* briefly to peer, and then read char at a time from the stream. Write   *
* to stdout. At EOF, shut down.                                          *
* 								         *
* Phil Kearns							         *
* April 11, 1987						         *
* 								         *
* Modified February 2009 to use getaddrinfo()                            *
\************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>


int conn1;
int conn2;
int ns_pid;

void usr2handler() {
	exit(3);
}

int send_msg (char* msg, int conn)  {

	int left, num, put;
	left = strlen(msg);

	put = 0;

	while (left > 0) {

		if ((num = write(conn, msg+put, left)) < 0) {
			perror("nim_match_server:send_msg:write");
			exit(1);
		}
		else 
			left -= num;

		put += num;
	}
	
	return 0;

}

void recv_msg (char* msg, int conn) {

	char ch;
	int i;

	for (i = 0; i < 29; i++)  {
		if (!read(conn, &ch, 1))  {

			msg[0] = 'G';
			if (conn == conn1)
				send_msg(msg, conn2);
			else if (conn == conn2)
				send_msg(msg, conn1);

			exit(1);

		}
		msg[i] = ch;
	}

	if (strcmp(msg, "A0000000000000000000000000000") == 0)  {

		if (conn == conn1)  {
			msg[0] = 'C';	
			send_msg(msg, conn1);
			msg[0] = 'D';
			send_msg(msg, conn2);
			exit(0);
		}
		else if (conn == conn2)  {
			msg[0] = 'D';
			send_msg(msg, conn1);
			msg[0] = 'C';
			send_msg(msg, conn2);
			exit(0);
		}
	}

	if (msg[0] == 'B')  {

		if (conn == conn1)  {	
			msg[0] = 'F';
			send_msg(msg, conn1);
			msg[0] = 'E';
			send_msg(msg, conn2);
			exit(0);
		}
		else if (conn == conn2)  {
			msg[0] = 'E';
			send_msg(msg, conn1);
			msg[0] = 'F';
			send_msg(msg, conn2);
			exit(0);
		}
	}

}

int main(int argc, char* argv[])  {

	if (signal(SIGUSR2, usr2handler) == SIG_ERR)  {
		fprintf(stderr, "nim_match_server:main:signal\n");
		exit(1);
	}

	if (argc != 4) {
		fprintf(stderr, "Incorrect number of arguments to nim_match_server!\n");
		exit(1);
	}

	conn1 = atoi(argv[1]);
	conn2 = atoi(argv[2]);
	ns_pid = atoi(argv[3]);


	char msg [29] = "AX000000XXX0000XXXXX00XXXXXXX";

	while (1)  {

		send_msg(msg, conn1);
		int flag1 = 1;

		while (flag1)  {

			fd_set mask1;
			int hits1, highestFD1;

			FD_ZERO(&mask1);
			FD_SET(conn1,&mask1);
			FD_SET(conn2,&mask1);

			if (conn1 > conn2)
				highestFD1 = conn1;
			else
				highestFD1 = conn2;

			if ((hits1 = select(highestFD1+1, &mask1, NULL, NULL, NULL)) < 0) {
				perror("nim_match_server:main:select");
				exit(1);
			}

			if ((hits1 > 0) && (FD_ISSET(conn2,&mask1))) {

				char ch;
				if (!read(conn2, &ch, 1))  {
					send_msg("G", conn1);
					exit(1);
				}

			}
			else
				flag1 = 0;

		}

		recv_msg(msg, conn1);
		send_msg(msg, conn2);

		int flag2 = 1;

		while (flag2)  {

			fd_set mask2;
			int hits2;
			int highestFD2;

			FD_ZERO(&mask2);
			FD_SET(conn1,&mask2);
			FD_SET(conn2,&mask2);

			if (conn1 > conn2)
				highestFD2 = conn1;
			else
				highestFD2 = conn2;

			if ((hits2 = select(highestFD2+1, &mask2, NULL, NULL, NULL)) < 0) {
				perror("nim_match_server:main:select");
				exit(1);
			}

			if ((hits2 > 0) && (FD_ISSET(conn1,&mask2))) {

				char ch;
				if (!read(conn1, &ch, 1))  {
					send_msg("G", conn2);
					exit(1);
				}

			}
			else
				flag2 = 0;

		}

		recv_msg(msg, conn2);	
	}
	
}
