//
// Created by liyan on 2025-07-26.
//
// common/AsyncLogger.cpp

#include "AsyncLogger.h"
#include <map>
#include <memory>
#include <mutex>


static std::map<std::string, std::unique_ptr<AsyncLogger>> loggers;
static std::mutex loggersMutex;

AsyncLogger& AsyncLogger::getInstance(const std::string& name) {
    std::lock_guard<std::mutex> guard(loggersMutex);

    auto it = loggers.find(name);
    if (it == loggers.end()) {
        std::string filePath = "/home/liyan/proxygen/log/" + name + ".log";
        it = loggers.emplace(name, std::unique_ptr<AsyncLogger>(new AsyncLogger(filePath))).first;
    }
    return *it->second;
}

AsyncLogger::AsyncLogger(const std::string& filePath) : queue_(1024 * 8), done_(false) {
    ofs_.open(filePath, std::ios::app);
    logThread_ = std::thread(&AsyncLogger::logThreadLoop, this);
}

AsyncLogger::~AsyncLogger() {
    stop();
}

// 事件循环线程调用此函数，将日志消息放入队列
void AsyncLogger::log(std::string&& message) {
    // write() 是非阻塞的，如果队列满了会返回false
    // 这里用一个简单的循环来确保消息一定能被写入，
    // 在实践中，如果队列大小合理，几乎不会发生循环。
    while (!queue_.write(std::move(message))) {
        // 可以在这里加一些处理，比如稍等片刻或记录队列已满的错误
    }
}


void AsyncLogger::stopAll() {
    std::lock_guard<std::mutex> guard(loggersMutex);
    for (auto& pair : loggers) {
        if (pair.second) {
            pair.second->stop();
        }
    }
    loggers.clear();
}

void AsyncLogger::stop() {
    bool alreadyDone = done_.exchange(true);
    if (alreadyDone) {
        return;
    }

    if (logThread_.joinable()) {
        logThread_.join();
    }

    if (ofs_.is_open()) {
        ofs_.close();
    }
}

void AsyncLogger::logThreadLoop() {
    std::string msg;
    // 只要没有收到停止信号，就一直循环
    while (!done_) {
        // read(msg) 会阻塞地等待队列中有新消息
        // 这种“阻塞”是高效的，线程会进入睡眠状态，不消耗CPU
        if (queue_.read(msg)) {
            ofs_ << msg << "\n";
        }
    }
    // 收到停止信号后，为了防止丢失消息，需要清空队列中剩余的内容
    while (queue_.read(msg)) {
        ofs_ << msg << "\n";
    }
    ofs_.flush();
}