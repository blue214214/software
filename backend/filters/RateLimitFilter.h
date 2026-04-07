#pragma once
#include <drogon/HttpFilter.h>
#include "utils/ApiResponse.h"
#include "utils/ErrorCode.h"
#include "utils/RedisClient.h"

// IP rate limit: max 300 requests per 60-second sliding window
class RateLimitFilter : public drogon::HttpFilter<RateLimitFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;
};
