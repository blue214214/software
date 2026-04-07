#include "RecommendCtrl.h"
#include "utils/ApiResponse.h"
#include "utils/ErrorCode.h"
#include "utils/CrowdLevel.h"
#include <drogon/drogon.h>
#include <json/json.h>

void RecommendCtrl::recommend(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    long long userId = std::stoll(req->getAttributes()->get<std::string>("userId"));
    const auto& cfg  = drogon::app().getCustomConfig();
    int maxResults   = cfg.get("recommendation_max_results", 10).asInt();
    int penaltyThreshold = cfg.get("queue_penalty_threshold", 15).asInt();

    auto db = drogon::app().getDbClient();

    // Check if user has history
    db->execSqlAsync(
        "SELECT COUNT(*) AS cnt FROM reservation WHERE user_id = ?",
        [=, cb = std::move(cb)](const drogon::orm::Result& r) mutable {
            int historyCount = r[0]["cnt"].as<int>();

            std::string sql;
            if (historyCount > 0) {
                // Personal history: rank by user's order frequency, penalize high-queue windows
                sql =
                    "SELECT d.id, d.name, d.price, d.available_today, d.stock_count, "
                    "  w.id AS window_id, w.name AS window_name, "
                    "  c.id AS canteen_id, c.name AS canteen_name, "
                    "  COUNT(r.id) AS freq, "
                    "  COALESCE(qr.queue_count, 0) AS queue_count "
                    "FROM dish d "
                    "JOIN window w ON w.id = d.window_id "
                    "JOIN canteen c ON c.id = w.canteen_id "
                    "LEFT JOIN reservation r ON r.dish_id = d.id AND r.user_id = " + std::to_string(userId) + " "
                    "LEFT JOIN ("
                    "  SELECT window_id, queue_count FROM queue_record "
                    "  WHERE (window_id, collected_at) IN ("
                    "    SELECT window_id, MAX(collected_at) FROM queue_record GROUP BY window_id"
                    "  )"
                    ") qr ON qr.window_id = w.id "
                    "WHERE d.available_today = 1 AND d.stock_count > 0 "
                    "GROUP BY d.id "
                    "ORDER BY "
                    "  CASE WHEN COALESCE(qr.queue_count,0) > " + std::to_string(penaltyThreshold) + " THEN 0 ELSE 1 END DESC, "
                    "  freq DESC "
                    "LIMIT " + std::to_string(maxResults);
            } else {
                // New user: global hot dishes
                sql =
                    "SELECT d.id, d.name, d.price, d.available_today, d.stock_count, "
                    "  w.id AS window_id, w.name AS window_name, "
                    "  c.id AS canteen_id, c.name AS canteen_name, "
                    "  COUNT(r.id) AS freq, "
                    "  COALESCE(qr.queue_count, 0) AS queue_count "
                    "FROM dish d "
                    "JOIN window w ON w.id = d.window_id "
                    "JOIN canteen c ON c.id = w.canteen_id "
                    "LEFT JOIN reservation r ON r.dish_id = d.id "
                    "LEFT JOIN ("
                    "  SELECT window_id, queue_count FROM queue_record "
                    "  WHERE (window_id, collected_at) IN ("
                    "    SELECT window_id, MAX(collected_at) FROM queue_record GROUP BY window_id"
                    "  )"
                    ") qr ON qr.window_id = w.id "
                    "WHERE d.available_today = 1 AND d.stock_count > 0 "
                    "GROUP BY d.id "
                    "ORDER BY "
                    "  CASE WHEN COALESCE(qr.queue_count,0) > " + std::to_string(penaltyThreshold) + " THEN 0 ELSE 1 END DESC, "
                    "  freq DESC "
                    "LIMIT " + std::to_string(maxResults);
            }

            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(
                sql,
                [cb](const drogon::orm::Result& r2) mutable {
                    Json::Value arr(Json::arrayValue);
                    for (auto& row : r2) {
                        Json::Value item;
                        item["id"]          = (Json::Int64)row["id"].as<long long>();
                        item["name"]        = row["name"].as<std::string>();
                        item["price"]       = row["price"].as<double>();
                        item["stockCount"]  = row["stock_count"].as<int>();
                        item["soldOut"]     = row["stock_count"].as<int>() == 0;
                        item["windowId"]    = (Json::Int64)row["window_id"].as<long long>();
                        item["windowName"]  = row["window_name"].as<std::string>();
                        item["canteenId"]   = (Json::Int64)row["canteen_id"].as<long long>();
                        item["canteenName"] = row["canteen_name"].as<std::string>();
                        item["queueCount"]  = row["queue_count"].as<int>();
                        arr.append(item);
                    }
                    cb(utils::ApiResponse::ok(arr));
                },
                [cb](const drogon::orm::DrogonDbException& e) mutable {
                    LOG_ERROR << e.base().what();
                    cb(utils::ApiResponse::error(utils::ERR_INTERNAL.status,
                                                 utils::ERR_INTERNAL.code,
                                                 utils::ERR_INTERNAL.message));
                }
            );
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
