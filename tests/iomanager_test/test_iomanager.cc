#include "iomanager.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include "log.h"
#include <random>
#include <fstream>


#define BUFFER_LENGTH 1024

HPGS::Logger::ptr g_logger = HPGS_LOG_ROOT();

bool running = true;
int sock = 0;
HPGS::Mutex s_mutex;

void accept_cb(int fd);
void recv_cb(int fd);
void send_cb(int fd, char* send_buffer, int buf_len);

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
    addr.sin_port = htons(2000);
    //addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //convert IPv4 and IPv6 addresses from text to binary form
    inet_pton(AF_INET, "192.168.205.137", &addr.sin_addr.s_addr);
    if(!connect(sock, (struct sockaddr*)&addr, sizeof(addr))){

        HPGS_LOG_INFO(g_logger) << "connected succed";
    }
    else if(errno == EINPROGRESS){
        HPGS_LOG_INFO(g_logger) << "add event errno = " << errno << " " << strerror(errno);
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
    std::cout << "EPOLLIN = " << EPOLLIN
              << " EPOLLOUT = " << EPOLLOUT << std::endl;
    HPGS::IOManager iom(3, false);
    HPGS_LOG_INFO(g_logger) << "end create iomanager";
    HPGS::IOManager::GetThis()->schedule(&test_fibre);
}

int server_init(){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(2000);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1){
        HPGS_LOG_ERROR(g_logger) << "server bind failed";
        close(sockfd);
        return -1;
    }

    listen(sockfd, 10);

    HPGS_LOG_INFO(g_logger) << "listening: " << sockfd;

    HPGS::IOManager::GetThis()->addEvent(sockfd, HPGS::IOManager::READ, std::bind(accept_cb, sockfd));

    // iom.addTimer(0, [sockfd](){
    //     //HPGS_LOG_INFO(g_logger) << sockfd;
    //     iom.addEvent(sockfd, HPGS::IOManager::READ, std::bind(accept_cb, sockfd));
    // }, true);

    return sockfd;
}

HPGS::Timer::ptr s_timer;
void test_timer(){
    HPGS::IOManager iom(1);
    s_timer = iom.addTimer(1000, [](){
        static int i = 0;
        HPGS_LOG_INFO(g_logger) << "hello timer i = " << i;
        if(++i == 3){
            s_timer->reset(2000, true);
        }
    }, true);
}

void accept_cb(int fd){
    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);

    int clientfd = accept(fd, (struct sockaddr*)&clientfd, &len);
    HPGS_LOG_INFO(g_logger) << "accept succeed:" << clientfd;

    HPGS::IOManager::GetThis()->addEvent(fd, HPGS::IOManager::READ, std::bind(accept_cb, fd));
    HPGS::IOManager::GetThis()->addEvent(clientfd, HPGS::IOManager::READ, std::bind(recv_cb, clientfd));
    // con_list[clientfd].fd = clientfd;
    // con_list[clientfd].recv_action.recv_callback = recv_callback;
    // con_list[clientfd].send_callback = send_callback;
    // memset(con_list[clientfd].rbuffer, 0, BUFFER_LENGTH);
    // con_list[clientfd].rlength = 0;
    // memset(con_list[clientfd].wbuffer, 0, BUFFER_LENGTH);
    // con_list[clientfd].wlength = 0;

}

void recv_cb(int fd){
    char buffer[BUFFER_LENGTH];
    memset(buffer, 0, BUFFER_LENGTH);
    //std::string buf;
    int count = recv(fd, buffer, BUFFER_LENGTH, 0);
    if(count == 0){
        HPGS_LOG_INFO(g_logger) << "client disconnect: " << fd;
        HPGS::IOManager::GetThis()->cancelAll(fd);
        close(fd);
        return;
    }

    HPGS_LOG_INFO(g_logger) << buffer;

    HPGS::IOManager::GetThis()->addEvent(fd, HPGS::IOManager::WRITE, std::bind(send_cb, fd, buffer, count));
}

void send_cb(int fd, char* send_buffer, int buf_len){
    int count = send(fd, send_buffer, buf_len, 0);
    if(count <= 0){
        HPGS_LOG_ERROR(g_logger) << "send error: " << count;
        return;
    }

    HPGS::IOManager::GetThis()->addEvent(fd, HPGS::IOManager::READ, std::bind(recv_cb, fd));
}

//一个函数只执行一次
void fibre_fun(std::ofstream& ofs, const std::string& content){
    HPGS::Mutex::Lock lock(s_mutex);
    struct tm tm;
    time_t time_ = time(0);
    localtime_r(&time_, &tm);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y:%m:%d %H:%M:%S", &tm);
    ofs << buf << " child fibre --" + content + ":" + std::to_string(HPGS::GetThreadId()) + "  " + std::to_string(HPGS::GetFibreId()) << std::endl;
}

void fibre_fun2(const std::string& content){
    HPGS::Mutex::Lock lock(s_mutex);
    HPGS_LOG_INFO(g_logger) << content; 
}

void th_fun(){
    HPGS_LOG_INFO(g_logger) << "th start";
    std::ofstream ofs("test.txt", std::ios::out | std::ios::app | std::ios::ate);

    auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(5);

    int i = 0;
    
    //线程执行5秒
    while(std::chrono::steady_clock::now() < end_time){
        std::random_device rd;
        std::mt19937 gen(rd()); // 标准的mersenne_twister_engine
        // 创建均匀分布,范围从0到500
        std::uniform_int_distribution<> dis(0, 500);
        uint32_t tem = dis(gen);

        std::string str = "some random message that will be send-->"+ std::to_string(i++) + " ";
        HPGS::Fibre::ptr fibre = std::make_shared<HPGS::Fibre>(std::bind(fibre_fun, std::ref(ofs), str));
        HPGS::IOManager::GetThis()->schedule(fibre);

        auto next_time = std::chrono::steady_clock::now();
        
        while(std::chrono::steady_clock::now() < next_time + std::chrono::milliseconds(tem)){}
    }
    
}

int main(int argc, char* argv[]){
    //test_timer();
    //test1();

    HPGS::IOManager iom(3, false);

    //int sockfd;

    //auto fd_ptr = std::make_shared<int>(sockfd);

    iom.schedule(server_init);

    iom.addTimer(2000, &th_fun, true);

    // iom.addConditionTimer(0, [sockfd](){
    //     HPGS_LOG_INFO(g_logger) << sockfd;
    //     iom.addEvent(sockfd, HPGS::IOManager::READ, std::bind(accept_cb, sockfd));
    // }, std::weak_ptr<int>(fd_ptr), true);

    return 0;
}