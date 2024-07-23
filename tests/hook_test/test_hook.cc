#include "hook.h"
#include "log.h"
#include "iomanager.h"
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>


HPGS::Logger::ptr g_logger = HPGS_LOG_ROOT();

void test_sleep(){
    HPGS::IOManager iom(1);
    iom.schedule([](){
        sleep(2);
        HPGS_LOG_INFO(g_logger) << "sleep 2";
    });

    iom.schedule([](){
        sleep(3);
        HPGS_LOG_INFO(g_logger) << "sleep 3";
    });
    HPGS_LOG_INFO(g_logger) << "test_sleep";
}

void test_sock(){
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "192.168.205.137", &addr.sin_addr.s_addr);

    HPGS_LOG_INFO(g_logger) << "begin connect";
    int rt = connect(sock, (const struct sockaddr*)&addr, sizeof(addr));
    HPGS_LOG_INFO(g_logger) << "connect rt = " << rt << " errno = " << errno;

    if(rt){
        return;
    }

    const char data[] = "GET / HTTP/1.0\r\n\r\n";
    rt = send(sock, data, sizeof(data), 0);
    HPGS_LOG_INFO(g_logger) << "send rt = " << rt << " errno = " << errno;

    if(rt <= 0){
        return;
    }

    std::string buff;
    buff.resize(4096);

    rt = recv(sock, &buff[0], buff.size(), 0);
    HPGS_LOG_INFO(g_logger) << "recv rt = " << rt << " errno = " << errno;

    if(rt <= 0){
        return;
    }

    buff.resize(rt);
    HPGS_LOG_INFO(g_logger) << buff;
}

int main(int argc, char* argv[]){
    //test_sleep();

    HPGS::IOManager iom;
    iom.schedule(test_sock);

    return 0;
}