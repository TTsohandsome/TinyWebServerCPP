/*
 * @Author       : mark
 * @Date         : 2023-06-15
 * @copyleft Apache 2.0
 */ 
#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include <string>
#include <unordered_map>
#include <fstream>
#include <sys/stat.h>    // stat
#include <fcntl.h>       // open
#include <unistd.h>      // close
#include <dirent.h>      // opendir, readdir
#include <sys/types.h>
#include <errno.h>

#include "../buffer/buffer.h"
#include "../log/log.h"

class FileHandler {
public:
    // 单例模式
    static FileHandler* Instance();

    // 初始化上传目录
    void Init(const std::string& uploadDir);

    // 处理文件上传请求
    bool HandleUpload(const std::string& boundary, const std::string& body, std::string& filename);

    // 处理文件下载请求
    bool HandleDownload(const std::string& filename, Buffer& buff, char** filePtr, size_t& fileLen);

    // 获取上传目录中的文件列表
    std::string GetFileList();

private:
    FileHandler() = default;
    ~FileHandler() = default;

    // 解析multipart/form-data格式的请求体
    bool ParseMultipartFormData(const std::string& boundary, const std::string& body, std::string& filename, std::string& fileContent);

    // 保存上传的文件
    bool SaveUploadedFile(const std::string& filename, const std::string& content);

    // 获取文件MIME类型
    std::string GetFileMimeType(const std::string& filename);

    std::string uploadDir_;
    static FileHandler* instance_;

    // 文件类型映射表
    static const std::unordered_map<std::string, std::string> MIME_TYPE;
};

#endif // FILE_HANDLER_H