#include "myaddress.h"
#include "log.h"
#include "socket.h"
#include "iomanager.h"

HPGS::Logger::ptr g_logger = HPGS_LOG_ROOT();

void test(){
    std::vector<HPGS::Address::ptr> addrs;

    HPGS_LOG_INFO(g_logger) << "begin";
    //bool v = HPGS::Address::Lookup(addrs, "localhost:3080");
    bool v = HPGS::Address::Lookup(addrs, "www.baidu.com", AF_INET);

    HPGS_LOG_INFO(g_logger) << "end";

    if(!v){
        HPGS_LOG_ERROR(g_logger) << "lookup fail";
        return;
    }

    for(size_t i = 0; i < addrs.size(); i++){
        HPGS_LOG_INFO(g_logger) << i << " - " << addrs[i]->toString();
    }

    auto addr = HPGS::Address::LookupAny("localhost::61963");
    if(addr){
        HPGS_LOG_INFO(g_logger) << *addr;
    }
    else{
        HPGS_LOG_ERROR(g_logger) << "error";
    }
}

void test_iface(){
    std::multimap<std::string, std::pair<HPGS::Address::ptr, uint32_t> > results;

    bool v = HPGS::Address::GetInterfaceAddresses(results);
    if(!v){
        HPGS_LOG_ERROR(g_logger) << "GetInterfaceAddresses fail";
        return;
    }

    for(auto& i : results){
        HPGS_LOG_INFO(g_logger) << i.first << " - " << i.second.first->toString() << " - " << i.second.second;
    }
}

void test_ipv4(){
    //auto addr = HPGS::IPAddress::Create("www.sylar.top");
    auto addr = HPGS::IPAddress::Create("127.0.0.0");
    if(addr){
        HPGS_LOG_INFO(g_logger) << addr->toString();
    }
}

void test_socket(){
    //std::vector<HPGS::Address::ptr> addr;
    //HPGS::Address::Lookup(addrs, "www.baidu.com", AF_INET);
    //HPGS::IPAddress::ptr addr;
    //for(auto& i : addrs){
    //    HPGS_LOG_INFO(g_logger) << i->toString();
    //    addr = std::dynamic_pointer_cast<HPGS_IPAddress>(i);
    //    if(addr){
    //        break;
    //    }
    //}
    HPGS::IPAddress::ptr addr = HPGS::Address::LookupAnyIPAddress("www.baidu.com");
    if(addr){
        HPGS_LOG_INFO(g_logger) << "get address" << addr->toString();
    }
    else{
        HPGS_LOG_ERROR(g_logger) << "get addrss fail";
        return;
    }

    HPGS::Socket::ptr sock = HPGS::Socket::CreateTcp(addr);
    addr->setPort(80);
    HPGS_LOG_INFO(g_logger) << "addr = " << addr->toString();
    if(!sock->connect(addr)){
        HPGS_LOG_INFO(g_logger) << "connect " << addr->toString() << " fail";
        return;
    }
    else{
        HPGS_LOG_INFO(g_logger) << "connect " << addr->toString() << " connected";
    }

    const char buff[] = "GET / HTTP/1.0\r\n\r\n";
    int rt = sock->send(buff, sizeof(buff));
    if(rt <= 0){
        HPGS_LOG_INFO(g_logger) << "send fail rt = " << rt;
        return;
    }

    std::string buffs;
    buffs.resize(4096);
    rt = sock->recv(&buffs[0], buffs.size());

    if(rt <= 0){
        HPGS_LOG_INFO(g_logger) << buffs;
        return;
    }
    
    buffs.resize(rt);
    HPGS_LOG_INFO(g_logger) << buffs;
}

void test2(){
    HPGS::IPAddress::ptr addr = HPGS::Address::LookupAnyIPAddress("www.baidu.com:80");
    if(addr){
        HPGS_LOG_INFO(g_logger) << "get address:" << addr->toString();
    }
    else{
        HPGS_LOG_ERROR(g_logger) << "get address fail";
        return;
    }

    HPGS::Socket::ptr sock = HPGS::Socket::CreateTcp(addr);
    if(!sock->connect(addr)){
        HPGS_LOG_ERROR(g_logger) << "connect " << addr->toString() << " fail";
        return;
    }
    else{
        HPGS_LOG_INFO(g_logger) << "connect " << addr->toString() << " connected";
    }

    uint64_t ts = HPGS::GetCurrentUs();
    for(size_t i = 0; i < 10000000000ul; i++){
        if(int err = sock->getError()){
            HPGS_LOG_INFO(g_logger) << "err = " << err << " errstr = " << strerror(err);
            break; 
        }

        //struct tcp_info tcp_info;
        //if(!sock->getOption(IPPROTO_TCP, TCP_INFO, tcp_info)) {
        //    SYLAR_LOG_INFO(g_looger) << "err";
        //    break;
        //}
        //if(tcp_info.tcpi_state != TCP_ESTABLISHED) {
        //    SYLAR_LOG_INFO(g_looger)
        //            << " state=" << (int)tcp_info.tcpi_state;
        //    break;
        //}
        static int batch = 10000000;
        if(i && (i & batch) == 0){
            uint64_t ts2 = HPGS::GetCurrentUs();
            HPGS_LOG_INFO(g_logger) << "i = " << i << " used:" << ((ts2 - ts) * 1.0 / batch) << "us";
            ts = ts2;
        }
    }
}

int main(int argc, char* argv[]){
    //test_ipv4();
    //test_iface();
    //test();

    //socket
    HPGS::IOManager iom;
    //iom.schedule(test_socket);
    iom.schedule(&test2);
    return 0;
}