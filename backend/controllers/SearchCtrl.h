#pragma once
#include <drogon/HttpController.h>

class SearchCtrl : public drogon::HttpController<SearchCtrl> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(SearchCtrl::search, "/api/v1/dishes/search", drogon::Get, "JwtFilter");
    METHOD_LIST_END

    void search(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};
