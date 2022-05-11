CC := gcc
CFLAGS := -Wall -Wextra
LOG_LEVEL := -DLOG_WRITE_TIME -DLOG_WHITE_POS -DLOG_WRITE_TYPE # Now full
INCLUDE_PATH := include/
LIBS := sources/my_inet_lib.c

all: server client

server: sources/server.c $(LIBS)
	$(CC) -I $(INCLUDE_PATH) $(CFLAGS) $(LOG_LEVEL) $^ -o server.out

client: sources/client.c $(LIBS)
	$(CC) -I $(INCLUDE_PATH) $(CFLAGS) $^ -o client.out
