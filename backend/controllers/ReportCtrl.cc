#include "ReportCtrl.h"
#include "utils/ApiResponse.h"
#include "utils/ErrorCode.h"
#include <drogon/drogon.h>
#include <json/json.h>

// ── GET /api/v1/reports/weekly ────────────────────────────────────────────────
void ReportCtrl::list(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    long long userId = std::stoll(req->getAttributes()->get<std::string>("userId"));
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT id, week_start, week_end, meal_count, dish_count, "
        "  total_amount, generated_at "
        "FROM weekly_report WHERE user_id = ? ORDER BY week_start DESC",
        [cb](const drogon::orm::Result& r) mutable {
            Json::Value arr(Json::arrayValue);
            for (auto& row : r) {
                Json::Value item;
                item["id"]          = (Json::Int64)row["id"].as<long long>();
                item["weekStart"]   = row["week_start"].as<std::string>();
                item["weekEnd"]     = row["week_end"].as<std::string>();
                item["mealCount"]   = row["meal_count"].as<int>();
                item["dishCount"]   = row["dish_count"].as<int>();
                item["totalAmount"] = row["total_amount"].as<double>();
                item["generatedAt"] = row["generated_at"].as<std::string>();
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
        userId
    );
}

// ── GET /api/v1/reports/weekly/{id} ──────────────────────────────────────────
void ReportCtrl::detail(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                         long long id) {
    long long userId = std::stoll(req->getAttributes()->get<std::string>("userId"));
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT * FROM weekly_report WHERE id = ? AND user_id = ?",
        [cb](const drogon::orm::Result& r) mutable {
            if (r.empty()) {
                cb(utils::ApiResponse::error(utils::ERR_NOT_FOUND.status,
                                             utils::ERR_NOT_FOUND.code,
                                             utils::ERR_NOT_FOUND.message));
                return;
            }
            auto row = r[0];
            Json::Value data;
            data["id"]                  = (Json::Int64)row["id"].as<long long>();
            data["weekStart"]           = row["week_start"].as<std::string>();
            data["weekEnd"]             = row["week_end"].as<std::string>();
            data["mealCount"]           = row["meal_count"].as<int>();
            data["dishCount"]           = row["dish_count"].as<int>();
            data["totalAmount"]         = row["total_amount"].as<double>();
            data["generatedAt"]         = row["generated_at"].as<std::string>();

            // Parse JSON fields for chart data
            Json::Reader reader;
            Json::Value topDishes, dailyCounts, canteenDist;
            if (!row["top_dishes"].isNull()) {
                reader.parse(row["top_dishes"].as<std::string>(), topDishes);
                data["topDishes"] = topDishes;
            }
            if (!row["daily_meal_counts"].isNull()) {
                reader.parse(row["daily_meal_counts"].as<std::string>(), dailyCounts);
                data["dailyMealCounts"] = dailyCounts;
            }
            if (!row["canteen_distribution"].isNull()) {
                reader.parse(row["canteen_distribution"].as<std::string>(), canteenDist);
                data["canteenDistribution"] = canteenDist;
            }
            cb(utils::ApiResponse::ok(data));
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << e.base().what();
            cb(utils::ApiResponse::error(utils::ERR_INTERNAL.status,
                                         utils::ERR_INTERNAL.code,
                                         utils::ERR_INTERNAL.message));
        },
        id, userId
    );
}
