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
#include <ctype.h>

#define MAX_PASSWORD_LENGTH 21

int queryMode = 0;
char password [MAX_PASSWORD_LENGTH] = "";
int conn;
char stream_port [7];
char dgram_port [7];
char host_name[21];

#define HEIGHT 4
#define WIDTH 7

void send_dgram() {

	int socket_fd, cc, ecode;
	struct sockaddr_in *dest;
	struct addrinfo hints, *addrlist;  
    
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_NUMERICSERV; hints.ai_protocol = 0;
	hints.ai_canonname = NULL; hints.ai_addr = NULL;
	hints.ai_next = NULL;

	ecode = getaddrinfo(host_name, dgram_port, &hints, &addrlist);
	if (ecode != 0) {
		fprintf(stderr, "nim:send_dgram:getaddrinfo: %s\n", gai_strerror(ecode));
		exit(1);
	}

	dest = (struct sockaddr_in *) addrlist->ai_addr; 



	socket_fd = socket (addrlist->ai_family, addrlist->ai_socktype, 0);
	if (socket_fd == -1) {
		perror ("nim:send_dgram:socket");
		exit (1);
	}


	cc = sendto(socket_fd,&password,sizeof(password),0,(struct sockaddr *) dest,
		          sizeof(struct sockaddr_in));
	if (cc < 0)  {
		perror("nim:send_dgram:sendto");
		exit(1);
	}

	char msg [162];

	socklen_t fsize;
	struct sockaddr_in from;

	fsize = sizeof(from);

	fd_set mask;
	int hits;

	FD_ZERO(&mask);
	FD_SET(socket_fd,&mask);

	struct timeval t;
	t.tv_sec = 60;

	if ((hits = select(socket_fd+1, &mask, NULL, NULL, &t)) < 0) {
		perror("nim:send_dgram:select");
		exit(1);
	}

	if (hits == 0) {
		fprintf(stderr, "Error: did not receive reply from nim_server\n");
		exit(4);
	}
	
	cc = recvfrom(socket_fd, &msg, sizeof(msg), 0, (struct sockaddr *)&from, &fsize);
	if (cc < 0) 
		perror("nim:send_dgram:recvfrom");

	printf("%s\n",msg);
	fflush(stdout);

	if (strcmp(msg, "\nYou have entered an incorrect password.\n") == 0)
		exit(6);

}

int send_msg (char* msg)  {

	int left, num, put;
	left = strlen(msg);

	put = 0;

	while (left > 0) {

		if ((num = write(conn, msg+put, left)) < 0) {
			perror("nim:send_msg:write");
			exit(1);
		}
		else
			left -= num;

		put += num;
	}
	
	return 0;

}

void recv_msg (char* msg) {

	char ch;
	int i;

	for (i = 0; i < 29; i++)  {
		if (!read(conn, &ch, 1))  {

			fprintf(stderr, "Error: nim_server has terminated.\n");
			exit(1);

		}
		msg[i] = ch;
	}
}

void init_conn() {

	int ecode;
	struct sockaddr_in *server;
	struct addrinfo hints, *addrlist;  
		    
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; 
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICSERV; 
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_canonname = NULL; 
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	ecode = getaddrinfo(host_name, stream_port, &hints, &addrlist);

	if (ecode != 0) {
		fprintf(stderr, "nim:init_conn:getaddrinfo: %s\n", gai_strerror(ecode));
		exit(1);
	}

	server = (struct sockaddr_in *) addrlist->ai_addr;


	conn = socket (addrlist->ai_family, addrlist->ai_socktype, 0);
	if (conn < 0) {
		perror ("nim:init_conn:socket");
		exit(1);
	}

	if (connect(conn, (struct sockaddr *)server, sizeof(struct sockaddr_in)) < 0) {
		perror("nim:init_conn:connect");
		exit(1);
	}

	send_msg(password);
	send_msg("\n");

}

void printBoard (char* msg) {

		int i;
		for (i = 1; i <= HEIGHT * WIDTH; i++)  {

			if (i % WIDTH == 1)
				printf("%d|", (i / WIDTH) + 1);
			if (msg[i] == 'X')
				printf(" 0");
			else if (msg[i] == '0')
				printf("  ");
			else
				printf("Error\n");

			if (i % WIDTH == 0)
				printf("\n");

		}

		printf(" +------------------------\n");
		printf("   1 2 3 4 5 6 7\n");

}

void endOfGame (char* msg)  {

	char c = msg[0];
	printBoard(msg);

	if (c == 'C')  {
		printf("Game over.  You win!\n");
		exit(0);
	}

	if (c == 'D')  {
		printf("Game over.  You lose!\n");
		exit(0);
	}

	if (c == 'E')  {
		printf("Your opponent has resigned.  You win!\n");
		exit(0);
	}

	if (c == 'F')  {
		printf("You have resigned.  You lose!\n");
		exit(0);
	}

	if (c == 'G') {
		fprintf(stderr,"Your opponent has disconnected.\n");
		exit(7);
	}

}

int main (int argc, char * argv [])  {

	if (argc == 1);	
	else if (argc == 2 && strcmp(argv[1], "-q") == 0)
		queryMode = 1;
	else if (argc == 4 && strcmp(argv[1], "-q") == 0 && strcmp(argv[2], "-p") == 0)  {
		strcpy(password, argv[3]);
		queryMode = 1;
	}
	else if (argc == 4 && strcmp(argv[1], "-p") == 0 && strcmp(argv[3], "-q") == 0)  {
		strcpy(password, argv[2]);	
		queryMode = 1;
	}
	else if (argc == 3 && strcmp(argv[1], "-p") == 0)
		strcpy(password, argv[2]);
	else  {
		fprintf(stderr, "Argument syntax is invalid.\n");
		exit(1);
	}

	FILE* f1;
	if (!(f1 = fopen("server_info.txt", "r"))) {
		sleep(60);
		if (!(f1 = fopen("server_info.txt", "r")))  {
			fprintf(stderr, "server_info.txt could not be opened!\n");
			exit(2);
		}
	}

	fgets(host_name, 21, f1);
	fgets(dgram_port, 7, f1);
	fgets(stream_port, 7, f1);
	host_name[strlen(host_name) - 1] = '\0';
	dgram_port[strlen(dgram_port) - 1] = '\0';
	stream_port[strlen(stream_port) - 1] = '\0';

	if (queryMode)
		send_dgram();

	else  {

		init_conn();

		char correct_pw;
		read(conn, &correct_pw, 1);

		if (correct_pw == '0') {
			fprintf(stderr,"\nError: You have entered an incorrect password.\n\n");
			exit(8);
		}

		printf("Your handle: ");
		char handle [21];
		fgets(handle, 22, stdin);
		send_msg(handle);

		char oppHandle [21];
		
		char c;
		int j = 0;

		while (1)  {

			if (!read(conn, &c, 1))  {
				fprintf(stderr, "Error: nim_server terminated.\n");
				exit(2);
			}
			if (c == '\n') {
				oppHandle[j] = '\0';
				break;
			}
			oppHandle[j] = c;
			j++;
		}

		printf("Opponent's handle: %s\n", oppHandle);

		while (1)  {


			fd_set mask;
			int hits;

			FD_ZERO(&mask);
			FD_SET(0,&mask);
			FD_SET(conn,&mask);

			if ((hits = select(conn+1, &mask, NULL, NULL, NULL)) < 0) {
				perror("nim:init_conn:select");
				exit(1);
			}

			if ((hits > 0) && (FD_ISSET(0,&mask))) {
				fprintf(stderr, "Error: do not input text until it is your turn.\n");
				exit(1);
			}

			char msg [29];
			recv_msg(msg);

			if (msg[0] != 'A')
				endOfGame(msg);

			printf("Board:\n");			
			printBoard(msg);

			printf("\nYour move:\n");

			int validMove = 0;
			int row = 0;
			int column = 0;

			fd_set mask1;
			int hits1;

			FD_ZERO(&mask1);
			FD_SET(0,&mask1);
			FD_SET(conn,&mask1);

			if ((hits1 = select(conn+1, &mask1, NULL, NULL, NULL)) < 0) {
				perror("nim:init_conn:select");
				exit(1);
			}

			if ((hits1 > 0) && (FD_ISSET(conn,&mask1))) {
				fprintf(stderr, "Error: the match has been terminated.\n");
				exit(1);
			}


			while (!validMove)  {

				char s [9];
				memset(&s, 0, 9);
				fgets(s, 10, stdin);

				if (isdigit(s[0]) && isspace(s[1]) && isdigit(s[2]) && strlen(s) == 4)  {

					row = s[0] - '0';
					column = s[2] - '0';

					if (row <= HEIGHT && column <= WIDTH && row > 0 && column > 0)
						validMove = 1;

					else if (row == 0 && column == 0)  {
						msg[0] = 'B';
						send_msg(msg);
						break;
					}

					int anyStonesRemoved = 0;
					int j;

					for (j = ((row-1) * WIDTH) + column; j <= row * WIDTH; j++)  {

						if (msg[j] == 'X')
							anyStonesRemoved = 1;

						msg[j] = '0';

					}

					validMove = validMove && anyStonesRemoved;

				}

				if (!validMove)
					printf("Invalid move.  Please enter a valid move:\n");
			}

			int err = send_msg(msg);
			if (err)  {
				fprintf(stderr, "Error sending message to nim_match_server\n");
				exit(1);
			}

			printf("Awaiting move from opponent...\n");

		}

	}

	return 0;

}
