#include "SearchCtrl.h"
#include "utils/ApiResponse.h"
#include "utils/ErrorCode.h"
#include <drogon/drogon.h>
#include <json/json.h>

void SearchCtrl::search(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    const std::string keyword   = req->getParameter("q");
    const std::string canteenIdStr = req->getParameter("canteen_id");

    if (keyword.empty()) {
        cb(utils::ApiResponse::validationError({{"q","关键词不能为空"}}));
        return;
    }
    if (keyword.size() > 50) {
        cb(utils::ApiResponse::error(utils::ERR_KEYWORD_TOO_LONG.status,
                                     utils::ERR_KEYWORD_TOO_LONG.code,
                                     utils::ERR_KEYWORD_TOO_LONG.message));
        return;
    }

    std::string sql =
        "SELECT d.id, d.name, d.price, d.available_today, d.stock_count, "
        "  w.id AS window_id, w.name AS window_name, "
        "  c.id AS canteen_id, c.name AS canteen_name "
        "FROM dish d "
        "JOIN window w ON w.id = d.window_id "
        "JOIN canteen c ON c.id = w.canteen_id "
        "WHERE MATCH(d.name) AGAINST(? IN BOOLEAN MODE) ";

    if (!canteenIdStr.empty()) {
        sql += "AND c.id = " + canteenIdStr + " ";
    }
    sql += "LIMIT 50";

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        sql,
        [cb](const drogon::orm::Result& r) mutable {
            Json::Value arr(Json::arrayValue);
            for (auto& row : r) {
                Json::Value item;
                item["id"]            = (Json::Int64)row["id"].as<long long>();
                item["name"]          = row["name"].as<std::string>();
                item["price"]         = row["price"].as<double>();
                item["availableToday"]= row["available_today"].as<int>() == 1;
                item["stockCount"]    = row["stock_count"].as<int>();
                item["windowId"]      = (Json::Int64)row["window_id"].as<long long>();
                item["windowName"]    = row["window_name"].as<std::string>();
                item["canteenId"]     = (Json::Int64)row["canteen_id"].as<long long>();
                item["canteenName"]   = row["canteen_name"].as<std::string>();
                arr.append(item);
            }
            cb(utils::ApiResponse::ok(arr));
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << e.base().what();
            cb(utils::ApiResponse::error(utils::ERR_INTERNAL.status,
                                         utils::ERR_INTERNAL.code,
                                         utils::ERR_INTERNAL.message));
        },
        "+" + keyword + "*"
    );
}
