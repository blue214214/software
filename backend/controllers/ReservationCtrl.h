#pragma once
#include <drogon/HttpController.h>

class ReservationCtrl : public drogon::HttpController<ReservationCtrl> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ReservationCtrl::create,        "/api/v1/reservations",              drogon::Post,   "JwtFilter");
    ADD_METHOD_TO(ReservationCtrl::myList,         "/api/v1/reservations/my",           drogon::Get,    "JwtFilter");
    ADD_METHOD_TO(ReservationCtrl::cancel,         "/api/v1/reservations/{id}",         drogon::Delete, "JwtFilter");
    ADD_METHOD_TO(ReservationCtrl::confirmPickup,  "/api/v1/reservations/{id}/pickup",  drogon::Post,   "JwtFilter");
    ADD_METHOD_TO(ReservationCtrl::confirmLeave,   "/api/v1/reservations/{id}/leave",   drogon::Post,   "JwtFilter");
    METHOD_LIST_END

    void create(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void myList(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void cancel(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                long long id);
    void confirmPickup(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                       long long id);
    void confirmLeave(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                      long long id);
};
