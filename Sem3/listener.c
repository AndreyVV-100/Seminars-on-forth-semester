#include "config.h"

#define DEFAULT_IP htonl (INADDR_ANY)

int StartListener();
int ListenerWork (int sockfd);

// mirror for checker
int main()
{
    int sockfd = 0;

    if ((sockfd = StartListener()) == -1)
	    return 0;
	    
    ListenerWork (sockfd);

    close (sockfd);
    return 0;
}

int StartListener()
{
    #define try(cond, str)  if (cond)           \
                            {                   \
                                close (sockfd); \
                                perror (str);   \
                                return -1;      \
                            }

    int sockfd = socket (AF_INET, SOCK_DGRAM, 0);
    try (sockfd == -1, "Socket error");

    struct sockaddr_in addr = {AF_INET, DEFAULT_PORT, {DEFAULT_IP}, {}};
    try (bind (sockfd, (struct sockaddr*) &addr, sizeof (struct sockaddr_in)) == -1, "Bind error");

    #undef try
    return sockfd;
}

int ListenerWork (int sockfd)
{
    struct sockaddr_in addr = {};
    socklen_t addrlen = sizeof (addr);
    char buf[BUFFER_SIZE] = "";

    recvfrom (sockfd, buf, BUFFER_SIZE - 1, 0, (struct sockaddr*) &addr, &addrlen);
    char* ip = inet_ntoa (addr.sin_addr);
    int port = ntohs (addr.sin_port);

    printf ("[%s:%d]: %s\n", ip, port, buf);
    sendto (sockfd, buf, strlen (buf) + 1, 0, (struct sockaddr*) &addr, sizeof (addr));
    
    return 0;
}

