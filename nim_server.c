/*  Some sections of code adopted from examples shown in class by Professor Phil Kearns.  */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/time.h>

#define MAX_PASSWORD_LENGTH 21

char hostname [20];
char password [MAX_PASSWORD_LENGTH] = "";
int passwordMode;
int stream_listener;
int dgram_fd;
int numActiveGames;
socklen_t length;
struct sockaddr_in peer;
struct sockaddr_in *s_in;
char waitingPlayer [21] = "";

struct nim_game {

	char handle1 [21];
	char handle2 [21];
	int nms_pid;

};

struct nim_game activeGames [10];

void removeFromActiveGames(int pid)  {

	int i;

	for (i = 0; i < numActiveGames; i++) {

		if (activeGames[i].nms_pid == pid)  {
			
			int j;

			for (j = i; j < numActiveGames; j++)
				activeGames[j] = activeGames[j+1];

			memset(activeGames + j, 0, sizeof(struct nim_game));
			numActiveGames--;
			
		}

	}

}

void sigusr2_handler() {

	int i;

	for (i = 0; i < numActiveGames; i++)  {
		if (kill(activeGames[i].nms_pid, SIGUSR2) < 0)  {
			perror("nim_server:sigusr2_handler:kill");
			exit(1);
		}
	}

	remove("server_info.txt");
	fprintf(stderr, "Received SIGUSR2\n");
	exit(3);

}

void sigint_handler() {

	int i;

	for (i = 0; i < numActiveGames; i++)  {
		if (kill(activeGames[i].nms_pid, SIGUSR2) < 0)  {
			perror("nim_server:sigint_handler:kill");
			exit(1);
		}
	}

	remove("server_info.txt");
	exit(3);

}

void sigchld_handler()  {

	int i;
	for (i = 0; i < 10; i++)  {

		int pid = waitpid(-1, NULL, WNOHANG);
		if (pid > 0)
			removeFromActiveGames(pid);

	}

}

int send_msg (char* msg, int conn)  {

	int left, num, put;
	left = strlen(msg);
	put = 0;

	while (left > 0) {

		if ((num = write(conn, msg+put, left)) < 0) {
			perror("nim_server:send_msg:write");
			exit(1);
		}
		else 
			left -= num;

		put += num;
	}

	write(conn, "\n", 1);
	return 0;

}

void recv_msg (char* msg, int conn) {

	char ch;
	int i = 0;
	
	while (read(conn, &ch, 1) && i < 21)  {

		if (ch == '\n')  {
			msg[i] = '\0';
			break;
		}
		msg[i] = ch;
		i++;
	}

}

void init_dgram() {

	int ecode;
	struct addrinfo hints, *addrlist; 

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_NUMERICSERV | AI_PASSIVE; hints.ai_protocol = 0;
	hints.ai_canonname = NULL; hints.ai_addr = NULL;
	hints.ai_next = NULL;

	if (gethostname(hostname, 10) < 0)  {
		perror("nim_server:init_dgram:gethostname:");
		exit(1);
	}

	strncat(hostname, ".cs.wm.edu", 10);

	ecode = getaddrinfo(hostname, "0", &hints, &addrlist);
	if (ecode != 0) {
		fprintf(stderr, "nim_server:init_dgram:getaddrinfo: %s\n", gai_strerror(ecode));
		exit(1);
	}

	s_in = (struct sockaddr_in *) addrlist->ai_addr;

	dgram_fd = socket (addrlist->ai_family, addrlist->ai_socktype, 0);
	if (dgram_fd < 0) {
		perror ("nim_server:init_dgram:socket");
		exit (1);
	}

	if (bind(dgram_fd, (struct sockaddr *)s_in, sizeof(struct sockaddr_in)) < 0) {
		perror("nim_server:init_dgram:bind");
		exit(1);
	}

	int length = sizeof(struct sockaddr_in);
	if (getsockname(dgram_fd, (struct sockaddr *)s_in, &length) < 0) {
		perror("nim_server:init_dgram:getsockname");
		exit(1);
	}

	FILE* f1 = fopen("server_info.txt", "w");
	fprintf(f1, "%s\n", hostname);
	fprintf(f1, "%d\n", ntohs(s_in->sin_port));
	fclose(f1);

}

void init_stream () {

	struct sockaddr_in *localaddr;
	int ecode;
	struct addrinfo hints, *addrlist;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICSERV; 
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_canonname = NULL; 
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	ecode = getaddrinfo(hostname, "0", &hints, &addrlist);

	if (ecode != 0) {
		fprintf(stderr, "nim_server:init_stream:getaddrinfo: %s\n", gai_strerror(ecode));
		exit(1);
	}

	localaddr = (struct sockaddr_in *) addrlist->ai_addr;

	if ( (stream_listener = socket( addrlist->ai_family, addrlist->ai_socktype, 0 )) < 0 ) {
		perror("nim_server:init_stream:socket");
		exit(1);
	}


	if (bind(stream_listener, (struct sockaddr *)localaddr, sizeof(struct sockaddr_in)) < 0) {
		perror("nim_server:init_stream:bind");
		exit(1);
	}

	length = sizeof(struct sockaddr_in);
	if (getsockname(stream_listener, (struct sockaddr *)localaddr, &length) < 0) {
		perror("nim_server:init_stream:getsockname");
		exit(1);
	}

	FILE* f1 = fopen("server_info.txt", "a");
	fprintf(f1, "%d\n", ntohs(localaddr->sin_port));
	fclose(f1);

	if (listen(stream_listener,20) < 0)  {
		perror("nim_server:init_stream:listen");
		exit(1);
	}

}

void accept_dgram () {

	char query_msg [MAX_PASSWORD_LENGTH];
	socklen_t fsize;
	int cc;
	struct sockaddr_in from;

	fsize = sizeof(from);
	cc = recvfrom(dgram_fd, &query_msg, sizeof(query_msg), 0, (struct sockaddr *)&from, &fsize);

	if (cc < 0) 
		perror("nim_server:accept_dgram:recvfrom");

	fflush(stdout);

	int fd = socket (AF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
		perror ("nim_server:accept_dgram:socket");
		exit(1);
	}

	char msg [163] = "";

	char a [53] = "";
	if (strcmp(waitingPlayer, "") != 0)
		sprintf(a, "\nUser %s is currently waiting to play.\n", waitingPlayer);
	strcat(msg, a);

	char b [31];
	sprintf(b, "\nThere are %d active games.\n", numActiveGames);
	strcat(msg, b);

	int i;
	for (i = 0; i < numActiveGames; i++) {

		char c [11];
		sprintf(c, "\nGame %d:\n", i+1);
		strcat(msg, c);

		char e [32];
		char f [33];
		sprintf(e, "Player 1: %s\n", activeGames[i].handle1);
		sprintf(f, "Player 2: %s\n", activeGames[i].handle2);
		strcat(msg, e);
		strcat(msg, f);

	}

	if (passwordMode && strcmp(query_msg, password) != 0)
		strcpy(msg, "\nYou have entered an incorrect password.\n");

	cc = sendto(fd, &msg, sizeof(msg), 0, (struct sockaddr *)&from, sizeof(struct sockaddr_in));

	if (cc < 0) {
		perror("nim_server:accept_dgram:sendto");
		exit(1);
	}

}

int accept_stream () {

	int conn;

	length = sizeof(peer);
	if ((conn=accept(stream_listener, (struct sockaddr *)&peer, &length)) < 0) {
		perror("nim_server:accept_stream:accept");
		exit(1);
	}

	char client_pw [MAX_PASSWORD_LENGTH];
	int j;

	for (j = 0; j < MAX_PASSWORD_LENGTH; j++)  {

		char ch;

		if (!read(conn, &ch, 1))  {
			return 0;
		}

		if (ch == '\n')
			break;

		client_pw[j] = ch;

	}

	client_pw[j] = '\0';
	
	if (passwordMode && strcmp(client_pw, password) != 0)  {
		write(conn, "0", 1);
		return 0;
	}
	else
		write(conn, "1", 1);

	return conn;

}

int main(int argc, char* argv[])
{

	char handle1 [21];
	char handle2 [21];

	int conn1;
	int conn2;
	int connections = 0;

	numActiveGames = 0;

	if (signal(SIGUSR2, sigusr2_handler) == SIG_ERR)  {
		fprintf(stderr, "nim_server:main:signal\n");
		exit(1);
	}

	if (signal(SIGINT, sigint_handler) == SIG_ERR)  {
		fprintf(stderr, "nim_server:main:signal\n");
		exit(1);
	}

	if (signal(SIGCHLD, sigchld_handler) == SIG_ERR)  {
		fprintf(stderr, "nim_server:main:signal\n");
		exit(1);
	}

	passwordMode = 0;

	if (argc > 2)  {
		fprintf(stderr, "Error: too many arguments.\n");
		exit(5);
	}
	else if (argc == 2) {
		passwordMode = 1;
		strcpy(password, argv[1]);
	}

	init_dgram();
	init_stream();

	while (numActiveGames < 10) {

		fd_set mask;
		sigset_t sigmask;
		int hits;

		FD_ZERO(&mask);
		FD_SET(dgram_fd,&mask);
		FD_SET(stream_listener,&mask);

		sigemptyset(&sigmask);
		sigaddset(&sigmask, SIGCHLD);

		int highestFD;

		if (stream_listener > dgram_fd)
			highestFD = stream_listener;
		else
			highestFD = dgram_fd;

		struct timespec t;
		t.tv_sec = 1;

		if ((hits = pselect(highestFD+1, &mask, NULL, NULL, &t, &sigmask)) < 0) {
			perror("nim_server:main:select");
			exit(1);
		}

		if ((hits > 0) && (FD_ISSET(dgram_fd,&mask)))
				accept_dgram();

		else if ((hits > 0) && (FD_ISSET(stream_listener, &mask))) {

			if (connections == 0)  {
					
				if ((conn1 = accept_stream()) != 0)  {

					connections++;
					memset(&handle1, 0, sizeof(handle1));
					recv_msg(handle1, conn1);
					strcpy(waitingPlayer, handle1);

				}

			}

			else if (connections == 1) {

				if ((conn2 = accept_stream()) != 0)  {

					strcpy(waitingPlayer, "");
					memset(&handle2, 0, sizeof(handle2));
					recv_msg(handle2, conn2);
					send_msg(handle2, conn1);
					send_msg(handle1, conn2);

					int nms_pid;
					int ns_pid = getpid();

					if ((nms_pid = fork()) == 0)  {
	
						char s4[20] = "./nim_match_server";
						char s1 [20];
						sprintf(s1, "%d", conn1);
						char s2 [20];
						sprintf(s2, "%d", conn2);
						char s3 [20];
						sprintf(s3, "%d", ns_pid);
						execl("./nim_match_server", s4, s1, s2, s3, NULL);
						printf("Error\n");
					}

					else  {

						struct nim_game game;
	
						close(conn1);
						close(conn2);

						strcpy(game.handle1, handle1);
						strcpy(game.handle2, handle2);
						game.nms_pid = nms_pid;

						activeGames[numActiveGames] = game;
						numActiveGames++;
						memset(&handle1, 0, sizeof(handle1));
						memset(&handle2, 0, sizeof(handle2));

						connections = 0;
	
					}  //end else

				}  //end if a_c_s() == 0
	
			}   //end else if connections == 1

		}   //end else if stream

	}   //end while loop

	printf("Cannot accept any more games\n");

	return 0;
	
}
