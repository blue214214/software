#pragma once
#include <drogon/plugins/Plugin.h>
#include <amqpcpp.h>
#include <amqpcpp/libev.h>
#include <thread>
#include <atomic>

// RabbitMQ consumer plugin: consumes QUEUE_RECORD and SEAT_RECORD messages
class MqConsumer : public drogon::Plugin<MqConsumer> {
public:
    void initAndStart(const Json::Value& config) override;
    void shutdown() override;
private:
    std::thread mqThread_;
    std::atomic<bool> running_{false};
    void processQueueRecord(const std::string& body);
    void processSeatRecord(const std::string& body);
};
