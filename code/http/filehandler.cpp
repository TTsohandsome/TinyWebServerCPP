/*
 * @Author       : mark
 * @Date         : 2023-06-15
 * @copyleft Apache 2.0
 */ 
#include "filehandler.h"
#include <sys/mman.h>

using namespace std;

FileHandler* FileHandler::instance_ = nullptr;

// 文件类型映射表，与httpresponse.cpp中的SUFFIX_TYPE保持一致
const unordered_map<string, string> FileHandler::MIME_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css" },
    { ".js",    "text/javascript" },
    { ".zip",   "application/zip" },
    { ".doc",   "application/msword" },
    { ".docx",  "application/vnd.openxmlformats-officedocument.wordprocessingml.document" },
    { ".xls",   "application/vnd.ms-excel" },
    { ".xlsx",  "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet" },
    { ".ppt",   "application/vnd.ms-powerpoint" },
    { ".pptx",  "application/vnd.openxmlformats-officedocument.presentationml.presentation" },
};

FileHandler* FileHandler::Instance() {
    if (instance_ == nullptr) {
        instance_ = new FileHandler();
    }
    return instance_;
}

void FileHandler::Init(const string& uploadDir) {
    uploadDir_ = uploadDir;
    // 确保上传目录存在
    if (access(uploadDir_.c_str(), F_OK) != 0) {
        // 目录不存在，创建目录
        if (mkdir(uploadDir_.c_str(), 0755) != 0) {
            LOG_ERROR("Failed to create upload directory: %s", uploadDir_.c_str());
        } else {
            LOG_INFO("Created upload directory: %s", uploadDir_.c_str());
        }
    }
}

bool FileHandler::HandleUpload(const string& boundary, const string& body, string& filename) {
    string fileContent;
    if (!ParseMultipartFormData(boundary, body, filename, fileContent)) {
        LOG_ERROR("Failed to parse multipart/form-data");
        return false;
    }
    
    if (filename.empty() || fileContent.empty()) {
        LOG_ERROR("Empty filename or file content");
        return false;
    }
    
    return SaveUploadedFile(filename, fileContent);
}

bool FileHandler::HandleDownload(const string& filename, Buffer& buff, char** filePtr, size_t& fileLen) {
    string filepath = uploadDir_ + "/" + filename;
    struct stat fileStat;
    
    // 检查文件是否存在
    if (stat(filepath.c_str(), &fileStat) < 0) {
        LOG_ERROR("File %s not found", filepath.c_str());
        return false;
    }
    
    // 检查是否是普通文件
    if (!S_ISREG(fileStat.st_mode)) {
        LOG_ERROR("%s is not a regular file", filepath.c_str());
        return false;
    }
    
    // 获取文件大小
    fileLen = fileStat.st_size;
    
    // 打开文件
    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd < 0) {
        LOG_ERROR("Failed to open file %s", filepath.c_str());
        return false;
    }
    
    // 将文件映射到内存
    void* mmapResult = mmap(0, fileLen, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    
    if (mmapResult == MAP_FAILED) {
        LOG_ERROR("mmap failed for file %s", filepath.c_str());
        return false;
    }
    
    *filePtr = static_cast<char*>(mmapResult);
    
    // 添加响应头
    buff.Append("Content-Type: " + GetFileMimeType(filename) + "\r\n");
    buff.Append("Content-Disposition: attachment; filename=\"" + filename + "\"\r\n");
    buff.Append("Content-Length: " + to_string(fileLen) + "\r\n\r\n");
    
    return true;
}

string FileHandler::GetFileList() {
    string fileList = "<html><head><title>File List</title>";
    fileList += "<style>body{font-family:Arial,sans-serif;margin:20px;} ";
    fileList += "h1{color:#333;} table{border-collapse:collapse;width:100%;} ";
    fileList += "th,td{padding:8px;text-align:left;border-bottom:1px solid #ddd;} ";
    fileList += "tr:hover{background-color:#f5f5f5;} ";
    fileList += ".upload-form{margin-top:20px;padding:15px;background-color:#f9f9f9;border-radius:5px;}";
    fileList += "</style></head><body>";
    fileList += "<h1>File List</h1>";
    fileList += "<table><tr><th>Filename</th><th>Action</th></tr>";
    
    DIR* dir = opendir(uploadDir_.c_str());
    if (dir == nullptr) {
        LOG_ERROR("Failed to open directory %s", uploadDir_.c_str());
        fileList += "<tr><td colspan=\"2\">Failed to open directory</td></tr>";
    } else {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            string name = entry->d_name;
            // 跳过 . 和 .. 目录
            if (name == "." || name == "..") continue;
            
            // 检查是否是普通文件
            string fullPath = uploadDir_ + "/" + name;
            struct stat statBuf;
            if (stat(fullPath.c_str(), &statBuf) == 0 && S_ISREG(statBuf.st_mode)) {
                fileList += "<tr><td>" + name + "</td>";
                fileList += "<td><a href=\"/download/" + name + "\">Download</a></td></tr>";
            }
        }
        closedir(dir);
    }
    
    fileList += "</table>";
    
    // 添加上传表单
    fileList += "<div class=\"upload-form\">";
    fileList += "<h2>Upload File</h2>";
    fileList += "<form action=\"/upload\" method=\"post\" enctype=\"multipart/form-data\">";
    fileList += "<input type=\"file\" name=\"file\" required><br><br>";
    fileList += "<input type=\"submit\" value=\"Upload\">";
    fileList += "</form></div>";
    
    fileList += "</body></html>";
    return fileList;
}

bool FileHandler::ParseMultipartFormData(const string& boundary, const string& body, string& filename, string& fileContent) {
    // 构建完整的分隔符
    string delimiter = "--" + boundary;
    string endDelimiter = delimiter + "--";
    
    // 查找第一个分隔符
    size_t pos = body.find(delimiter);
    if (pos == string::npos) {
        LOG_ERROR("Boundary not found in request body");
        return false;
    }
    
    // 查找第二个分隔符（文件内容的开始）
    pos = body.find(delimiter, pos + delimiter.length());
    if (pos == string::npos) {
        LOG_ERROR("Second boundary not found");
        return false;
    }
    
    // 查找Content-Disposition头
    size_t headerStart = pos + delimiter.length();
    size_t headerEnd = body.find("\r\n\r\n", headerStart);
    if (headerEnd == string::npos) {
        LOG_ERROR("Header end not found");
        return false;
    }
    
    // 解析Content-Disposition头，获取文件名
    string header = body.substr(headerStart, headerEnd - headerStart);
    size_t filenamePos = header.find("filename=\"");
    if (filenamePos == string::npos) {
        LOG_ERROR("Filename not found in header");
        return false;
    }
    
    filenamePos += 10; // "filename=\"" 的长度
    size_t filenameEnd = header.find('"', filenamePos);
    if (filenameEnd == string::npos) {
        LOG_ERROR("Filename end quote not found");
        return false;
    }
    
    filename = header.substr(filenamePos, filenameEnd - filenamePos);
    
    // 获取文件内容
    size_t contentStart = headerEnd + 4; // \r\n\r\n 的长度
    size_t contentEnd = body.find(delimiter, contentStart) - 2; // -2 for \r\n before delimiter
    
    if (contentEnd <= contentStart || contentEnd == string::npos - 2) {
        LOG_ERROR("Content boundaries invalid");
        return false;
    }
    
    fileContent = body.substr(contentStart, contentEnd - contentStart);
    return true;
}

bool FileHandler::SaveUploadedFile(const string& filename, const string& content) {
    string filepath = uploadDir_ + "/" + filename;
    ofstream file(filepath, ios::binary);
    
    if (!file.is_open()) {
        LOG_ERROR("Failed to open file for writing: %s", filepath.c_str());
        return false;
    }
    
    file.write(content.c_str(), content.length());
    file.close();
    
    if (file.fail()) {
        LOG_ERROR("Failed to write to file: %s", filepath.c_str());
        return false;
    }
    
    LOG_INFO("File saved successfully: %s", filepath.c_str());
    return true;
}

string FileHandler::GetFileMimeType(const string& filename) {
    string::size_type idx = filename.find_last_of('.');
    if (idx == string::npos) {
        return "application/octet-stream"; // 默认二进制流
    }
    
    string suffix = filename.substr(idx);
    if (MIME_TYPE.count(suffix) == 1) {
        return MIME_TYPE.find(suffix)->second;
    }
    
    return "application/octet-stream";
}