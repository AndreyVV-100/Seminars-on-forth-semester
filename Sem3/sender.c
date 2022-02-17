#include "config.h"

#define DEFAULT_IP htonl (INADDR_BROADCAST)

int StartSender();

int SendBroadcast (int sockfd);
int ReceiveAnswer (int sockfd);
	
// Broadcast checker
int main()
{
    int sockfd = 0;

    if ((sockfd = StartSender()) == -1)
	    return 0;
	    
    SendBroadcast (sockfd);
	ReceiveAnswer (sockfd);

    close (sockfd);
    return 0;
}

int StartSender()
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

    int option_on = 1;
    try (setsockopt (sockfd, SOL_SOCKET, SO_BROADCAST, &option_on, sizeof (option_on)) == -1, "Setsockopt error")

    #undef try
    return sockfd;
}

int SendBroadcast (int sockfd)
{
	struct sockaddr_in addr = {AF_INET, DEFAULT_PORT, {}, {}};
	inet_aton ("192.168.0.255", &(addr.sin_addr));
	
    sendto (sockfd, str_check, sizeof (str_check), 0, (struct sockaddr*) &addr, sizeof (addr));
    return 0;
}

int ReceiveAnswer (int sockfd)
{
    struct sockaddr_in addr = {};
    socklen_t addrlen = sizeof (addr);
    char buf[BUFFER_SIZE] = "";

    recvfrom (sockfd, buf, BUFFER_SIZE, 0, (struct sockaddr*) &addr, &addrlen);
    char* ip = inet_ntoa (addr.sin_addr);
    int port = ntohs (addr.sin_port);

    printf ("Answer reseived from [%s:%d]. Messages are %s\n", ip, port, strcmp (buf, str_check) ? "not equal." : "equal.");
    return 0;
}

