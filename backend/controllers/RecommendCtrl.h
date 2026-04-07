#pragma once
#include <drogon/HttpController.h>

class RecommendCtrl : public drogon::HttpController<RecommendCtrl> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(RecommendCtrl::recommend, "/api/v1/recommendations", drogon::Get, "JwtFilter");
    METHOD_LIST_END

    void recommend(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};
