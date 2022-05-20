#define _GNU_SOURCE // ToDo: Why??

#include "my_inet_lib.h"

#include <termios.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>

const size_t TCP_TICKS_WAIT = 60000; // 1 minute
const struct timespec TCP_CHECK_PERIOD = {0, 1000000}; // 1 ms
const int PTY_TIMEOUT = 1000; // 1 second

char BASH_DEFAULT_NAME[] = "sh";
char* const BASH_DEFAULT_ARGS[] = {BASH_DEFAULT_NAME, NULL};


typedef struct __ClientUDP
{
    struct sockaddr_in addr;
    int master;
    struct __ClientUDP* next;
} ClientUDP;

ClientUDP* ClientUDPConstructor (const struct sockaddr_in* addr);
void SendToAllClients (int sockfd, ClientUDP* client_list);
ClientUDP* FindClientFromList (ClientUDP* client_list, const struct sockaddr_in* addr);
void ClientUDPDestructor (ClientUDP* client);

void CopyFileFromClient (int sockfd, char* buf, const struct sockaddr_in* client_info);

int OpenSocketUDP();
int OpenSocketTCP();

void RunServer (int udp_sockfd, int tcp_sockfd);
int ProcessUDP (int sockfd);
int ProcessTCP (int sockfd);
int ProcessOneClientTCP (int sockfd, struct sockaddr_in* addr);

int CreateNewTerminal();

int main()
{
    close (STDIN_FILENO);
    close (STDOUT_FILENO);
    close (STDERR_FILENO);

    #define ACTION(str) {log_error(str); close (LOG_FD); return 0;}
    // SignalFixer (0);

    LOG_FD = open ("my_ssh.log", O_APPEND | O_CREAT | O_WRONLY, 0666); // In production: /var/log/my_ssh.log
    int udp_sockfd = 0, tcp_sockfd = 0;

    TRY (udp_sockfd = OpenSocketUDP(), ACTION ("Can't open UDP server.\n"));
    log_info ("UDP port opened.\n");

    TRY (tcp_sockfd = OpenSocketTCP(), ACTION ("Can't open TCP server.\n"));
    log_info ("TCP port opened.\n");

    RunServer (udp_sockfd, tcp_sockfd);

    CloseSocket (udp_sockfd);
    CloseSocket (tcp_sockfd);

    close (LOG_FD);

    // SignalFixer (0);
    // int a = CreateNewTerminal();
    // int b = CreateNewTerminal();
    // close (a);
    // close (b);

    return 0;
    #undef ACTION
}

int OpenSocketUDP()
{
    return OpenSocket (UDP, DEFAULT_IP, DEFAULT_UDP_PORT);
}

int OpenSocketTCP()
{
    int sockfd = 0;
    TRY (sockfd = OpenSocket (TCP, DEFAULT_IP, DEFAULT_TCP_PORT), {});
    TRY (listen (sockfd, QUEUE_MAX_LEN), log_error ("Listen error.\n"););
    return sockfd;
}

void RunServer (int udp_sockfd, int tcp_sockfd)
{
    // ToDo: fork error?
    int udp_child = 0, tcp_child = 0;   

    if (!(udp_child = fork()))
    {
        CloseSocket (tcp_sockfd);
        ProcessUDP (udp_sockfd);
        return;
    }

    if (!(tcp_child = fork()))
    {
        CloseSocket (udp_sockfd);
        ProcessTCP (tcp_sockfd);
        return;
    }

    while (1) // ToDo: log buffer flushing, checking are TCP and UDP ok?
    {
        sleep (1);
    }
}

int ProcessUDP (int sockfd)
{
    struct sockaddr_in addr = {};
    socklen_t addrlen = sizeof (addr);
    char buf[BUFFER_SIZE] = "";
    ssize_t recv_size = 0;

    ClientUDP* client_list = NULL;

    while (1)
    {
        SendToAllClients (sockfd, client_list);

        if (!CheckReadyForRead (sockfd, 1))
            continue;

        TRY (recv_size = recvfrom (sockfd, buf, BUFFER_SIZE - 1, 0, (struct sockaddr*) &addr, &addrlen),
             log_error ("UDP receiving error, errno = %d.\n", errno);) // ToDo: print errno - ok?

        MessageStruct* msg = (MessageStruct*) buf;
        if (recv_size < (ssize_t) sizeof (msg->type))
            continue;

        switch (msg->type)
        {
            case UDP_START_CONNECTION:
            {
                ClientUDP* old_client = FindClientFromList (client_list, &addr);
                if (old_client)
                    // ToDo: Destructor
                    continue;

                ClientUDP* new_client = ClientUDPConstructor (&addr);
                if (client_list)
                {
                    new_client->next = client_list;
                    client_list = new_client;
                }
                else
                    client_list = new_client;

                break;
            }

            case UDP_WORK_CONNECTION:
            {
                ClientUDP* client = FindClientFromList (client_list, &addr);
                if (client)
                    write (client->master, buf + sizeof (msg->type), recv_size - sizeof (msg->type));

                break;
            }

            case UDP_END_CONNECTION:
                break;

            case LOGIN_INFO:
                break;

            case BROADCAST_SEARCHING:
                sendto (sockfd, buf, recv_size, 0, (struct sockaddr*) &addr, addrlen);
                break;

            case FILE_COPY_START:
                // ToDo: It's blocking, not good
                CopyFileFromClient (sockfd, buf, &addr);
                break;

            case FILE_COPY_PROCESS:
                break;

            default:
                break;
        }
        // ToDo: fork with fork_counter?
        // ToDo: list of structures.
        // HandleUDP ();
        
        memset (&buf, 0, BUFFER_SIZE); // ToDo: Maybe not all buffer?
    }
}

int ProcessTCP (int sockfd)
{
    struct sockaddr_in addr = {};
    socklen_t addrlen = sizeof (addr);
    int connect_sockfd = 0;

    while (1)
    {
        TRY (connect_sockfd = accept (sockfd, (struct sockaddr*) &addr, &addrlen), 
             log_info ("TCP connection error, errno = %d.\n", errno);)

        if (errno != ECONNABORTED && errno != 0)
        {
            log_error ("Connection error is fatal, errno = %d.\n", errno);
            return -1; // ToDo: kill all childs?
        }

        if (!fork())
            break;
        else
            log_info ("TCP connection with %s:%d successfuly created.\n", 
                      inet_ntoa (addr.sin_addr), (unsigned int) ntohs (addr.sin_port));
    }
    
    ProcessOneClientTCP (connect_sockfd, &addr);
    log_info ("Client %s:%u was disconnected.\n", inet_ntoa (addr.sin_addr), (unsigned int) ntohs (addr.sin_port));
    CloseSocket (connect_sockfd);
    return 0;
}

int ProcessOneClientTCP (int sockfd, struct sockaddr_in* addr)
{
    #define CLIENT_ADDR_INFO inet_ntoa (addr->sin_addr), (unsigned int) ntohs (addr->sin_port)

    char buf_stdin [BUFFER_SIZE] = "",
         buf_stdout[BUFFER_SIZE] = "";
    size_t tick_counter = 0;

    int master = CreateNewTerminal();
    TRY (master, log_info ("Can't create new terminal for %s:%u.\n", CLIENT_ADDR_INFO);)

    while (1)
    {
        ssize_t send_size = 0, recv_size = 0;

        if (CheckReadyForRead (master, 1))
        {
            TRY (send_size = read (master, buf_stdout, BUFFER_SIZE - 1), 
                log_info ("PTY read error from address %s:%u, errno = %d.\n", CLIENT_ADDR_INFO, errno);)
            TRY (send (sockfd, buf_stdout, send_size, 0), log_info ("Can't send message.\n"););
        }

        if (CheckReadyForRead (sockfd, 1))
        {
            tick_counter = 0;
            recv_size = recv (sockfd, buf_stdin, BUFFER_SIZE, 0);
            
            TRY (write (master, buf_stdin, recv_size), 
                log_info ("PTY write error from address %s:%u, errno = %d.\n", CLIENT_ADDR_INFO, errno);
                return -1;)

            if (strncmp ("exit\n", buf_stdin, recv_size) == 0)
                return 0;
        }

        tick_counter += 2;
        if (tick_counter >= TCP_TICKS_WAIT)
        {
            log_info ("Request timeout from address %s:%u.\n", CLIENT_ADDR_INFO);
            return -1;
        }

        memset (buf_stdin,  0, recv_size);
        memset (buf_stdout, 0, send_size);

        // ToDo: read in other thread.  
    }
    
    return 0;
    #undef CLIENT_ADDR_INFO
}

int CreateNewTerminal()
{
    struct termios t = {};
    int master = 0;

    TRY (master = posix_openpt(O_RDWR | O_NOCTTY), 
         log_error ("Posix_openpt error, errno = %d.\n", errno);)

    TRY (grantpt (master), log_error ("Grantpt error, errno = %d.\n", errno);)

    TRY (unlockpt (master), log_error ("Unlockpt error, errno = %d.\n", errno);)

    TRY (tcgetattr(master, &t), log_error ("Tcgetattr error, errno = %d.\n", errno);)

    cfmakeraw(&t);

    TRY (tcsetattr(master, TCSANOW, &t), log_error ("Tcsetattr error, errno = %d.\n", errno);)

    if (!fork()) 
    {
        // setgid (0);
        setsid();

        // close (STDIN_FILENO);
        // close (STDOUT_FILENO);
        // close (STDERR_FILENO);

        int term = open (ptsname (master), O_RDWR);
        TRY (term, log_info ("Cannot open slave terminal.\n");)

        dup2 (term, STDIN_FILENO);
        dup2 (term, STDOUT_FILENO);
        dup2 (term, STDERR_FILENO);

        close (master);

        TRY (execvp (BASH_DEFAULT_NAME, BASH_DEFAULT_ARGS), log_info ("Cannot open bash.\n");)
    }

    return master;
}

ClientUDP* ClientUDPConstructor (const struct sockaddr_in* addr)
{
    log_info ("UDP connection with %s:%d successfuly created.\n", 
                      inet_ntoa (addr->sin_addr), (unsigned int) ntohs (addr->sin_port));

    ClientUDP* client = calloc (1, sizeof (*client));
    client->addr   = *addr;
    client->master = CreateNewTerminal();
    return client;
}

void SendToAllClients (int sockfd, ClientUDP* client_list)
{
    char buf[BUFFER_SIZE] = "";

    while (client_list)
    {
        while (CheckReadyForRead (client_list->master, 1))
        {
            ssize_t send_size = read (client_list->master, buf, BUFFER_SIZE);
            if (send_size > 0)
            {
                log_info ("%s\n", buf);
                sendto (sockfd, buf, send_size, 0, (struct sockaddr*) &(client_list->addr), sizeof (*client_list));
            }
        }
        client_list = client_list->next;
    }
}

ClientUDP* FindClientFromList (ClientUDP* client_list, const struct sockaddr_in* addr)
{
    if (!client_list)
        return NULL;

    while (client_list && !(client_list->addr.sin_addr.s_addr == addr->sin_addr.s_addr && 
                            client_list->addr.sin_port        == addr->sin_port)) 
    {
        client_list = client_list->next;
    }
    return client_list;
}

void ClientUDPDestructor (ClientUDP* client)
{
    close (client->master);
    free (client);
}

void CopyFileFromClient (int sockfd, char* buf, const struct sockaddr_in* client_info)
{
    MessageStruct* msg = (MessageStruct*) buf;
    int fd = open (buf + sizeof (msg->type), O_WRONLY | O_TRUNC | O_CREAT, 0666);
    if (fd == -1)
        return;

    struct sockaddr_in addr = {};
    socklen_t addrlen = sizeof (addr);

    while (1)
    {
        if (!CheckReadyForRead (sockfd, 5000))
            break;

        ssize_t recv_size = recvfrom (sockfd, buf, BUFFER_SIZE - 1, 0, (struct sockaddr*) &addr, &addrlen);
        if (addr.sin_addr.s_addr != client_info->sin_addr.s_addr || 
            addr.sin_port        != client_info->sin_port || msg->type != FILE_COPY_PROCESS)
            continue;

        if (recv_size == sizeof (msg->type))
            break;

        write (fd, buf + sizeof (msg->type), recv_size - sizeof (msg->type));
        sendto (sockfd, buf, sizeof (msg->type), 0, (struct sockaddr*) &addr, addrlen);
    }

    close (fd);
}
