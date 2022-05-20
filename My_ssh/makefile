CC := gcc
CFLAGS := -Wall -Wextra
LOG_LEVEL := -DLOG_WRITE_TIME -DLOG_WHITE_POS -DLOG_WRITE_TYPE # Now full
INCLUDE_PATH := include/

# LIBS := sources/my_inet_lib.c
LIBS := my_inet_lib
LIBS_CRC := sources/my_inet_lib.c
SLIB := $(realpath .)/my_inet_lib.a
DLIB := -L$(realpath .) -l$(LIBS)

# LIBS := my_inet_lib

.PHONY: all server client library

all: server client

ifeq ($(LIBTYPE), DYNAMIC)
	SLIB := $(DLIB)
endif

server: library sources/server.c
	$(CC) -I $(INCLUDE_PATH) $(CFLAGS) $(LOG_LEVEL) sources/server.c -o server.out $(SLIB)

client: library sources/client.c
	$(CC) -I $(INCLUDE_PATH) $(CFLAGS) sources/client.c -o client.out $(SLIB)

library: $(LIBS_CRC)
	$(CC) -fPIC -I$(INCLUDE_PATH) $(CFLAGS) $^ -c -o my_inet_lib.o
	$(CC) -shared $(LIBS).o -o lib$(LIBS).so
	ar rcs my_inet_lib.a my_inet_lib.o

clean:
	rm -f *.a
	rm -f *.so
	rm -f *.o
	rm -f *.out
