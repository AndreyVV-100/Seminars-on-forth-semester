#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#define DEFAULT_PORT htons (27312)
#define BUFFER_SIZE 1024

const char str_check[] = "This is broadcast test.";
