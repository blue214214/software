#include "JwtFilter.h"
#include <drogon/drogon.h>

void JwtFilter::doFilter(const drogon::HttpRequestPtr& req,
                         drogon::FilterCallback&& fcb,
                         drogon::FilterChainCallback&& fccb) {
    const auto& authHeader = req->getHeader("Authorization");
    if (authHeader.empty() || authHeader.rfind("Bearer ", 0) != 0) {
        fcb(utils::ApiResponse::error(utils::ERR_TOKEN_INVALID.status,
                                      utils::ERR_TOKEN_INVALID.code,
                                      utils::ERR_TOKEN_INVALID.message));
        return;
    }

    const std::string token = authHeader.substr(7);
    const auto& cfg = drogon::app().getCustomConfig();
    utils::JwtUtil jwtUtil(
        cfg.get("jwt_secret", "").asString(),
        cfg.get("access_token_expiry_seconds",  1800).asInt(),
        cfg.get("refresh_token_expiry_seconds", 604800).asInt()
    );

    try {
        auto payload = jwtUtil.parse(token);

        if (payload.get("type", "").asString() != "access") {
            fcb(utils::ApiResponse::error(utils::ERR_TOKEN_INVALID.status,
                                          utils::ERR_TOKEN_INVALID.code,
                                          utils::ERR_TOKEN_INVALID.message));
            return;
        }

        req->getAttributes()->insert("userId", payload["userId"].asString());
        req->getAttributes()->insert("role",   payload.get("role", "USER").asString());
        fccb();

    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        if (msg == "token expired") {
            fcb(utils::ApiResponse::error(utils::ERR_TOKEN_EXPIRED.status,
                                          utils::ERR_TOKEN_EXPIRED.code,
                                          utils::ERR_TOKEN_EXPIRED.message));
        } else {
            fcb(utils::ApiResponse::error(utils::ERR_TOKEN_INVALID.status,
                                          utils::ERR_TOKEN_INVALID.code,
                                          utils::ERR_TOKEN_INVALID.message));
        }
    }
}
