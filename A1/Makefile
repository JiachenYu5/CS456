# define complier
CC=gcc

# targers
TARGET=client server
CFLAGS= -g -Wall -Wextra
normal: $(TARGET)
client: client.c
	$(CC) $(CFLAGS) client.c -o client
server: server.c
	$(CC) $(CFLAGS) server.c -o server
clean: 
	$(RM) $(TARGET)
