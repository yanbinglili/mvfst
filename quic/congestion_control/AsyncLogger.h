//
// Created by liyan on 2025-07-26.
//

#pragma once

// common/AsyncLogger.h

#include <folly/ProducerConsumerQueue.h>
#include <fstream>
#include <string>
#include <thread>
#include <atomic>

class AsyncLogger {
public:
    static AsyncLogger& getInstance(const std::string& name = "default");

    AsyncLogger(const AsyncLogger&) = delete;
    void operator=(const AsyncLogger&) = delete;

    void log(std::string&& message);

    static void stopAll();

    void stop();

    ~AsyncLogger();

private:
    explicit AsyncLogger(const std::string& filePath);

    void logThreadLoop();

    std::ofstream ofs_;

    folly::ProducerConsumerQueue<std::string> queue_;

    std::atomic<bool> done_;
    std::thread logThread_;
};

