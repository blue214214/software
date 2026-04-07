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
    const std::string secret = cfg.get("jwt_secret", "").asString();
    const int accessExp  = cfg.get("access_token_expiry_seconds",  1800).asInt();
    const int refreshExp = cfg.get("refresh_token_expiry_seconds", 604800).asInt();

    utils::JwtUtil jwtUtil(secret, accessExp, refreshExp);

    try {
        auto decoded = jwtUtil.parse(token);

        const std::string type = decoded.get_payload_claim("type").as_string();
        if (type != "access") {
            fcb(utils::ApiResponse::error(utils::ERR_TOKEN_INVALID.status,
                                          utils::ERR_TOKEN_INVALID.code,
                                          utils::ERR_TOKEN_INVALID.message));
            return;
        }

        const std::string userId = decoded.get_payload_claim("userId").as_string();
        const std::string role   = decoded.get_payload_claim("role").as_string();

        req->getAttributes()->insert("userId", userId);
        req->getAttributes()->insert("role",   role);

        fccb();
    } catch (const jwt::error::token_expired_exception&) {
        fcb(utils::ApiResponse::error(utils::ERR_TOKEN_EXPIRED.status,
                                      utils::ERR_TOKEN_EXPIRED.code,
                                      utils::ERR_TOKEN_EXPIRED.message));
    } catch (const std::exception&) {
        fcb(utils::ApiResponse::error(utils::ERR_TOKEN_INVALID.status,
                                      utils::ERR_TOKEN_INVALID.code,
                                      utils::ERR_TOKEN_INVALID.message));
    }
}
