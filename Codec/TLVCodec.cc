#include "TLVCodec.h"
#include <Vortex-Net/Buffer.h>
#include <Vortex-Net/Timestamp.h>
#include <Vortex-Net/logger.h>
#include <Vortex-Net/TcpConnection.h>
#include "protocol.h"
#include <iostream>
#include <string>
#include <arpa/inet.h>
#define PROTOCOL_MAGIC 0x12345678
#define PROTOCOL_VERSION
//接收数据并反序列化
void TLVCodec::onTLVmessage(const TcpConnectionPtr& conn,Buffer* buf,Timestamp time){
    while(buf->readableBytes()>=sizeof(MessageHeader)){
        MessageHeader header;

        std::memcpy(&header,buf->peek(),sizeof(MessageHeader));
        
        if(ntohl(header.magic)!=PROTOCOL_MAGIC){
            LOG_ERROR("Protocol Magic Error, closing connection");
            conn->shutdown();
            break;
        }
        uint32_t len=ntohl(header.length);
        uint16_t type = ntohs(header.type);
        //如果长度不够一个数据包则等待下一次epollin触发
        if(buf->readableBytes()<len+sizeof(MessageHeader)){
            break;
        }
        buf->retrieve(sizeof(MessageHeader));
        std::string body=buf->retrieveAllAsString(len);
        
        send(conn,type,body);
    }

}
//序列化数据并发送
void TLVCodec::send(const TcpConnectionPtr& conn,uint16_t type,std::string msg){
    Buffer buf;

    MessageHeader header;
    header.magic=htonl(PROTOCOL_MAGIC);
    //header.version=PROTOCOL_VERSION;
    
    header.type=htons(type);
    header.length=htonl(static_cast<uint32_t>(msg.size()));

    buf.append(reinterpret_cast<const char*>(&header),sizeof(MessageHeader));
    buf.append(msg.data(),msg.size());
    std::string data=buf.retrieveAllAsString();
    conn->send(data);
}