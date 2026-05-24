#pragma once
#include <Vortex-Net/Buffer.h>
#include <Vortex-Net/Timestamp.h>
#include <Vortex-Net/Callbacks.h>
#include <string>
#include <string>
class TLVCodec{
public:
    static void onTLVmessage(const TcpConnectionPtr& conn,Buffer* buf,Timestamp time);
    static void send(const TcpConnectionPtr& conn,uint16_t type,std::string msg);
};