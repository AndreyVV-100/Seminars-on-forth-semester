#define _GNU_SOURCE // ToDo: Why??

#include "my_inet_lib.h"

#include <termios.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
#include <stdlib.h>
#include <fcntl.h>

const size_t TCP_TICKS_WAIT = 60000; // 1 minute
const struct timespec TCP_CHECK_PERIOD = {0, 1000000}; // 1 ms
const int PTY_TIMEOUT = 1000; // 1 second

char BASH_DEFAULT_NAME[] = "sh";
char* const BASH_DEFAULT_ARGS[] = {BASH_DEFAULT_NAME, NULL};

int OpenSocketUDP();
int OpenSocketTCP();

void RunServer (int udp_sockfd, int tcp_sockfd);
int ProcessUDP (int sockfd);
int ProcessTCP (int sockfd);
int ProcessOneClientTCP (int sockfd, struct sockaddr_in* addr);

int CreateNewTerminal();

int main()
{
    #define ACTION(str) {log_error(str); close (LOG_FD); return 0;}

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
        ProcessUDP (udp_sockfd);
        return;
    }

    if (!(tcp_child = fork()))
    {
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

    while (1)
    {
        TRY (recvfrom (sockfd, buf, BUFFER_SIZE - 1, 0, (struct sockaddr*) &addr, &addrlen),
             log_error ("UDP receiving error, errno = %d.\n", errno);) // ToDo: print errno - ok?
        
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
        ssize_t send_size = 0;
        struct pollfd poll_wait_info = {master, POLLIN, 0};
        poll (&poll_wait_info, 1, PTY_TIMEOUT);

        if (poll_wait_info.revents & POLLIN)
        {
            TRY (send_size = read (master, buf_stdout, BUFFER_SIZE - 1), 
                log_info ("PTY read error from address %s:%u, errno = %d.\n", CLIENT_ADDR_INFO, errno);)

            TRY (send (sockfd, buf_stdout, send_size, 0), log_info ("Can't send message.\n"););
            memset (buf_stdout, 0, send_size);
            continue;
        }
        
        ssize_t recv_size = recv (sockfd, buf_stdin, BUFFER_SIZE, MSG_DONTWAIT);
        if (recv_size == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                nanosleep (&TCP_CHECK_PERIOD, NULL); 
                errno = 0;
                tick_counter++;
                if (tick_counter >= TCP_TICKS_WAIT)
                {
                    log_info ("Request timeout from address %s:%u.\n", CLIENT_ADDR_INFO);
                    return -1;
                }
                continue;
            }

            else 
            {
                log_info ("TCP receiving error from address %s:%u, errno = %d.\n", CLIENT_ADDR_INFO, errno);
                return -1;
            }
        }

        tick_counter = 0;
        TRY (write (master, buf_stdin, recv_size), 
             log_info ("PTY write error from address %s:%u, errno = %d.\n", CLIENT_ADDR_INFO, errno);
             return -1;)

        if (strncmp ("exit\n", buf_stdin, recv_size) == 0)
            return 0;

        memset (buf_stdin, 0, recv_size);

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
        setgid(0);

        close (STDIN_FILENO);
        close (STDOUT_FILENO);
        close (STDERR_FILENO);

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
