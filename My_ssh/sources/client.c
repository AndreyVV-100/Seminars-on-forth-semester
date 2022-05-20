#include "my_inet_lib.h"

enum WorkModes
{
    TCP_MODE  = 0,
    UDP_MODE  = 1,
    BROADCAST = 2,
    FILE_COPY = 3,
    UNKNOWN_MODE = -1
};

enum WorkModes ProcessArgs (int argc, char** argv, struct sockaddr_in* addr);
enum WorkModes GetMode (const char* str);

int ProcessTCP (int sockfd, struct sockaddr_in* addr);
int ProcessUDP (int sockfd, struct sockaddr_in* addr);
int ProcessBroadcast (int sockfd);
int ProcessFilecopy (int sockfd, struct sockaddr_in* addr, const char* file_from, const char* ip_and_file_to);

int main (int argc, char** argv)
{
    LOG_FD = STDERR_FILENO;
    enum WorkModes mode = UNKNOWN_MODE;
    int sockfd = -1;

    struct sockaddr_in server_addr = {};
    TRY (mode = ProcessArgs (argc, argv, &server_addr), log_info ("Can't parse arguments.\n"););

    switch (mode)
    {
        case TCP_MODE:
            sockfd = OpenSocket (TCP, DEFAULT_IP, 0);
            if (sockfd == -1)
            {
                log_error ("Can't open socket.\n");
                return 0;
            }

            ProcessTCP (sockfd, &server_addr);
            break;

        case BROADCAST:
            sockfd = OpenSocket (UDP, DEFAULT_IP, 0);
            if (sockfd == -1)
            {
                log_error ("Can't open socket.\n");
                return 0;
            }

            ProcessBroadcast (sockfd);
            break;

        case UDP_MODE:
            sockfd = OpenSocket (UDP, DEFAULT_IP, 0);
            if (sockfd == -1)
            {
                log_error ("Can't open socket.\n");
                return 0;
            }

            ProcessUDP (sockfd, &server_addr);
            break;

        case FILE_COPY:
            sockfd = OpenSocket (UDP, DEFAULT_IP, 0);
            if (sockfd == -1)
            {
                log_error ("Can't open socket.\n");
                return 0;
            }

            ProcessFilecopy (sockfd, &server_addr, argv[2], argv[3]);
            break;

        default:
            log_info ("Now isn't supported.\n");
            break;
    }

    CloseSocket (sockfd);
    return 0;
}

enum WorkModes ProcessArgs (int argc, char** argv, struct sockaddr_in* addr)
{
    if (argc <= 1)
        return UNKNOWN_MODE;

    enum WorkModes mode = GetMode (argv[1]);

    switch (mode)
    {
        case TCP_MODE:
        case UDP_MODE:
        {   
            if (argc != 3)
            {
                log_error ("No ip addr for connection.\n");
                return UNKNOWN_MODE;
            }

            int ip_ok = inet_aton (argv[2], &(addr->sin_addr));// sscanf (argv[2], "%u.%u.%u.%u", ip, ip + 1, ip + 2, ip + 3);
            if (ip_ok != 1)
            {
                log_error ("Bad ip address.\n");
                return UNKNOWN_MODE;
            }

            addr->sin_family = AF_INET;
            addr->sin_port = (mode == TCP_MODE) ? DEFAULT_TCP_PORT : DEFAULT_UDP_PORT;
            break;
        }

        case BROADCAST:
            break;

        case FILE_COPY:
        {
            char* colon = NULL;

            if (argc != 4 || !(colon = strchr (argv[3], ':')))
            {
                log_error ("Bad input parameters. Write:\n" 
                           "./client.out --filecopy src ip:dest\n");
                printf ("%s", colon);
                return UNKNOWN_MODE;
            }

            *colon = '\0';
            int ip_ok = inet_aton (argv[3], &(addr->sin_addr));
            *colon = ':';

            if (ip_ok != 1)
            {
                log_error ("Bad ip address.\n");
                return UNKNOWN_MODE;
            }

            addr->sin_family = AF_INET;
            addr->sin_port = DEFAULT_UDP_PORT;
            break;
        }

        case UNKNOWN_MODE:
        default:
            log_error ("Bad mode. Use --tcp {ip}, --udp {ip}, --broadcast or --filecopy {from} {to}.");
            return UNKNOWN_MODE;
    }

    return mode;
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

    while (1)
    {
        ssize_t recv_size = 0, send_size = 0;

        if (CheckReadyForRead (sockfd, 1))
        {
            recv_size = recv (sockfd, buf_stdout, BUFFER_SIZE, 0);
            TRY (recv_size,  perror ("recv");)
            write (STDOUT_FILENO, buf_stdout, recv_size);
        }

        if (CheckReadyForRead (STDIN_FILENO, 1))
        {
            send_size = read (STDIN_FILENO, buf_stdin, BUFFER_SIZE - 1);
            if (send_size == -1)
            {
                perror ("fgets");
                return -1;
            }

            if (send_size > 0)
                send (sockfd, buf_stdin, send_size, 0);

            if (strcmp ("exit\n", buf_stdin) == 0)
                return 0;
        }

        memset (buf_stdin,  0, BUFFER_SIZE);
        memset (buf_stdout, 0, recv_size);
    }

    return 0;
}

int ProcessUDP (int sockfd, struct sockaddr_in* addr)
{
    char buf_stdin[BUFFER_SIZE] = "", buf_stdout[BUFFER_SIZE] = "";
    socklen_t addrlen = sizeof (*addr);
    const size_t offset = sizeof (((MessageStruct*) buf_stdin)->type);

    ((MessageStruct*) buf_stdin)->type = UDP_START_CONNECTION;
    sendto (sockfd, buf_stdin, offset, 0, (struct sockaddr*) addr, addrlen);

    while (1)
    {
        ssize_t recv_size = 0, send_size = 0;

        if (CheckReadyForRead (sockfd, 1))
        {
            recv_size = recvfrom (sockfd, buf_stdout, BUFFER_SIZE, 0, (struct sockaddr*) addr, &addrlen);
            TRY (recv_size,  perror ("recv");)
            write (STDOUT_FILENO, buf_stdout, recv_size);
        }

        if (CheckReadyForRead (STDIN_FILENO, 1))
        {
            send_size = read (STDIN_FILENO, buf_stdin + offset, BUFFER_SIZE - offset);
            if (send_size == -1)
            {
                perror ("fgets");
                return -1;
            }

            ((MessageStruct*) buf_stdin)->type = UDP_WORK_CONNECTION;
            if (send_size > 0)
                sendto (sockfd, buf_stdin, send_size + offset, 0, (struct sockaddr*) addr, addrlen);

            if (strcmp ("exit\n", buf_stdin + offset) == 0)
                return 0;
        }

        memset (buf_stdin,  0, BUFFER_SIZE);
        memset (buf_stdout, 0, recv_size);
    }

    return 0;
}

int ProcessBroadcast (int sockfd)
{
    int option_on = 1;
    TRY (setsockopt (sockfd, SOL_SOCKET, SO_BROADCAST, &option_on, sizeof (option_on)), log_info ("Setsockopt error.\n");)

    struct sockaddr_in addr = {AF_INET, DEFAULT_UDP_PORT, {}, {}};
    socklen_t addrlen = sizeof (addr);
	inet_aton ("192.168.0.255", &(addr.sin_addr));
    MessageStruct msg = {BROADCAST_SEARCHING};

    sendto (sockfd, &msg, sizeof (msg), 0, (struct sockaddr*) &addr, addrlen);

    while (CheckReadyForRead (sockfd, 1000)) // wait 1 second
    {
        recvfrom (sockfd, &msg, sizeof (msg), 0, (struct sockaddr*) &addr, &addrlen);
        printf ("Server was found in %s\n", inet_ntoa (addr.sin_addr));
    }

    return 0;
}

int ProcessFilecopy (int sockfd, struct sockaddr_in* addr, const char* file_from, const char* ip_and_file_to)
{
    char buf [BUFFER_SIZE] = "";
    char* colon = strchr (ip_and_file_to, ':');

    int fd = open (file_from, O_RDONLY);
    if (!fd)
        return -1;

    ((MessageStruct*) buf)->type = FILE_COPY_START;
    socklen_t addrlen = sizeof (*addr);

    ssize_t offset = sizeof (MessageType);
    strcpy (buf + offset, colon + 1);
    ssize_t send_size = strlen (buf + offset);

    sendto (sockfd, buf, send_size + offset, 0, (struct sockaddr*) addr, addrlen);
    ((MessageStruct*) buf)->type = FILE_COPY_PROCESS;

    while (1)
    {
        send_size = read (fd, buf + offset, BUFFER_SIZE - offset);
        if (send_size == -1)
            send_size = 0;
        sendto   (sockfd, buf, send_size + offset, 0, (struct sockaddr*) addr, addrlen);
        if (!send_size)
            break;

        do
        {
            recvfrom (sockfd, buf, offset, 0, (struct sockaddr*) addr, &addrlen);
        } while (((MessageStruct*) buf)->type != FILE_COPY_PROCESS);
    }

    close (fd);
    return 0;
}
