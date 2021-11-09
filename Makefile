all:
	$(CC) -Wall chatserver.c -O2 -std=c11 -lpthread -o chatserver
	$(CC) client.c -o client


