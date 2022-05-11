// Include part

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>

// Socket part

#define DEFAULT_TCP_PORT htons (8000)
#define DEFAULT_UDP_PORT htons (8080)
#define DEFAULT_IP htonl (INADDR_ANY)
#define QUEUE_MAX_LEN 128
#define BUFFER_SIZE (1 << 20) // 1 MB

typedef enum
{
    TCP = 0,
    UDP = 1
} Protocol;

// Functions part

int OpenSocket (Protocol prt, in_addr_t ip_addr, in_port_t port);
static inline int CloseSocket (int sockfd) {return close (sockfd);}

// Logging part

#define LOG_BUF_SIZE 1024
static int LOG_FD = 0;

void WriteLog (int logfd,
              const char* file     __attribute_maybe_unused__,
              const char* function __attribute_maybe_unused__, 
              int line             __attribute_maybe_unused__,
              const char* type     __attribute_maybe_unused__, 
              const char* frmt, ...) __attribute__ ((format(printf, 6, 7)));

#define log(type, frmt, ...) WriteLog (LOG_FD, __FILE__, __func__, __LINE__, type, frmt, ##__VA_ARGS__)
#define log_info(frmt, ...)  log ("INFO",  frmt, ##__VA_ARGS__)
#define log_error(frmt, ...) log ("ERROR", frmt, ##__VA_ARGS__)

#define TRY(cond, action)  if ((cond) == -1)   \
                           {                   \
                               {action}        \
                               return -1;      \
                           }
