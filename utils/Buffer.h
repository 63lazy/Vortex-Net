#pragma once
#include <vector>
#include <string>
#include <algorithm>
class Buffer{
public:
    static const size_t kCheapPrepend_;
    static const size_t kInitiaSize_;
    explicit Buffer(size_t initialSize=kInitiaSize_)
        :buffer_(kCheapPrepend_+kInitiaSize_),
        readerIndex_(kCheapPrepend_),
        writerIndex_(kCheapPrepend_)
    {}
    size_t readableBytes() const
    {
        return writerIndex_-readerIndex_;
    }
    size_t writeableBytes() const
    {
        return buffer_.size()-writerIndex_;
    }
    size_t prependableBytes() const
    {
        return readerIndex_;
    }
    //返回缓冲区中可读区域的起始地址
    const char *peek()const{
        return begin()+readerIndex_;
    }  
    void retrieve(size_t len){
        if(len<readableBytes()){
            readerIndex_+=len;
        }
        else{   //len==readableBytes()
            retrieveAll();
        }

    }
    void retrieveAll(){
        readerIndex_=writerIndex_=kCheapPrepend_;
    }
    std::string retrieveAllAsString(){
        return retrieveAllAsString(readableBytes());
    }
    std::string retrieveAllAsString(size_t len){
        std::string result(peek(),len);
        retrieve(len);      //对上一句读出的缓冲区进行复位操作
        return result;
    }

    void ensureWriteableBytes(size_t len){
        if(writeableBytes()<len){
            makeSpace(len);
        }
    }
    //添加数据
    void append(const char *data,size_t len){
        ensureWriteableBytes(len);
        std::copy(data, data+len, beginwrite());
        writerIndex_+=len;
    }
    char *beginwrite(){
        return begin()+writerIndex_;
    }
    const char *beginwrite() const{
        return begin()+writerIndex_;
    }
    //从fd上读取数据
    ssize_t readFd(int fd, int *saveError);
private:
    char *begin(){
        return &*buffer_.begin();
    }
    const char *begin()const{
        return &*buffer_.begin();
    }   
    //扩容函数//
    void makeSpace(size_t len){
        if(writeableBytes()+prependableBytes()<len+kCheapPrepend_){
            buffer_.resize(writerIndex_+len);
        }
        else{
            size_t readable=readableBytes();
            std::copy(begin()+readerIndex_,
                      begin()+writerIndex_,
                      begin()+kCheapPrepend_);
            readerIndex_=kCheapPrepend_;
            writerIndex_=kCheapPrepend_+readable;
        }
    }
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};