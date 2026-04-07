#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>

namespace utils {

// Unified API response builder
class ApiResponse {
public:
    static drogon::HttpResponsePtr ok(const Json::Value& data = Json::Value::null) {
        Json::Value body;
        body["success"] = true;
        if (!data.isNull()) body["data"] = data;
        body["timestamp"] = currentTimestamp();
        auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
        resp->setStatusCode(drogon::k200OK);
        return resp;
    }

    static drogon::HttpResponsePtr error(drogon::HttpStatusCode status,
                                          const std::string& code,
                                          const std::string& message) {
        Json::Value body;
        body["success"] = false;
        body["code"] = code;
        body["message"] = message;
        body["timestamp"] = currentTimestamp();
        auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
        resp->setStatusCode(status);
        return resp;
    }

    static drogon::HttpResponsePtr validationError(
            const std::vector<std::pair<std::string,std::string>>& fieldErrors) {
        Json::Value body;
        body["success"] = false;
        body["code"] = "VALIDATION_ERROR";
        body["message"] = "请求参数不满足约束";
        Json::Value details(Json::arrayValue);
        for (auto& [field, msg] : fieldErrors) {
            Json::Value fe;
            fe["field"] = field;
            fe["message"] = msg;
            details.append(fe);
        }
        body["details"] = details;
        body["timestamp"] = currentTimestamp();
        auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
        resp->setStatusCode(drogon::k400BadRequest);
        return resp;
    }

private:
    static std::string currentTimestamp() {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
        return std::string(buf);
    }
};

} // namespace utils
