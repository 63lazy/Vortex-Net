#include "EPollPoller.h"
#include "logger.h"
#include "Channel.h"
#include <errno.h>
#include <unistd.h>
#include <strings.h>
#include <cstring>
//表示Channel是否添加到poller中
const int kNew=-1;     //对应初始channel中的成员变量index_
const int kAdded=0;
const int kDeleted=1;

EPollPoller::EPollPoller(EventLoop* loop)
    :Poller(loop)
    ,epollfd_(epoll_create1(EPOLL_CLOEXEC))//EPOLL_CLOEXEC防止fd泄露给子进程//
    ,events_(kInitEventListSize)
{
    if(epollfd_<0){
        LOG_FATAL("epoll_create1 error");
    }
};

EPollPoller::~EPollPoller(){
    ::close(epollfd_);
}
//eventloop通过poll获取需要处理事件的channel列表//
Timestamp EPollPoller::poll(int timeoutMs,ChannelList* activeChannels){
    //LOG_DEBUG比较合适//
    LOG_INFO("fun=%s:fd total count=%d",__FUNCTION__,(int)channels_.size());
    int numEvents=epoll_wait(epollfd_,&*events_.begin(),static_cast<int>(events_.size()),timeoutMs);
    int saveError = errno;
    Timestamp now(Timestamp::now());
    if(numEvents>0){
        LOG_INFO("%d events happened",numEvents);
        fillActiveChannels(numEvents,activeChannels);
        //扩容//
        if(numEvents==static_cast<int>(events_.size())){
            events_.resize(events_.size()*2);
        }
    }
    else if(numEvents==0){
        LOG_INFO("%s timeout ",__FUNCTION__);
    }
    else{
        if(saveError!= EINTR){
            errno=saveError;
            LOG_ERROR("epoll_wait error");
        }
    }
    return now;
}
void EPollPoller::updateChannel(Channel* channel){
    const int index=channel->index();
    LOG_INFO("fun=%s:fd=%d events=%d index=%d",__FUNCTION__,channel->fd(),channel->events(),index);
    if(index==kNew||index==kDeleted){
        if(index==kNew){
            int fd=channel->fd();
            channels_[fd]=channel;
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD,channel);
    }
    //如果channel已经添加到poller中，但是事件发生了变化，需要更新
    else{
        //如果channel的事件为空，说明它暂时没有事件发生，需要从监控中删除
        if(channel->IsNoneEvent()){
            update(EPOLL_CTL_DEL,channel);
            channel->set_index(kDeleted);
        }
        else{
            update(EPOLL_CTL_MOD,channel);
        }
    }
}
void EPollPoller::removeChannel(Channel* channel){
    int fd=channel->fd();
    LOG_INFO("fun=%s:fd=%d",__FUNCTION__,fd); 
    channels_.erase(fd);

    int index=channel->index();
    if(index==kAdded){
        update(EPOLL_CTL_DEL,channel);
    }
    channel->set_index(kNew);

}
//填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents,ChannelList* activeChannels) const
{
    for(int i=0;i<numEvents;i++){
        //这里的events_是epoll_wait返回的事件vector 不要与channel中的events_混淆了//
        Channel* channel=static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
        //Eventloop拿到需要处理的事件列表//
    }
}
//更新Channel
void EPollPoller::update(int operation,Channel* channel){
    epoll_event ev;
    bzero(&ev,sizeof(ev));
    ev.events=channel->events();
    ev.data.ptr=channel;

    int fd=channel->fd();
    if(::epoll_ctl(epollfd_,operation,fd,&ev)<0){
        if(operation==EPOLL_CTL_DEL){
            LOG_ERROR("epoll_ctl del error:%s",strerror(errno));
        }
        else{
            LOG_FATAL("epoll_ctl add/mod error:%s",strerror(errno));
        }
    }
}