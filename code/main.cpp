/*
 * @Author       : mark
 * @Date         : 2020-06-18
 * @copyleft Apache 2.0
 */ 
#include <unistd.h>
#include <iostream>
#include "server/webserver.h"
#include "http/filehandler.h"

int main() {
    /* 守护进程 后台运行 */
    //daemon(1, 0); 
    
    const int port = 1316;
    std::cout << "========================================" << std::endl;
    std::cout << "WebServer 启动中..." << std::endl;
    std::cout << "访问地址: http://localhost:" << port << std::endl;
    std::cout << "按 Ctrl+C 停止服务器" << std::endl;
    std::cout << "========================================" << std::endl;

    // 初始化文件处理器，设置上传目录
    FileHandler::Instance()->Init("./uploads");
    
    WebServer server(
        port, 3, 60000, false,             /* 端口 ET模式 timeoutMs 优雅退出  */
        3306, "ct", "123456", "ctdb", /* Mysql配置 */
        12, 6, true, 1, 1024);             /* 连接池数量 线程池数量 日志开关 日志等级 日志异步队列容量 */
    server.Start();
}
