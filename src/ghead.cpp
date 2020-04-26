#include "ghead.h"
#include <stdio.h>
#include <stdarg.h>
#include <poll.h>
#include <chrono>
#include "log.h"

namespace galois::ghead {

const unsigned int ghead::GHEAD_MAGICNUM = 0xe8c4a59;

RETURN_CODE ghead::gread(int sock, ghead* head, size_t buflen, int timeout) {
    if (sock < 0 || !head || buflen < sizeof(ghead)) {
        WARNING("[galois head] param error.", "");
        return RET_EPARAM;
    }
    // read head
    int rlen = sync_read_n_tmo(sock, reinterpret_cast<uint8_t*>(head), 
        sizeof(ghead), timeout);
    if (rlen <= 0) {
        // read fail 
        goto ghead_read_fail;
    } else if (rlen != sizeof(ghead)) {
        // timeout
        WARNING("<%u>[galois head] read head incomplete: receive[%d] want[%u]",
                head->log_id, rlen, sizeof(ghead));
        return RET_READHEAD;
    }
    TRACE("<%u>[galois head] read head succeed: body_len:[%u]", 
        head->log_id, head->body_len);

    // check magic
    if (head->magic_num != GHEAD_MAGICNUM) {
        ERROR("<%u>[galois head] magic num mismatch: receive[%x] want[%x]",
                head->log_id, head->magic_num, GHEAD_MAGICNUM);
        return RET_EMAGICNUM;
    }
    TRACE("<%u>[galois head] check magic succeed: magic:[%x]", 
        head->log_id, head->magic_num);

    // check reqsize
    if (buflen < sizeof(ghead) + head->body_len) {
        WARNING("<%u>[galois head] buffer too small: bodylen[%u] buflen[%u(%u|%u)]",
            head->log_id, head->body_len, buflen - sizeof(ghead), buflen, sizeof(ghead));
        return RET_EBODYLEN;
    }
    TRACE("<%u>[galois head] check size succeed: bodylen[%u] buflen[%u][%u|%u]", 
        head->log_id, head->body_len, buflen - sizeof(ghead), buflen, sizeof(ghead));

    // read body
    if (head->body_len > 0) {
        rlen = sync_read_n_tmo(sock, head->body, head->body_len, timeout);
        if (rlen <= 0) {
            // read fail
            goto ghead_read_fail;
        } else if (rlen != (int)head->body_len) {
            // timeout
            WARNING("<%u>[galois head] read body incomplete: receive[%d] want[%u]",
                    head->log_id, rlen, head->body_len);
            return RET_READ;
        }
    }
    return RET_SUCCESS;

ghead_read_fail:
    if (rlen == 0) {
        return RET_PEARCLOSE;
    }
    WARNING("<%u>[galois head] read fail: ret[%d] errno:[%d|%s]",
        head->log_id, rlen, errno, strerror(errno));
    if (rlen == -1 && errno == ETIMEDOUT) {
        return RET_ETIMEDOUT;
    } else {
        return RET_READ;
    }
}


// return -1: read fail
// return -
ssize_t ghead::sync_read_n_tmo(int fd, uint8_t* ptr, size_t nbytes, int timeout_ms) {
    if (ptr == nullptr || nbytes == 0) {
        TRACE("[galois head] param error.", "");
        return -1;
    }
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    size_t nleft = nbytes;
    while (nleft > 0) {
        TRACE("[galois head] waiting for read poll ready.", "");
        int presult = poll_wrap(&pfd, 1, timeout_ms);
        TRACE("[galois head] read poll ready.", "");
        if (presult > 0) {
            int nread = ::read(fd, ptr, nleft);
            if (nread < 0) {
                if (errno == EINTR) {
                    TRACE("[galois head] read interrupt by EINTR.", "");
                    continue;
                } else {
                    TRACE("[galois head] read fail return[%d] errno[%d|%s]", 
                        nread, errno, strerror(errno));
                    return -1;
                }
            } else if (nread == 0) {
                TRACE("[galois head] read connection invalid read return [0]", "");
                break;
            }
            ptr += nread;
            nleft -= nread;
            TRACE("[galois head] read[%u] left[%u]", nread, nleft);
        } else if (presult == 0) {
            TRACE("[galois head] read poll timeout.", "");
            break;
        } else {
            WARNING("[galois head] read poll fail.", "");
            return -1;
        }
    }
    return nbytes - nleft;
}

RETURN_CODE ghead::gwrite(int sock, ghead * head, size_t buflen, int timeout) {
    if (sock < 0 || !head || buflen < sizeof(ghead))
        return RET_EPARAM;
    // write head
    int rlen = 0;
    head->magic_num = GHEAD_MAGICNUM;
    rlen = sync_write_n_tmo(sock, reinterpret_cast<uint8_t*>(head), sizeof(ghead), timeout);
    if (rlen <= 0) {
        goto ghead_write_fail;
    } else if (rlen != sizeof(ghead)) {
        WARNING("<%u>[galois head] write head incomplete: write[%d] want[%u]",
                head->log_id, rlen, sizeof(ghead));
        return RET_WRITEHEAD;
    }
    TRACE("<%u>[galois head] write head succeed: body_len:[%u] magic:[%x]", 
        head->log_id, head->body_len, head->magic_num);

    // check reqsize
    if (buflen < sizeof(ghead) + head->body_len) {
        WARNING("<%u>[galois head] buffer too small: bodylen[%u] buflen[%u(%u|%u)]",
            head->log_id, head->body_len, buflen - sizeof(ghead), buflen, sizeof(ghead));
        return RET_EBODYLEN;
    }
    TRACE("<%u>[galois head] check size succeed: bodylen[%u] buflen[%u][%u|%u]", 
        head->log_id, head->body_len, buflen - sizeof(ghead), buflen, sizeof(ghead));

    // read body
    if (head->body_len > 0) {
        rlen = sync_write_n_tmo(sock, head->body, head->body_len, timeout);
        if (rlen <= 0) {
            goto ghead_write_fail;
        } else if (rlen != (int)head->body_len) {
            WARNING("<%u>[galois head] write body incomplete: write[%d] want[%u] errno[%d|%s]",
                    head->log_id, rlen, head->body_len, errno, strerror(errno));
            return RET_WRITE;
        }
    }
    return RET_SUCCESS;

ghead_write_fail:
    if (rlen == 0) {
        return RET_PEARCLOSE;
    }
    WARNING("<%u>[galois head] write fail: ret[%d] errno[%d|%s]",
        head->log_id, rlen, errno, strerror(errno));
    if (rlen == -1 && errno == ETIMEDOUT) {
        return RET_ETIMEDOUT;
    } else {
        return RET_WRITE;
    }

}

ssize_t ghead::sync_write_n_tmo(int fd, uint8_t * ptr, size_t nbytes, int timeout_ms) {
    if (ptr == nullptr || nbytes == 0) {
        TRACE("[galois head] param error.", "");
        return -1;
    }
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLOUT;
    size_t nleft = nbytes;
    while(nleft > 0) {
        TRACE("[galois head] waiting for write poll ready.", "");
        if (!set_sock_noblock(fd)) {
            TRACE("[galois head] write set_sock_noblock fail.", "");
            return -1;
        }
        int presult = poll_wrap(&pfd, 1, timeout_ms);
        TRACE("[galois head] write poll ready.", "");
        if (presult > 0) {
            if (!set_sock_noblock(fd)) {
                TRACE("[galois head] write set_sock_noblock fail.", "");
                return -1;
            }
            int nwrite = ::write(fd, ptr, nleft);
            if (nwrite < 0) {
                if (errno == EINTR) {
                    TRACE("[galois head] write interrupt by EINTR.", "");
                    continue;
                } else {
                    TRACE("[galois head] write fail return[%d] errno[%d|%s]", 
                        nwrite, errno, strerror(errno));
                    return -1;
                }
            } else if (nwrite == 0) {
                TRACE("[galois head] write connection invalid write return [0]", "");
                break;
            }
            ptr += nwrite;
            nleft -= nwrite;
            TRACE("[galois head] write[%u] left[%u]", nwrite, nleft);
        } else if (presult == 0) {
            TRACE("[galois head] write poll timeout.", "");
            break;
        } else {
            WARNING("[galois head] write poll fail.", "");
            return -1;
        }
    }
    return nbytes - nleft;
}

// A simple wrap function of linux poll
// fdarray : pollfd array
// nfds : count of fds
// timeout : timeout(ms) or (-1,0,>0)
// return  -1, 0, nfds
int ghead::poll_wrap(pollfd * fdarray, nfds_t nfds, int timeout_ms) {
    using namespace std::chrono;
    if (fdarray == nullptr || nfds == 0)
        return -1;
    int ret_val = 0;
    auto start = system_clock::now();
    duration rest_timeout = milliseconds(timeout_ms); 
    while (true) {
        ret_val = poll(fdarray, nfds, 
            static_cast<int>(duration_cast<milliseconds>(rest_timeout).count()));
        if (ret_val > 0) {
            return ret_val;
        } else if (ret_val == 0) {
            errno = ETIMEDOUT;
            return ret_val;
        } else if (ret_val < 0 && errno == EINTR) {
            rest_timeout -= duration_cast<milliseconds>(system_clock::now() - start); 
            if (duration_cast<milliseconds>(rest_timeout).count() > 0) {
                continue;
            } else {
                errno = ETIMEDOUT;
                ret_val = 0;
                return ret_val;
            }
        } else {
            WARNING("[galois head] poll error:[%d][%s]", errno, strerror(errno));
            return ret_val;
        }
    }
    errno = ETIMEDOUT;
    return ret_val;
}

bool ghead::set_sock_noblock(int fd) {
    int flags;
    // On error, -1 is returned, and errno is set appropriately.
    while ((flags = fcntl(fd, F_GETFL, 0)) == -1) {
        if (errno == EINTR)
            continue;
        WARNING("[galois head] fcntl(%d,F_GETFL) failed: [%d|%s].", 
            fd, errno, strerror(errno));
        return false;
    }
    if (flags & O_NONBLOCK)
        return true;
    flags |= O_NONBLOCK;
    while (fcntl(fd, F_SETFL, flags) == -1) {
        if (errno == EINTR)
            continue;
        WARNING("[galois head]  fcntl(%d,F_SETFL,%d) failed: [%d|%s].", 
            fd, flags, errno, strerror(errno));
        return false;
    }
    return true;
}

}