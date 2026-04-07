#include "RateLimitFilter.h"
#include "utils/RedisUtil.h"
#include <drogon/drogon.h>

void RateLimitFilter::doFilter(const drogon::HttpRequestPtr& req,
                                drogon::FilterCallback&& fcb,
                                drogon::FilterChainCallback&& fccb) {
    const std::string ip  = req->getPeerAddr().toIp();
    const std::string key = "ratelimit:ip:" + ip;

    try {
        utils::RedisClient redis = utils::makeRedis();

        long long count = redis.incr(key);
        if (count == 1) redis.expire(key, 60); // 60s window

        if (count > 300) {
            fcb(utils::ApiResponse::error(utils::ERR_RATE_LIMIT.status,
                                          utils::ERR_RATE_LIMIT.code,
                                          utils::ERR_RATE_LIMIT.message));
            return;
        }
    } catch (const std::exception& e) {
        LOG_WARN << "RateLimit Redis error: " << e.what() << " — allowing request";
    }

    fccb();
}
