#include "iomanager.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include "log.h"

HPGS::Logger::ptr g_logger = HPGS_LOG_ROOT();

int sock = 0;

void test_fibre(){
    HPGS_LOG_INFO(g_logger) << "test_fibre sock = " << sock;

    //sleep(3);

    //close(sock);
    //HPGS::IOManager::GetThis()->cancellAll(sock);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sock, F_SETFL, O_NONBLOCK);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "192.168.0.112", &addr.sin_addr.s_addr);

    if(!connect(sock, (const sockaddr*)&addr, sizeof(addr))){

    }
    else if(errno == EINPROGRESS){
        HPGS_LOG_INFO(g_logger) << "add evemt errno = " << errno << " " << strerror(errno);
        HPGS::IOManager::GetThis()->addEvent(sock, HPGS::IOManager::READ, [](){
            HPGS_LOG_INFO(g_logger) << "read callback";
        });
        HPGS::IOManager::GetThis()->addEvent(sock, HPGS::IOManager::WRITE, [](){
            HPGS_LOG_INFO(g_logger) << "write callback";
            //close(sock);
            HPGS::IOManager::GetThis()->cancelEvent(sock, HPGS::IOManager::READ);
            close(sock);
        });
    }
    else{
        HPGS_LOG_INFO(g_logger) << " else " << errno << " " << strerror(errno);
    }
}

void test1(){
    std::cout << "RPOLLIN = " << EPOLLIN
              << " EPOLLOUT = " << EPOLLOUT << std::endl;
    HPGS::IOManager iom(2, false);
    iom.schedule(&test_fibre);
}

HPGS::Timer::ptr s_timer;
void test_timer(){
    HPGS::IOManager iom(2);
    s_timer = iom.addTimer(1000, [](){
        static int i = 0;
        HPGS_LOG_INFO(g_logger) << "hello timer i = " << i;
        if(++i == 3){
            s_timer->reset(2000, true);
        }
    }, true);
}

int main(int argc, char* argv[]){
    //test_timer();
    test1();
    return 0;
}