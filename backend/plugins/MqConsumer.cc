#include "MqConsumer.h"
#include "utils/RedisClient.h"
#include <drogon/drogon.h>
#include <json/json.h>
#include <trantor/utils/Logger.h>

void MqConsumer::initAndStart(const Json::Value&) {
    running_ = true;
    const auto& cfg = drogon::app().getCustomConfig();
    std::string host = cfg.get("rabbitmq_host", "127.0.0.1").asString();
    int port         = cfg.get("rabbitmq_port", 5672).asInt();
    std::string user = cfg.get("rabbitmq_user", "canteen").asString();
    std::string pass = cfg.get("rabbitmq_pass", "canteen123").asString();

    mqThread_ = std::thread([this, host, port, user, pass] {
        try {
            struct ev_loop* loop = ev_loop_new(EVFLAG_AUTO);
            AMQP::LibEvHandler handler(loop);

            std::string address = "amqp://" + user + ":" + pass + "@" + host + ":" + std::to_string(port) + "/";
            AMQP::TcpConnection connection(&handler, AMQP::Address(address));
            AMQP::TcpChannel channel(&connection);

            channel.onError([](const char* msg) {
                LOG_ERROR << "RabbitMQ channel error: " << msg;
            });

            // Consume queue.record
            channel.consume("queue.record")
                .onReceived([this, &channel](const AMQP::Message& msg, uint64_t tag, bool) {
                    std::string body(msg.body(), msg.bodySize());
                    try {
                        processQueueRecord(body);
                        channel.ack(tag);
                    } catch (const std::exception& e) {
                        LOG_ERROR << "queue.record processing error: " << e.what();
                        channel.reject(tag, false); // send to DLQ
                    }
                });

            // Consume seat.record
            channel.consume("seat.record")
                .onReceived([this, &channel](const AMQP::Message& msg, uint64_t tag, bool) {
                    std::string body(msg.body(), msg.bodySize());
                    try {
                        processSeatRecord(body);
                        channel.ack(tag);
                    } catch (const std::exception& e) {
                        LOG_ERROR << "seat.record processing error: " << e.what();
                        channel.reject(tag, false);
                    }
                });

            while (running_) {
                ev_run(loop, EVRUN_NOWAIT);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            ev_loop_destroy(loop);
        } catch (const std::exception& e) {
            LOG_ERROR << "MqConsumer fatal error: " << e.what();
        }
    });

    LOG_INFO << "MqConsumer started";
}

void MqConsumer::shutdown() {
    running_ = false;
    if (mqThread_.joinable()) mqThread_.join();
}

void MqConsumer::processQueueRecord(const std::string& body) {
    Json::Value msg;
    Json::Reader reader;
    if (!reader.parse(body, msg)) {
        LOG_ERROR << "Invalid JSON in queue.record: " << body;
        throw std::runtime_error("invalid JSON");
    }

    // Validate required fields
    if (!msg.isMember("canteen_id") || !msg.isMember("window_id") ||
        !msg.isMember("collected_at") || !msg.isMember("queue_count")) {
        LOG_ERROR << "Missing fields in queue.record: " << body;
        throw std::runtime_error("missing fields");
    }

    long long canteenId = msg["canteen_id"].asInt64();
    long long windowId  = msg["window_id"].asInt64();
    std::string collectedAt = msg["collected_at"].asString();
    int queueCount      = msg["queue_count"].asInt();
    int waitMin         = msg.get("estimated_wait_minutes", 0).asInt();

    // Insert with dedup (unique index on canteen_id, window_id, collected_at)
    auto db = drogon::app().getDbClient();
    db->execSqlSync(
        "INSERT IGNORE INTO queue_record "
        "(window_id, canteen_id, queue_count, estimated_wait_minutes, collected_at) "
        "VALUES (?, ?, ?, ?, ?)",
        windowId, canteenId, queueCount, waitMin, collectedAt
    );

    // Update Redis cache
    const auto& cfg = drogon::app().getCustomConfig();
    try {
        utils::RedisClient redis(
            cfg.get("redis_host", "127.0.0.1").asString(),
            cfg.get("redis_port", 6379).asInt()
        );
        std::string key = "queue:canteen:" + std::to_string(canteenId);
        redis.hset(key, std::to_string(windowId), body);
        redis.expire(key, 60);
    } catch (const std::exception& e) {
        LOG_WARN << "Redis update failed for queue record: " << e.what();
    }
}

void MqConsumer::processSeatRecord(const std::string& body) {
    Json::Value msg;
    Json::Reader reader;
    if (!reader.parse(body, msg)) {
        LOG_ERROR << "Invalid JSON in seat.record: " << body;
        throw std::runtime_error("invalid JSON");
    }

    if (!msg.isMember("canteen_id") || !msg.isMember("collected_at")) {
        LOG_ERROR << "Missing fields in seat.record: " << body;
        throw std::runtime_error("missing fields");
    }

    long long canteenId = msg["canteen_id"].asInt64();

    // Update Redis seat cache
    const auto& cfg = drogon::app().getCustomConfig();
    try {
        utils::RedisClient redis(
            cfg.get("redis_host", "127.0.0.1").asString(),
            cfg.get("redis_port", 6379).asInt()
        );
        std::string key = "seat:canteen:" + std::to_string(canteenId);
        redis.setex(key, body, 120);
    } catch (const std::exception& e) {
        LOG_WARN << "Redis update failed for seat record: " << e.what();
    }
}
