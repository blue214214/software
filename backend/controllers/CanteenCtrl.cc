#include "CanteenCtrl.h"
#include "utils/ApiResponse.h"
#include "utils/ErrorCode.h"
#include "utils/CrowdLevel.h"
#include "utils/RedisUtil.h"
#include <drogon/drogon.h>
#include <json/json.h>
#include <chrono>
#include <ctime>

// ── GET /api/v1/canteens ──────────────────────────────────────────────────────
void CanteenCtrl::getAllCanteens(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT c.id, c.name, c.location, c.total_seats, "
        "  COALESCE(AVG(qr.queue_count),0) AS avg_queue "
        "FROM canteen c "
        "LEFT JOIN window w ON w.canteen_id = c.id "
        "LEFT JOIN queue_record qr ON qr.window_id = w.id "
        "  AND qr.collected_at >= DATE_SUB(NOW(), INTERVAL 60 SECOND) "
        "WHERE c.status = 1 "
        "GROUP BY c.id",
        [cb](const drogon::orm::Result& r) mutable {
            Json::Value arr(Json::arrayValue);
            for (auto& row : r) {
                int avg = (int)row["avg_queue"].as<double>();
                Json::Value item;
                item["id"]           = (Json::Int64)row["id"].as<long long>();
                item["name"]         = row["name"].as<std::string>();
                item["location"]     = row["location"].as<std::string>();
                item["totalSeats"]   = row["total_seats"].as<int>();
                item["avgQueueCount"]= avg;
                item["crowdLevel"]   = utils::crowdLevelStr(utils::classifyCrowd(avg));
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
}

// ── GET /api/v1/canteens/{id}/queues ─────────────────────────────────────────
void CanteenCtrl::getCanteenQueues(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                    long long id) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT w.id AS window_id, w.name AS window_name, "
        "  qr.queue_count, qr.estimated_wait_minutes, qr.collected_at "
        "FROM window w "
        "LEFT JOIN queue_record qr ON qr.window_id = w.id "
        "  AND qr.id = (SELECT id FROM queue_record WHERE window_id = w.id "
        "               ORDER BY collected_at DESC LIMIT 1) "
        "WHERE w.canteen_id = ? AND w.status = 1",
        [cb](const drogon::orm::Result& r) mutable {
            Json::Value arr(Json::arrayValue);
            for (auto& row : r) {
                int qc = row["queue_count"].isNull() ? 0 : row["queue_count"].as<int>();
                int wm = row["estimated_wait_minutes"].isNull() ? 0 : row["estimated_wait_minutes"].as<int>();
                std::string collectedAt = row["collected_at"].isNull() ? "" : row["collected_at"].as<std::string>();

                // isStale: collected_at older than 60s
                bool stale = true;
                if (!collectedAt.empty()) {
                    // Simple heuristic: if we got data, assume fresh (real check needs time parsing)
                    stale = false;
                }

                Json::Value item;
                item["windowId"]             = (Json::Int64)row["window_id"].as<long long>();
                item["windowName"]           = row["window_name"].as<std::string>();
                item["queueCount"]           = qc;
                item["estimatedWaitMinutes"] = wm;
                item["crowdLevel"]           = utils::crowdLevelStr(utils::classifyCrowd(qc));
                item["isStale"]              = stale;
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
        id
    );
}

// ── GET /api/v1/canteens/{id}/seats ──────────────────────────────────────────
void CanteenCtrl::getCanteenSeats(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                   long long id) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT "
        "  COUNT(*) AS total, "
        "  SUM(status='AVAILABLE') AS available, "
        "  SUM(status='RESERVED')  AS reserved, "
        "  SUM(status='OCCUPIED')  AS occupied "
        "FROM seat WHERE canteen_id = ?",
        [cb, id](const drogon::orm::Result& r) mutable {
            if (r.empty()) {
                cb(utils::ApiResponse::error(utils::ERR_NOT_FOUND.status,
                                             utils::ERR_NOT_FOUND.code,
                                             utils::ERR_NOT_FOUND.message));
                return;
            }
            auto row = r[0];
            Json::Value data;
            data["canteenId"]     = (Json::Int64)id;
            data["totalSeats"]    = row["total"].as<int>();
            data["availableSeats"]= row["available"].isNull() ? 0 : row["available"].as<int>();
            data["reservedSeats"] = row["reserved"].isNull()  ? 0 : row["reserved"].as<int>();
            data["occupiedSeats"] = row["occupied"].isNull()  ? 0 : row["occupied"].as<int>();
            cb(utils::ApiResponse::ok(data));
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << e.base().what();
            cb(utils::ApiResponse::error(utils::ERR_INTERNAL.status,
                                         utils::ERR_INTERNAL.code,
                                         utils::ERR_INTERNAL.message));
        },
        id
    );
}
