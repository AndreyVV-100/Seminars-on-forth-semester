#include "my_inet_lib.h"

int OpenSocket (Protocol prt, in_addr_t ip_addr, in_port_t port)
{
    #define ACTION(str) close (sockfd); log_error (str);

    int sockfd = socket (AF_INET, (prt == TCP) ? SOCK_STREAM : SOCK_DGRAM, 0);
    TRY (sockfd, ACTION ("Socket error.\n"));

    if (port != 0)
    {
        struct sockaddr_in addr = {AF_INET, port, {ip_addr}, {}};
        TRY (bind (sockfd, (struct sockaddr*) &addr, sizeof (struct sockaddr_in)), ACTION ("Bind error.\n"));
    }

    int reuse_on = 1;
    TRY (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse_on, sizeof (reuse_on)), ACTION ("Setsockopt error.\n"))

    return sockfd;
}

void WriteLog (int logfd,
              const char* file     __attribute_maybe_unused__,
              const char* function __attribute_maybe_unused__, 
              int line             __attribute_maybe_unused__,
              const char* type     __attribute_maybe_unused__, 
              const char* frmt, ...)
{
    assert (file);
    assert (function);
    assert (frmt);

    // ToDo: create config file
    size_t offset = 0;
    char buf[LOG_BUF_SIZE] = "";

    #ifdef LOG_WRITE_TIME
        time_t time_now_num = 0;
        time (&time_now_num);
        const struct tm* time_now = localtime (&time_now_num);
        assert (time_now);
        offset += snprintf (buf + offset, LOG_BUF_SIZE - offset, 
                                         "[%02d.%02d.%04d %02d:%02d:%02d]\t", time_now->tm_mday, time_now->tm_mon, time_now->tm_year + 1900,
                                         /*dd.mm.yyyy hh:mm:ss*/              time_now->tm_hour, time_now->tm_min, time_now->tm_sec);
    #endif

    #ifdef LOG_WHITE_POS
        offset += snprintf (buf + offset, LOG_BUF_SIZE - offset, "<%s::%s::%d>\t", file, function, line);
    #endif

    #ifdef LOG_WRITE_TYPE
        offset += snprintf (buf + offset, LOG_BUF_SIZE - offset, "{%s}\t", type);
    #endif

    va_list user_params = {};
    va_start (user_params, frmt);

    offset += vsnprintf (buf + offset, LOG_BUF_SIZE - offset, frmt, user_params);
    write (logfd, buf, offset);

    return;
}

int CheckReadyForRead (int sockfd, int timeout)
{
    struct pollfd poll_wait_info = {sockfd, POLLIN, 0};
    poll (&poll_wait_info, 1, timeout);
    return poll_wait_info.revents & POLLIN;
}
