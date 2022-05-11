#include "my_inet_lib.h"

enum WorkModes
{
    TCP_MODE  = 0,
    UDP_MODE  = 1,
    BROADCAST = 2,
    FILE_COPY = 3,
    UNKNOWN_MODE = 4
};

int ProcessArgs (int argc, char** argv, struct sockaddr_in* addr);
int ProcessTCP (int sockfd, struct sockaddr_in* addr);
enum WorkModes GetMode (const char* str);

int main (int argc, char** argv)
{
    LOG_FD = STDERR_FILENO;

    struct sockaddr_in server_addr = {};
    TRY (ProcessArgs (argc, argv, &server_addr), log_info ("Can't parse arguments.\n"););

    int sockfd = OpenSocket (TCP, DEFAULT_IP, 0);
    if (sockfd == -1)
    {
        log_error ("Can't open socket.\n");
        return 0;
    }

    ProcessTCP (sockfd, &server_addr);

    CloseSocket (sockfd);
    return 0;
}

int ProcessArgs (int argc, char** argv, struct sockaddr_in* addr)
{
    if (argc <= 1)
        return -1;

    enum WorkModes mode = GetMode (argv[1]);

    switch (mode)
    {
        case TCP_MODE:
        case UDP_MODE:
        {   
            if (argc != 3)
            {
                log_error ("No ip addr for connection.\n");
                return -1;
            }

            unsigned int ip[4] = {};
            int ip_ok = sscanf (argv[2], "%u.%u.%u.%u", ip, ip + 1, ip + 2, ip + 3);
            if (ip_ok != 4)
            {
                log_error ("Bad ip address.\n");
                return -1;
            }

            addr->sin_family = AF_INET;
            addr->sin_addr.s_addr = ip[0] + (ip[1] << 8) + (ip[2] << 16) + (ip[3] << 24);
            addr->sin_port = (mode == TCP_MODE) ? DEFAULT_TCP_PORT : DEFAULT_UDP_PORT;
            break;
        }

        case BROADCAST:
            log_error ("Sorry, now is not supported.\n");
            return -1;
            break;

        case FILE_COPY:
            log_error ("Sorry, now is not supported.\n");
            return -1;
            break;

        case UNKNOWN_MODE:
        default:
            log_error ("Bad mode. Use --tcp {ip}, --udp {ip}, --broadcast or --filecopy {from} {to}.");
            return -1;
    }

    /* (void) argc; (void) argv;

    addr->sin_family = AF_INET;
    // inet_aton ("192.168.0.103", &(addr->sin_addr));
    inet_aton ("192.168.0.115", &(addr->sin_addr));
    addr->sin_port = DEFAULT_TCP_PORT;
    return 0; */
    return 0;
}

enum WorkModes GetMode (const char* str)
{
    if (strcmp ("--tcp", str) == 0)
        return TCP_MODE;

    if (strcmp ("--udp", str) == 0)
        return UDP_MODE;

    if (strcmp ("--broadcast", str) == 0)
        return BROADCAST;

    if (strcmp ("--filecopy", str) == 0)
        return FILE_COPY;

    return UNKNOWN_MODE;
}

int ProcessTCP (int sockfd, struct sockaddr_in* addr)
{
    char buf_stdin[BUFFER_SIZE] = "", buf_stdout[BUFFER_SIZE] = "";
    TRY (connect (sockfd, (struct sockaddr*) addr, sizeof (*addr)), perror ("connect"); return -1;)

    int kek = fork();

    while (1)
    {
        ssize_t recv_size = 0;

        if (kek)
        {
            recv_size = recv (sockfd, buf_stdout, BUFFER_SIZE, 0);
            TRY (recv_size,  perror ("recv");)
            write (STDOUT_FILENO, buf_stdout, recv_size);
        } 
        
        else
        {
            if (!fgets (buf_stdin, BUFFER_SIZE - 1, stdin))
            {
                perror ("fgets");
                return -1;
            }

            send (sockfd, buf_stdin, strlen (buf_stdin), 0);

            if (strcmp ("exit\n", buf_stdin) == 0)
                return 0;
        }

        memset (buf_stdin,  0, BUFFER_SIZE);
        memset (buf_stdout, 0, recv_size);
    }

    return 0;
}
