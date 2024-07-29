#include "tcp_server.h"
#include "iomanager.h"
#include "log.h"

HPGS::Logger::ptr g_logger = HPGS_LOG_ROOT();

void run(){
    auto addr = HPGS::Address::LookupAny("0.0.0.0:8033");
    //auto addr2 = HPGS::UnixAddress::ptr(new HPGS::UnixAddress("/tmp/unix_addr"));
    std::vector<HPGS::Address::ptr> addrs;
    addrs.push_back(addr);
    //addrs.push_back(addr2);

    HPGS::TcpServer::ptr tcp_server(new HPGS::TcpServer);
    std::vector<HPGS::Address::ptr> fails;
    while(!tcp_server->bind(addrs, fails)){
        sleep(2);
    }
    tcp_server->start();
}

int main(int argc, char* argv[]){
    HPGS::IOManager iom(2);
    iom.schedule(run);
    return 0;
}