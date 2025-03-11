/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft Apache 2.0
 */ 
#include "httpconn.h"
using namespace std;

const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;
bool HttpConn::isET;

HttpConn::HttpConn() { 
    fd_ = -1;
    addr_ = { 0 };
    isClose_ = true;
};

HttpConn::~HttpConn() { 
    Close(); 
};

void HttpConn::init(int fd, const sockaddr_in& addr) {
    assert(fd > 0);
    userCount++;
    addr_ = addr;
    fd_ = fd;
    writeBuff_.RetrieveAll();
    readBuff_.RetrieveAll();
    isClose_ = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

void HttpConn::Close() {
    response_.UnmapFile();
    if(isClose_ == false){
        isClose_ = true; 
        userCount--;
        close(fd_);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
    }
}

int HttpConn::GetFd() const {
    return fd_;
};

struct sockaddr_in HttpConn::GetAddr() const {
    return addr_;
}

const char* HttpConn::GetIP() const {
    return inet_ntoa(addr_.sin_addr);
}

int HttpConn::GetPort() const {
    return addr_.sin_port;
}

ssize_t HttpConn::read(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = readBuff_.ReadFd(fd_, saveErrno);
        if (len <= 0) {
            break;
        }
    } while (isET);
    return len;
}

ssize_t HttpConn::write(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = writev(fd_, iov_, iovCnt_);
        if(len <= 0) {
            *saveErrno = errno;
            break;
        }
        if(iov_[0].iov_len + iov_[1].iov_len  == 0) { break; } /* 传输结束 */
        else if(static_cast<size_t>(len) > iov_[0].iov_len) {
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if(iov_[0].iov_len) {
                writeBuff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        else {
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len; 
            iov_[0].iov_len -= len; 
            writeBuff_.Retrieve(len);
        }
    } while(isET || ToWriteBytes() > 10240);
    return len;
}

bool HttpConn::process() {
    request_.Init();
    if(readBuff_.ReadableBytes() <= 0) {
        return false;
    }
    else if(request_.parse(readBuff_)) {
        LOG_DEBUG("%s", request_.path().c_str());
        
        // 处理文件上传请求
        if(request_.method() == "POST" && request_.path() == "/upload") {
            std::string boundary = request_.GetBoundary();
            std::string filename;
            
            if(!boundary.empty() && FileHandler::Instance()->HandleUpload(boundary, request_.GetBody(), filename)) {
                LOG_INFO("File uploaded successfully: %s", filename.c_str());
                response_.Init(srcDir, "/filelist", request_.IsKeepAlive(), 200);
            } else {
                LOG_ERROR("File upload failed");
                response_.Init(srcDir, "/error.html", request_.IsKeepAlive(), 400);
            }
        }
        // 处理文件下载请求
        else if(request_.path().find("/download/") == 0) {
            std::string filename = request_.path().substr(10); // 去掉 "/download/"
            char* filePtr = nullptr;
            size_t fileLen = 0;
            
            if(FileHandler::Instance()->HandleDownload(filename, writeBuff_, &filePtr, fileLen)) {
                LOG_INFO("File download request: %s", filename.c_str());
                response_.SetFile(filePtr, fileLen);
                response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
            } else {
                LOG_ERROR("File download failed: %s", filename.c_str());
                response_.Init(srcDir, "/error.html", request_.IsKeepAlive(), 404);
            }
        }
        // 处理文件列表请求
        else if(request_.path() == "/filelist") {
            std::string fileListHtml = FileHandler::Instance()->GetFileList();
            response_.SetContent(fileListHtml);
            response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
        }
        // 处理普通请求
        else {
            response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
        }
    } else {
        response_.Init(srcDir, request_.path(), false, 400);
    }

    response_.MakeResponse(writeBuff_);
    /* 响应头 */
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;

    /* 文件 */
    if(response_.FileLen() > 0  && response_.File()) {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }
    LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen() , iovCnt_, ToWriteBytes());
    return true;
}
