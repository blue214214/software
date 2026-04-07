#pragma once
#include <drogon/drogon.h>
#include <string>

namespace utils {

struct ErrorInfo {
    drogon::HttpStatusCode status;
    std::string code;
    std::string message;
};

// All error codes
inline const ErrorInfo ERR_VALIDATION       {drogon::k400BadRequest,          "VALIDATION_ERROR",           "请求参数不满足约束"};
inline const ErrorInfo ERR_KEYWORD_TOO_LONG {drogon::k400BadRequest,          "KEYWORD_TOO_LONG",           "搜索关键词超过50字符"};
inline const ErrorInfo ERR_INVALID_CRED     {drogon::k401Unauthorized,        "INVALID_CREDENTIALS",        "学号或密码错误"};
inline const ErrorInfo ERR_ACCOUNT_LOCKED   {drogon::k401Unauthorized,        "ACCOUNT_LOCKED",             "账户已被锁定，请15分钟后重试"};
inline const ErrorInfo ERR_TOKEN_EXPIRED    {drogon::k401Unauthorized,        "TOKEN_EXPIRED",              "访问令牌已过期"};
inline const ErrorInfo ERR_TOKEN_INVALID    {drogon::k401Unauthorized,        "TOKEN_INVALID",              "令牌签名无效或已被篡改"};
inline const ErrorInfo ERR_REFRESH_USED     {drogon::k401Unauthorized,        "REFRESH_TOKEN_USED",         "刷新令牌已被使用"};
inline const ErrorInfo ERR_PERMISSION       {drogon::k403Forbidden,           "PERMISSION_DENIED",          "角色权限不足"};
inline const ErrorInfo ERR_NOT_FOUND        {drogon::k404NotFound,            "RESOURCE_NOT_FOUND",         "请求的资源不存在"};
inline const ErrorInfo ERR_STOCK            {drogon::k409Conflict,            "STOCK_INSUFFICIENT",         "菜品库存不足"};
inline const ErrorInfo ERR_NO_SEAT          {drogon::k409Conflict,            "NO_AVAILABLE_SEAT",          "食堂无可用座位"};
inline const ErrorInfo ERR_RESERVATION_LIMIT{drogon::k409Conflict,            "RESERVATION_LIMIT_EXCEEDED", "超过单用户预订数量限制"};
inline const ErrorInfo ERR_SEAT_STATUS      {drogon::k409Conflict,            "INVALID_SEAT_STATUS",        "座位状态不允许当前操作"};
inline const ErrorInfo ERR_RATE_LIMIT       {drogon::k429TooManyRequests,     "RATE_LIMIT_EXCEEDED",        "请求频率超限"};
inline const ErrorInfo ERR_INTERNAL         {drogon::k500InternalServerError, "INTERNAL_ERROR",             "服务器内部错误"};

} // namespace utils
