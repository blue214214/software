#pragma once
#include <drogon/HttpController.h>

class CanteenCtrl : public drogon::HttpController<CanteenCtrl> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(CanteenCtrl::getAllCanteens,  "/api/v1/canteens",             drogon::Get, "JwtFilter");
    ADD_METHOD_TO(CanteenCtrl::getCanteenQueues,"/api/v1/canteens/{id}/queues", drogon::Get, "JwtFilter");
    ADD_METHOD_TO(CanteenCtrl::getCanteenSeats, "/api/v1/canteens/{id}/seats",  drogon::Get, "JwtFilter");
    METHOD_LIST_END

    void getAllCanteens(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void getCanteenQueues(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                          long long id);
    void getCanteenSeats(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                         long long id);
};
