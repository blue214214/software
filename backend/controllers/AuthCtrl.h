#pragma once
#include <drogon/HttpController.h>

class AuthCtrl : public drogon::HttpController<AuthCtrl> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AuthCtrl::login,   "/api/v1/auth/login",   drogon::Post);
    ADD_METHOD_TO(AuthCtrl::refresh, "/api/v1/auth/refresh", drogon::Post);
    ADD_METHOD_TO(AuthCtrl::logout,  "/api/v1/auth/logout",  drogon::Post);
    METHOD_LIST_END

    void login(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void refresh(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void logout(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};
