#pragma once
#include <drogon/HttpFilter.h>
#include "utils/JwtUtil.h"
#include "utils/ApiResponse.h"
#include "utils/ErrorCode.h"

class JwtFilter : public drogon::HttpFilter<JwtFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;
};
