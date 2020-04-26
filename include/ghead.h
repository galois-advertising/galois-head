#pragma once
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <map>
#include <string>
#include <poll.h>

namespace galois::ghead
{
enum GHEAD_FLAGS_T {
    GHEAD_CHECK_NONE = 0,
    GHEAD_CHECK_MAGICNUM = 0x01,
    GHEAD_CHECK_PARAM = 0x02,
};

enum RETURN_CODE {
    RET_SUCCESS = 0,
    RET_EPARAM = -1,
    RET_EBODYLEN = -2,
    RET_WRITE = -3,
    RET_READ = -4,
    RET_READHEAD = -5,
    RET_WRITEHEAD = -6,
    RET_PEARCLOSE = -7,
    RET_ETIMEDOUT = -8,
    RET_EMAGICNUM = -9,
    RET_UNKNOWN = -10
};
typedef int socket_t;
struct ghead 
{
    // No virtual, No 'this' pointer, all static except
    unsigned short id;
    unsigned short version;
    unsigned int log_id;
    char provider[16];
    unsigned int magic_num;
    unsigned int reserved1;
    unsigned int reserved2;
    unsigned int reserved3;
    unsigned int body_len;
    unsigned char body[];

    static RETURN_CODE gread(int sock, ghead* head, size_t buflen, int timeout);
    static RETURN_CODE gwrite(int sock, ghead* head, size_t buflen, int timeout);
private:
    static const unsigned int GHEAD_MAGICNUM;
    static ssize_t sync_read_n_tmo(socket_t fd, uint8_t* ptr1, size_t nbytes, int msecs);
    static ssize_t sync_write_n_tmo(int fd, uint8_t* ptr, size_t nbytes, int timeout_ms);
    static int poll_wrap(pollfd* fdarray, nfds_t nfds, int timeout);
    static bool set_sock_noblock(int fd);
};
}