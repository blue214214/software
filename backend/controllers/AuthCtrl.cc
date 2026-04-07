#include "AuthCtrl.h"
#include "utils/ApiResponse.h"
#include "utils/ErrorCode.h"
#include "utils/JwtUtil.h"
#include "utils/PasswordUtil.h"
#include "utils/RedisClient.h"
#include <drogon/drogon.h>
#include <json/json.h>

static utils::JwtUtil makeJwt() {
    const auto& cfg = drogon::app().getCustomConfig();
    return utils::JwtUtil(
        cfg.get("jwt_secret", "secret").asString(),
        cfg.get("access_token_expiry_seconds",  1800).asInt(),
        cfg.get("refresh_token_expiry_seconds", 604800).asInt()
    );
}

static utils::RedisClient makeRedis() {
    const auto& cfg = drogon::app().getCustomConfig();
    return utils::RedisClient(
        cfg.get("redis_host", "127.0.0.1").asString(),
        cfg.get("redis_port", 6379).asInt()
    );
}

// ── POST /api/v1/auth/login ───────────────────────────────────────────────────
void AuthCtrl::login(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto body = req->getJsonObject();
    if (!body || !body->isMember("studentId") || !body->isMember("password")) {
        cb(utils::ApiResponse::validationError({{"studentId","必填"}, {"password","必填"}}));
        return;
    }

    const std::string studentId = (*body)["studentId"].asString();
    const std::string password  = (*body)["password"].asString();

    // Check account lock via Redis
    try {
        auto redis = makeRedis();
        auto failVal = redis.get("login:fail:" + studentId);
        if (failVal && std::stoi(*failVal) >= 5) {
            cb(utils::ApiResponse::error(utils::ERR_ACCOUNT_LOCKED.status,
                                         utils::ERR_ACCOUNT_LOCKED.code,
                                         utils::ERR_ACCOUNT_LOCKED.message));
            return;
        }
    } catch (...) {}

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT id, password_hash, role FROM user WHERE student_id = ? AND status = 1",
        [=, cb = std::move(cb)](const drogon::orm::Result& r) mutable {
            if (r.empty() || !utils::PasswordUtil::verify(password, r[0]["password_hash"].as<std::string>())) {
                try {
                    auto redis = makeRedis();
                    std::string lockKey = "login:fail:" + studentId;
                    long long cnt = redis.incr(lockKey);
                    if (cnt == 1) redis.expire(lockKey, 900);
                } catch (...) {}
                cb(utils::ApiResponse::error(utils::ERR_INVALID_CRED.status,
                                             utils::ERR_INVALID_CRED.code,
                                             utils::ERR_INVALID_CRED.message));
                return;
            }

            long long userId = r[0]["id"].as<long long>();
            std::string role = r[0]["role"].as<std::string>();

            try { makeRedis().del("login:fail:" + studentId); } catch (...) {}

            auto jwt = makeJwt();
            Json::Value data;
            data["accessToken"]  = jwt.generateAccessToken(userId, studentId, role);
            data["refreshToken"] = jwt.generateRefreshToken(userId, studentId);
            data["tokenType"]    = "Bearer";
            cb(utils::ApiResponse::ok(data));
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << e.base().what();
            cb(utils::ApiResponse::error(utils::ERR_INTERNAL.status,
                                         utils::ERR_INTERNAL.code,
                                         utils::ERR_INTERNAL.message));
        },
        studentId
    );
}

// ── POST /api/v1/auth/refresh ─────────────────────────────────────────────────
void AuthCtrl::refresh(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto body = req->getJsonObject();
    if (!body || !body->isMember("refreshToken")) {
        cb(utils::ApiResponse::validationError({{"refreshToken","必填"}}));
        return;
    }

    const std::string refreshToken = (*body)["refreshToken"].asString();
    auto jwt = makeJwt();

    Json::Value payload;
    try {
        payload = jwt.parse(refreshToken);
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        auto& err = (msg == "token expired") ? utils::ERR_TOKEN_EXPIRED : utils::ERR_TOKEN_INVALID;
        cb(utils::ApiResponse::error(err.status, err.code, err.message));
        return;
    }

    if (payload.get("type", "").asString() != "refresh") {
        cb(utils::ApiResponse::error(utils::ERR_TOKEN_INVALID.status,
                                     utils::ERR_TOKEN_INVALID.code,
                                     utils::ERR_TOKEN_INVALID.message));
        return;
    }

    std::string jti      = payload["jti"].asString();
    std::string studentId = payload["sub"].asString();
    long long userId     = std::stoll(payload["userId"].asString());
    long long remaining  = jwt.getRemainingSeconds(refreshToken);

    try {
        auto redis = makeRedis();
        if (redis.exists("auth:blacklist:" + jti)) {
            cb(utils::ApiResponse::error(utils::ERR_REFRESH_USED.status,
                                         utils::ERR_REFRESH_USED.code,
                                         utils::ERR_REFRESH_USED.message));
            return;
        }
        if (remaining > 0) redis.setex("auth:blacklist:" + jti, "1", (int)remaining);
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis error during refresh: " << e.what();
        cb(utils::ApiResponse::error(utils::ERR_INTERNAL.status,
                                     utils::ERR_INTERNAL.code,
                                     utils::ERR_INTERNAL.message));
        return;
    }

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT role FROM user WHERE id = ? AND status = 1",
        [=, cb = std::move(cb)](const drogon::orm::Result& r) mutable {
            if (r.empty()) {
                cb(utils::ApiResponse::error(utils::ERR_TOKEN_INVALID.status,
                                             utils::ERR_TOKEN_INVALID.code,
                                             utils::ERR_TOKEN_INVALID.message));
                return;
            }
            auto jwt2 = makeJwt();
            Json::Value data;
            data["accessToken"]  = jwt2.generateAccessToken(userId, studentId, r[0]["role"].as<std::string>());
            data["refreshToken"] = jwt2.generateRefreshToken(userId, studentId);
            data["tokenType"]    = "Bearer";
            cb(utils::ApiResponse::ok(data));
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << e.base().what();
            cb(utils::ApiResponse::error(utils::ERR_INTERNAL.status,
                                         utils::ERR_INTERNAL.code,
                                         utils::ERR_INTERNAL.message));
        },
        userId
    );
}

// ── POST /api/v1/auth/logout ──────────────────────────────────────────────────
void AuthCtrl::logout(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto body = req->getJsonObject();
    if (!body || !body->isMember("refreshToken")) {
        cb(utils::ApiResponse::validationError({{"refreshToken","必填"}}));
        return;
    }

    const std::string refreshToken = (*body)["refreshToken"].asString();
    auto jwt = makeJwt();

    try {
        auto payload = jwt.parseUnverified(refreshToken);
        std::string jti = payload["jti"].asString();
        long long remaining = jwt.getRemainingSeconds(refreshToken);
        auto redis = makeRedis();
        if (remaining > 0) redis.setex("auth:blacklist:" + jti, "1", (int)remaining);
    } catch (...) {}

    cb(utils::ApiResponse::ok());
}
