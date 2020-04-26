#include <iostream>
#include <string>
#include <fstream>
#include <sys/types.h>    /* basic system data types */
#include <sys/socket.h>    /* basic socket definitions */
#include <sys/time.h>    /* timeval{} for select() */
#include <time.h>        /* timespec{} for pselect() */
#include <netinet/in.h>    /* sockaddr_in{} and other Internet defns */
#include <arpa/inet.h>    /* inet(3) functions */
#include <errno.h>
#include <fcntl.h>        /* for nonblocking */
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>    /* for S_xxx file mode constants */
#include <sys/uio.h>        /* for iovec{} and readv/writev */
#include <unistd.h>
#include <sys/wait.h>
#include <sys/un.h>    
#include "ghead.h"
#include <thread>
using namespace galois::ghead;

void read(uint8_t * buf) {
    std::fstream  afile;
    afile.open("./server_ghead.txt", std::ios::in);
    if (afile.is_open() == false) {
        std::cout<<"Cannot find ./client_ghead.txt"<<std::endl;
        return;
    }
    ghead * phead = reinterpret_cast<ghead*>(buf);
    afile>>phead->id>>phead->magic_num>>phead->log_id;
    afile>>phead->provider;
    afile>>phead->reserved1>>phead->reserved2;
    afile>>phead->body_len;
    afile>>*(buf + sizeof(ghead));
    *(buf + sizeof(ghead) + phead->body_len) = '\0';

}

void query_thread(int fd) {
    std::cout<<"new thread -----------------"<<std::endl;
    char buf[58];
    ghead * head = reinterpret_cast<ghead*>(buf);
    if (RET_SUCCESS == ghead::gread(fd, head, sizeof(buf), 10000))
    {
        buf[sizeof(ghead) + head->body_len] = 0;
        std::cout<<"Body:"<<head->body<<std::endl;
        size_t request_body_len = strlen(reinterpret_cast<const char *>(head->body));
        std::cout<<"request_body_len:"<<request_body_len<<std::endl;
        size_t response_len = sizeof(ghead) + request_body_len + 2;
        unsigned char * response = new unsigned char[response_len + 1];
        memset(response, 0, response_len + 1);
        ghead * p = reinterpret_cast<ghead*>(response);
        p->body_len = request_body_len + 2;
        p->body[0] = '[';
        p->body[request_body_len + 2 - 1] = ']';
        strncpy(reinterpret_cast<char *>(&p->body[1]), 
            reinterpret_cast<const char *>(head->body), request_body_len);
        ghead::gwrite(fd, p, response_len, 10000);
        std::cout<<p->body<<std::endl;
        delete [] response;
    }
    std::cout<<"close fd   -----------------"<<std::endl;
    close(fd);
}

int listenfd;
void on_sigterm(int arg) {
    (void) arg;
    close(listenfd);
}

int main(int argc, char * argv[]) {
    signal(SIGPIPE, SIG_IGN); 
    signal(SIGTERM, on_sigterm);
    int connfd;
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(8707);

    bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

    listen(listenfd, 5);
    while(true) {
        clilen = sizeof(cliaddr);
        std::cout<<"accept..."<<std::endl;
        if ( (connfd = accept(listenfd, (struct sockaddr*) &cliaddr, &clilen)) < 0) {
            if (errno == EINTR)
                continue;        /* back to for() */
            else
                std::cout<<"accept error"<<std::endl;
        } else {
            std::thread(query_thread, connfd).detach();
        }
    }
    //std::cout<<"should not be here"<<std::endl;
    close(listenfd);
    return 0;
}