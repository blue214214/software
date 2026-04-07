#pragma once
#include <hiredis/hiredis.h>
#include <string>
#include <optional>
#include <stdexcept>
#include <memory>

namespace utils {

// Simple synchronous Redis client wrapping hiredis
class RedisClient {
public:
    RedisClient(const std::string& host, int port) {
        ctx_ = redisConnect(host.c_str(), port);
        if (!ctx_ || ctx_->err) {
            std::string err = ctx_ ? ctx_->errstr : "connection failed";
            if (ctx_) redisFree(ctx_);
            ctx_ = nullptr;
            throw std::runtime_error("Redis connect error: " + err);
        }
    }

    ~RedisClient() {
        if (ctx_) redisFree(ctx_);
    }

    // SET key value EX seconds
    void setex(const std::string& key, const std::string& value, int ttlSeconds) {
        auto* r = static_cast<redisReply*>(
            redisCommand(ctx_, "SET %s %s EX %d", key.c_str(), value.c_str(), ttlSeconds));
        freeReplyObject(r);
    }

    // GET key → optional<string>
    std::optional<std::string> get(const std::string& key) {
        auto* r = static_cast<redisReply*>(redisCommand(ctx_, "GET %s", key.c_str()));
        std::optional<std::string> result;
        if (r && r->type == REDIS_REPLY_STRING) result = std::string(r->str, r->len);
        freeReplyObject(r);
        return result;
    }

    // EXISTS key
    bool exists(const std::string& key) {
        auto* r = static_cast<redisReply*>(redisCommand(ctx_, "EXISTS %s", key.c_str()));
        bool result = r && r->integer > 0;
        freeReplyObject(r);
        return result;
    }

    // DEL key
    void del(const std::string& key) {
        auto* r = static_cast<redisReply*>(redisCommand(ctx_, "DEL %s", key.c_str()));
        freeReplyObject(r);
    }

    // INCR key → new value
    long long incr(const std::string& key) {
        auto* r = static_cast<redisReply*>(redisCommand(ctx_, "INCR %s", key.c_str()));
        long long val = r ? r->integer : 0;
        freeReplyObject(r);
        return val;
    }

    // EXPIRE key seconds
    void expire(const std::string& key, int ttlSeconds) {
        auto* r = static_cast<redisReply*>(
            redisCommand(ctx_, "EXPIRE %s %d", key.c_str(), ttlSeconds));
        freeReplyObject(r);
    }

    // HSET key field value
    void hset(const std::string& key, const std::string& field, const std::string& value) {
        auto* r = static_cast<redisReply*>(
            redisCommand(ctx_, "HSET %s %s %s", key.c_str(), field.c_str(), value.c_str()));
        freeReplyObject(r);
    }

    // HGET key field
    std::optional<std::string> hget(const std::string& key, const std::string& field) {
        auto* r = static_cast<redisReply*>(
            redisCommand(ctx_, "HGET %s %s", key.c_str(), field.c_str()));
        std::optional<std::string> result;
        if (r && r->type == REDIS_REPLY_STRING) result = std::string(r->str, r->len);
        freeReplyObject(r);
        return result;
    }

private:
    redisContext* ctx_ = nullptr;
};

} // namespace utils
