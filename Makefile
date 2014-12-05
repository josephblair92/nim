all:
	gcc -o nim nim.c
	gcc -o nim_match_server nim_match_server.c
	gcc -o nim_server nim_server.c
