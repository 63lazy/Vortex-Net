#include "socket.h"
#include "InetAddress.h"
#include "logger.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
Socket::~Socket(){
    close(sockfd_);
}
void Socket::bindAddress(const InetAddress &loacaladdr){
    if(0!=::bind(sockfd_,(sockaddr*)loacaladdr.getSockAddrInet(),sizeof(sockaddr_in))){
        LOG_FATAL("bind socket:%d fail",sockfd_); 
    }
}
void Socket::listen(){
    if(0!=::listen(sockfd_,1024))
    {
        LOG_FATAL("listen socket:%d fail",sockfd_); 
    }
}
int Socket::accept(InetAddress *peeraddr){
    sockaddr_in addr;
    socklen_t len=sizeof(sockaddr_in);
    bzero(&addr,sizeof(sockaddr_in));
    int connfd = ::accept4(sockfd_,(sockaddr*)&addr,&len,SOCK_NONBLOCK |SOCK_CLOEXEC);
    if(connfd>=0){
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

int Socket::connect(int sockfd,const InetAddress* serverAddr){
    return ::connect(sockfd,(sockaddr*)serverAddr->getSockAddrInet(),static_cast<socklen_t>(sizeof(sockaddr_in)));
}

//TcpConnection::shutdownInLoop触发
void Socket::shutdownWrite(){
    //向对端发送FIN包
    if(::shutdown(sockfd_,SHUT_WR)<0)
    {
        LOG_ERROR("shutdownwrite error");
    }
}

void Socket::setTcpNonDelay(bool on){
    int optval = on ?1:0;
    ::setsockopt(sockfd_,IPPROTO_TCP,TCP_NODELAY,&optval,sizeof optval);
}
void Socket::setReuseAddr(bool on){
    int optval = on ?1:0;
    ::setsockopt(sockfd_,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof optval);
}
void Socket::setReusePort(bool on){
    int optval = on ?1:0;
    ::setsockopt(sockfd_,SOL_SOCKET,SO_REUSEPORT,&optval,sizeof optval);
}
void Socket::setReuseAlive(bool on){
    int optval = on ?1:0;
    ::setsockopt(sockfd_,SOL_SOCKET,SO_KEEPALIVE,&optval,sizeof optval);
}

void Socket::setKeepAlive(bool on)
{
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE,
               &optval, static_cast<socklen_t>(sizeof optval));
}


int Socket::getSocketError(int sockfd)
{
  int optval;
  socklen_t optlen = static_cast<socklen_t>(sizeof optval);

  if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
  {
    return errno;
  }
  else
  {
    return optval;
  }
}
