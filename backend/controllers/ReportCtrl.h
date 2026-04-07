#pragma once
#include <drogon/HttpController.h>

class ReportCtrl : public drogon::HttpController<ReportCtrl> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ReportCtrl::list,   "/api/v1/reports/weekly",      drogon::Get, "JwtFilter");
    ADD_METHOD_TO(ReportCtrl::detail, "/api/v1/reports/weekly/{id}", drogon::Get, "JwtFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void detail(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                long long id);
};
