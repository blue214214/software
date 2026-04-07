#pragma once
#include "RedisClient.h"
#include <drogon/drogon.h>

namespace utils {

inline RedisClient makeRedis() {
    const auto& cfg = drogon::app().getCustomConfig();
    return RedisClient(
        cfg.get("redis_host",     "127.0.0.1").asString(),
        cfg.get("redis_port",     6379).asInt(),
        cfg.get("redis_password", "").asString(),
        cfg.get("redis_tls",      false).asBool()
    );
}

} // namespace utils
