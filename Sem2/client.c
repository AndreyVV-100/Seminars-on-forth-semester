#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

typedef enum
{
    TCP = 0,
    UDP = 1
} Protocol;

#define DEFAULT_PORT htons (8080)
#define DEFAULT_IP htonl (INADDR_LOOPBACK)
#define BUFFER_SIZE 1024

int StartClient (Protocol prt);

void ProcessTCP (int sockfd);
void ProcessUDP (int sockfd);

int main (int argc, char** argv)
{
    int sockfd = 0;

    if (argc == 1 || strcmp (argv[1], "-UDP"))
    {
        if ((sockfd = StartClient (TCP)) == -1)
            return 0;
        ProcessTCP (sockfd); 
    }

    else
    {
        if ((sockfd = StartClient (UDP)) == -1)
            return 0;
        ProcessUDP (sockfd);
    }

    close (sockfd);
    return 0;
}

int StartClient (Protocol prt)
{
    #define try(cond, str)  if (cond)           \
                            {                   \
                                close (sockfd); \
                                perror (str);   \
                                return -1;      \
                            }

    int sockfd = socket (AF_INET, (prt == TCP) ? SOCK_STREAM : SOCK_DGRAM, 0);
    try (sockfd == -1, "Socket error");

    if (prt == TCP)
    {
        struct sockaddr_in addr = {};
        try (bind (sockfd, (struct sockaddr*) &addr, sizeof (struct sockaddr_in)) == -1, "Bind error");

        int reuse_on = 1;
        try (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse_on, sizeof (reuse_on)) == -1, "Setsockopt error")

    }

    #undef try
    return sockfd;
}

void ProcessTCP (int sockfd)
{
    (void) sockfd;
}

void ProcessUDP (int sockfd)
{
    char buf[BUFFER_SIZE] = "";
    struct sockaddr_in addr = {AF_INET, DEFAULT_PORT, {DEFAULT_IP}, {}};

    while (1)
    {
        fgets (buf, BUFFER_SIZE, stdin);
        sendto (sockfd, buf, BUFFER_SIZE, 0, (struct sockaddr*) &addr, sizeof (addr));
    }
}

int ReadMessage (char* buf, size_t bufsize)
{
    fgets (buf, bufsize, stdin);
    return 0;
}
