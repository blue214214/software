#pragma once
#include <hiredis/hiredis.h>
#include <hiredis/hiredis_ssl.h>
#include <string>
#include <optional>
#include <stdexcept>

namespace utils {

class RedisClient {
public:
    // useTls=true for Upstash / cloud Redis, false for local
    RedisClient(const std::string& host, int port,
                const std::string& password = "",
                bool useTls = false) {
        if (useTls) {
            redisInitOpenSSL();
            redisSSLContextError sslErr;
            redisSSLContext* ssl = redisCreateSSLContext(
                nullptr, nullptr, nullptr, nullptr, nullptr, &sslErr);
            if (!ssl) throw std::runtime_error("Redis SSL context error");

            ctx_ = redisConnect(host.c_str(), port);
            if (!ctx_ || ctx_->err) {
                std::string err = ctx_ ? ctx_->errstr : "connection failed";
                if (ctx_) redisFree(ctx_);
                ctx_ = nullptr;
                redisFreeSSLContext(ssl);
                throw std::runtime_error("Redis connect error: " + err);
            }
            if (redisInitiateSSLWithContext(ctx_, ssl) != REDIS_OK) {
                redisFree(ctx_);
                ctx_ = nullptr;
                redisFreeSSLContext(ssl);
                throw std::runtime_error("Redis TLS handshake failed");
            }
            redisFreeSSLContext(ssl);
        } else {
            ctx_ = redisConnect(host.c_str(), port);
            if (!ctx_ || ctx_->err) {
                std::string err = ctx_ ? ctx_->errstr : "connection failed";
                if (ctx_) redisFree(ctx_);
                ctx_ = nullptr;
                throw std::runtime_error("Redis connect error: " + err);
            }
        }

        // AUTH if password provided
        if (!password.empty()) {
            auto* r = static_cast<redisReply*>(
                redisCommand(ctx_, "AUTH %s", password.c_str()));
            freeReplyObject(r);
        }
    }

    ~RedisClient() {
        if (ctx_) redisFree(ctx_);
    }

    void setex(const std::string& key, const std::string& value, int ttlSeconds) {
        auto* r = static_cast<redisReply*>(
            redisCommand(ctx_, "SET %s %s EX %d", key.c_str(), value.c_str(), ttlSeconds));
        freeReplyObject(r);
    }

    std::optional<std::string> get(const std::string& key) {
        auto* r = static_cast<redisReply*>(redisCommand(ctx_, "GET %s", key.c_str()));
        std::optional<std::string> result;
        if (r && r->type == REDIS_REPLY_STRING) result = std::string(r->str, r->len);
        freeReplyObject(r);
        return result;
    }

    bool exists(const std::string& key) {
        auto* r = static_cast<redisReply*>(redisCommand(ctx_, "EXISTS %s", key.c_str()));
        bool result = r && r->integer > 0;
        freeReplyObject(r);
        return result;
    }

    void del(const std::string& key) {
        auto* r = static_cast<redisReply*>(redisCommand(ctx_, "DEL %s", key.c_str()));
        freeReplyObject(r);
    }

    long long incr(const std::string& key) {
        auto* r = static_cast<redisReply*>(redisCommand(ctx_, "INCR %s", key.c_str()));
        long long val = r ? r->integer : 0;
        freeReplyObject(r);
        return val;
    }

    void expire(const std::string& key, int ttlSeconds) {
        auto* r = static_cast<redisReply*>(
            redisCommand(ctx_, "EXPIRE %s %d", key.c_str(), ttlSeconds));
        freeReplyObject(r);
    }

    void hset(const std::string& key, const std::string& field, const std::string& value) {
        auto* r = static_cast<redisReply*>(
            redisCommand(ctx_, "HSET %s %s %s", key.c_str(), field.c_str(), value.c_str()));
        freeReplyObject(r);
    }

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
