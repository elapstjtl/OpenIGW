#include "SouthboundService.hpp"
#include <iostream>
#include <string>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <thread>
#include <chrono>
#include <fcntl.h>

using namespace southbound;

// 全局服务实例，用于信号处理
std::unique_ptr<SouthboundService> g_service;
volatile sig_atomic_t g_reload_requested = 0;

// 终止信号处理
void term_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (g_service) g_service->stop();
}

// 配置重载信号处理
void hup_handler(int signal) {
    if (signal == SIGHUP) g_reload_requested = 1;
}

// 打印使用说明
void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n"
              << "Options:\n"
              << "  -c, --config FILE    指定配置文件路径 (默认: /etc/southbound/southbound.conf)\n"
              << "  -d, --daemon         以守护进程模式运行\n"
              << "  -v, --verbose        详细输出\n"
              << "  -h, --help           显示此帮助信息\n"
              << "  -V, --version        显示版本信息\n"
              << "\n"
              << "示例:\n"
              << "  " << program_name << " -c /etc/southbound/southbound.conf\n"
              << "  " << program_name << " -d -v\n"
              << std::endl;
}

// 打印版本信息
void print_version() {
    std::cout << "Southbound Service v1.0.0\n"
              << "Copyright (C) 2024 Southbound Framework\n"
              << "A plugin-based southbound communication service\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    // 默认配置
    std::string config_file = "/etc/southbound/southbound.conf";
    bool daemon_mode = false;
    bool verbose = false;
    
    // 命令行选项
    static struct option long_options[] = {
        {"config",   required_argument, 0, 'c'},
        {"daemon",   no_argument,       0, 'd'},
        {"verbose",  no_argument,       0, 'v'},
        {"help",     no_argument,       0, 'h'},
        {"version",  no_argument,       0, 'V'},
        {0, 0, 0, 0}
    };
    
    // 解析命令行参数
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "c:dvVh", long_options, &option_index)) != -1) {
        switch (c) {
            case 'c':
                config_file = optarg;
                break;
            case 'd':
                daemon_mode = true;
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'V':
                print_version();
                return 0;
            case '?':
                print_usage(argv[0]);
                return 1;
            default:
                abort();
        }
    }
    
    // 检查配置文件是否存在
    if (access(config_file.c_str(), R_OK) != 0) {
        std::cerr << "Error: Cannot read config file: " << config_file << std::endl;
        std::cerr << "Please create a configuration file or specify a different path with -c option." << std::endl;
        return 1;
    }
    
    // 设置信号处理
    signal(SIGINT, term_handler);
    signal(SIGTERM, term_handler);
    signal(SIGHUP, hup_handler);  // 用于重新加载配置
    
    // 创建服务实例
    g_service = std::make_unique<SouthboundService>();
    
    // 初始化服务
    std::cout << "Initializing Southbound Service..." << std::endl;
    if (!g_service->initialize(config_file)) {
        std::cerr << "Failed to initialize service" << std::endl;
        return 1;
    }
    
    // 守护进程模式
    if (daemon_mode) {
        std::cout << "Starting in daemon mode..." << std::endl;
        
        // 创建子进程
        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "Failed to fork daemon process" << std::endl;
            return 1;
        }
        
        // 父进程退出
        if (pid > 0) {
            std::cout << "Daemon started with PID: " << pid << std::endl;
            return 0;
        }
        
        // 子进程继续执行
        // 分离会话
        if (setsid() < 0) {
            std::cerr << "Failed to create new session" << std::endl;
            return 1;
        }
        
        // 改变工作目录
        chdir("/");
        
        // 关闭标准文件描述符
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        
        // 重定向到/dev/null
        open("/dev/null", O_RDONLY);
        open("/dev/null", O_WRONLY);
        open("/dev/null", O_WRONLY);
    }
    
    // 启动服务
    std::cout << "Starting Southbound Service..." << std::endl;
    if (!g_service->start()) {
        std::cerr << "Failed to start service" << std::endl;
        return 1;
    }
    
    std::cout << "Southbound Service is running..." << std::endl;
    
    // 主循环
    while (g_service->is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 处理SIGHUP信号（重新加载配置）
        if (g_reload_requested) {
            std::cout << "Reloading configuration..." << std::endl;
            if (g_service->is_running()) g_service->stop();
            g_service = std::make_unique<SouthboundService>();
            if (g_service->initialize(config_file) && g_service->start()) {
                std::cout << "Configuration reloaded successfully" << std::endl;
            } else {
                std::cerr << "Failed to reload configuration" << std::endl;
            }
            g_reload_requested = 0;
        }
    }
    
    std::cout << "Southbound Service stopped." << std::endl;
    return 0;
}

// SIGHUP信号处理函数
void sighup_handler(int) {}
