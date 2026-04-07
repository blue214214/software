#include "ReservationCtrl.h"
#include "utils/ApiResponse.h"
#include "utils/ErrorCode.h"
#include "utils/RedisUtil.h"
#include <drogon/drogon.h>
#include <json/json.h>

// ── POST /api/v1/reservations ─────────────────────────────────────────────────
void ReservationCtrl::create(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto body = req->getJsonObject();
    if (!body || !body->isMember("dishId")) {
        cb(utils::ApiResponse::validationError({{"dishId","必填"}}));
        return;
    }

    long long userId = std::stoll(req->getAttributes()->get<std::string>("userId"));
    long long dishId = (*body)["dishId"].asInt64();

    auto db = drogon::app().getDbClient();

    // 1. Check dish exists, has stock, get canteen_id via window
    db->execSqlAsync(
        "SELECT d.id, d.stock_count, d.price, w.canteen_id "
        "FROM dish d JOIN window w ON w.id = d.window_id "
        "WHERE d.id = ? AND d.available_today = 1",
        [=, cb = std::move(cb)](const drogon::orm::Result& r) mutable {
            if (r.empty()) {
                cb(utils::ApiResponse::error(utils::ERR_NOT_FOUND.status,
                                             utils::ERR_NOT_FOUND.code,
                                             utils::ERR_NOT_FOUND.message));
                return;
            }
            int stock       = r[0]["stock_count"].as<int>();
            long long canteenId = r[0]["canteen_id"].as<long long>();

            if (stock <= 0) {
                cb(utils::ApiResponse::error(utils::ERR_STOCK.status,
                                             utils::ERR_STOCK.code,
                                             utils::ERR_STOCK.message));
                return;
            }

            // 2. Check user reservation limit (≤3 PENDING in same meal period)
            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(
                "SELECT COUNT(*) AS cnt FROM reservation "
                "WHERE user_id = ? AND status = 'PENDING'",
                [=, cb = std::move(cb)](const drogon::orm::Result& r2) mutable {
                    int pending = r2[0]["cnt"].as<int>();
                    if (pending >= 3) {
                        cb(utils::ApiResponse::error(utils::ERR_RESERVATION_LIMIT.status,
                                                     utils::ERR_RESERVATION_LIMIT.code,
                                                     utils::ERR_RESERVATION_LIMIT.message));
                        return;
                    }

                    // 3. Allocate a seat (SELECT FOR UPDATE)
                    auto db3 = drogon::app().getDbClient();
                    db3->execSqlAsync(
                        "SELECT id FROM seat WHERE canteen_id = ? AND status = 'AVAILABLE' LIMIT 1 FOR UPDATE",
                        [=, cb = std::move(cb)](const drogon::orm::Result& r3) mutable {
                            if (r3.empty()) {
                                cb(utils::ApiResponse::error(utils::ERR_NO_SEAT.status,
                                                             utils::ERR_NO_SEAT.code,
                                                             utils::ERR_NO_SEAT.message));
                                return;
                            }
                            long long seatId = r3[0]["id"].as<long long>();
                            std::string reservationNo = drogon::utils::getUuid().substr(0, 16);

                            // 4. Deduct stock, reserve seat, create reservation atomically
                            auto db4 = drogon::app().getDbClient();
                            db4->execSqlAsync(
                                "UPDATE dish SET stock_count = stock_count - 1 WHERE id = ? AND stock_count > 0",
                                [=, cb = std::move(cb)](const drogon::orm::Result&) mutable {
                                    auto db5 = drogon::app().getDbClient();
                                    db5->execSqlAsync(
                                        "UPDATE seat SET status = 'RESERVED' WHERE id = ?",
                                        [=, cb = std::move(cb)](const drogon::orm::Result&) mutable {
                                            auto db6 = drogon::app().getDbClient();
                                            db6->execSqlAsync(
                                                "INSERT INTO reservation "
                                                "(user_id, dish_id, seat_id, reservation_no, status, pickup_deadline) "
                                                "VALUES (?, ?, ?, ?, 'PENDING', DATE_ADD(NOW(), INTERVAL 15 MINUTE))",
                                                [=, cb = std::move(cb)](const drogon::orm::Result& ins) mutable {
                                                    Json::Value data;
                                                    data["reservationNo"] = reservationNo;
                                                    data["seatId"]        = (Json::Int64)seatId;
                                                    data["dishId"]        = (Json::Int64)dishId;
                                                    data["status"]        = "PENDING";
                                                    cb(utils::ApiResponse::ok(data));
                                                },
                                                [cb](const drogon::orm::DrogonDbException& e) mutable {
                                                    LOG_ERROR << e.base().what();
                                                    cb(utils::ApiResponse::error(utils::ERR_INTERNAL.status,
                                                                                 utils::ERR_INTERNAL.code,
                                                                                 utils::ERR_INTERNAL.message));
                                                },
                                                userId, dishId, seatId, reservationNo
                                            );
                                        },
                                        [cb](const drogon::orm::DrogonDbException& e) mutable {
                                            LOG_ERROR << e.base().what();
                                            cb(utils::ApiResponse::error(utils::ERR_INTERNAL.status,
                                                                         utils::ERR_INTERNAL.code,
                                                                         utils::ERR_INTERNAL.message));
                                        },
                                        seatId
                                    );
                                },
                                [cb](const drogon::orm::DrogonDbException& e) mutable {
                                    LOG_ERROR << e.base().what();
                                    cb(utils::ApiResponse::error(utils::ERR_INTERNAL.status,
                                                                 utils::ERR_INTERNAL.code,
                                                                 utils::ERR_INTERNAL.message));
                                },
                                dishId
                            );
                        },
                        [cb](const drogon::orm::DrogonDbException& e) mutable {
                            LOG_ERROR << e.base().what();
                            cb(utils::ApiResponse::error(utils::ERR_INTERNAL.status,
                                                         utils::ERR_INTERNAL.code,
                                                         utils::ERR_INTERNAL.message));
                        },
                        canteenId
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
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << e.base().what();
            cb(utils::ApiResponse::error(utils::ERR_INTERNAL.status,
                                         utils::ERR_INTERNAL.code,
                                         utils::ERR_INTERNAL.message));
        },
        dishId
    );
}

// ── GET /api/v1/reservations/my ───────────────────────────────────────────────
void ReservationCtrl::myList(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    long long userId = std::stoll(req->getAttributes()->get<std::string>("userId"));
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT r.id, r.reservation_no, r.dish_id, r.seat_id, r.status, "
        "  r.pickup_deadline, r.pickup_at, r.leave_deadline, r.created_at, "
        "  d.name AS dish_name, d.price "
        "FROM reservation r JOIN dish d ON d.id = r.dish_id "
        "WHERE r.user_id = ? ORDER BY r.created_at DESC",
        [cb](const drogon::orm::Result& r) mutable {
            Json::Value arr(Json::arrayValue);
            for (auto& row : r) {
                Json::Value item;
                item["id"]              = (Json::Int64)row["id"].as<long long>();
                item["reservationNo"]   = row["reservation_no"].as<std::string>();
                item["dishId"]          = (Json::Int64)row["dish_id"].as<long long>();
                item["dishName"]        = row["dish_name"].as<std::string>();
                item["price"]           = row["price"].as<double>();
                item["seatId"]          = row["seat_id"].isNull() ? Json::Value::null : (Json::Int64)row["seat_id"].as<long long>();
                item["status"]          = row["status"].as<std::string>();
                item["pickupDeadline"]  = row["pickup_deadline"].as<std::string>();
                item["pickupAt"]        = row["pickup_at"].isNull() ? Json::Value::null : Json::Value(row["pickup_at"].as<std::string>());
                item["leaveDeadline"]   = row["leave_deadline"].isNull() ? Json::Value::null : Json::Value(row["leave_deadline"].as<std::string>());
                item["createdAt"]       = row["created_at"].as<std::string>();
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

// ── DELETE /api/v1/reservations/{id} ─────────────────────────────────────────
void ReservationCtrl::cancel(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                              long long id) {
    long long userId = std::stoll(req->getAttributes()->get<std::string>("userId"));
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT dish_id, seat_id, status FROM reservation WHERE id = ? AND user_id = ?",
        [=, cb = std::move(cb)](const drogon::orm::Result& r) mutable {
            if (r.empty()) {
                cb(utils::ApiResponse::error(utils::ERR_NOT_FOUND.status,
                                             utils::ERR_NOT_FOUND.code,
                                             utils::ERR_NOT_FOUND.message));
                return;
            }
            std::string status = r[0]["status"].as<std::string>();
            if (status != "PENDING") {
                cb(utils::ApiResponse::error(utils::ERR_SEAT_STATUS.status,
                                             utils::ERR_SEAT_STATUS.code,
                                             "只能取消待取餐的预订"));
                return;
            }
            long long dishId = r[0]["dish_id"].as<long long>();
            long long seatId = r[0]["seat_id"].isNull() ? 0 : r[0]["seat_id"].as<long long>();

            // Update reservation status
            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(
                "UPDATE reservation SET status = 'CANCELLED' WHERE id = ? AND status = 'PENDING'",
                [=, cb = std::move(cb)](const drogon::orm::Result&) mutable {
                    // Restore stock
                    auto db3 = drogon::app().getDbClient();
                    db3->execSqlAsync(
                        "UPDATE dish SET stock_count = stock_count + 1 WHERE id = ?",
                        [=, cb = std::move(cb)](const drogon::orm::Result&) mutable {
                            // Release seat
                            if (seatId > 0) {
                                auto db4 = drogon::app().getDbClient();
                                db4->execSqlAsync(
                                    "UPDATE seat SET status = 'AVAILABLE' WHERE id = ? AND status = 'RESERVED'",
                                    [cb](const drogon::orm::Result&) mutable { cb(utils::ApiResponse::ok()); },
                                    [cb](const drogon::orm::DrogonDbException&) mutable { cb(utils::ApiResponse::ok()); },
                                    seatId
                                );
                            } else {
                                cb(utils::ApiResponse::ok());
                            }
                        },
                        [cb](const drogon::orm::DrogonDbException& e) mutable {
                            LOG_ERROR << e.base().what();
                            cb(utils::ApiResponse::ok()); // best-effort
                        },
                        dishId
                    );
                },
                [cb](const drogon::orm::DrogonDbException& e) mutable {
                    LOG_ERROR << e.base().what();
                    cb(utils::ApiResponse::error(utils::ERR_INTERNAL.status,
                                                 utils::ERR_INTERNAL.code,
                                                 utils::ERR_INTERNAL.message));
                },
                id
            );
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

// ── POST /api/v1/reservations/{id}/pickup ────────────────────────────────────
void ReservationCtrl::confirmPickup(const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                     long long id) {
    long long userId = std::stoll(req->getAttributes()->get<std::string>("userId"));
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT seat_id, status FROM reservation WHERE id = ? AND user_id = ?",
        [=, cb = std::move(cb)](const drogon::orm::Result& r) mutable {
            if (r.empty() || r[0]["status"].as<std::string>() != "PENDING") {
                cb(utils::ApiResponse::error(utils::ERR_SEAT_STATUS.status,
                                             utils::ERR_SEAT_STATUS.code,
                                             utils::ERR_SEAT_STATUS.message));
                return;
            }
            long long seatId = r[0]["seat_id"].isNull() ? 0 : r[0]["seat_id"].as<long long>();

            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(
                "UPDATE reservation SET status='PICKED_UP', pickup_at=NOW(), "
                "leave_deadline=DATE_ADD(NOW(), INTERVAL 60 MINUTE) WHERE id = ?",
                [=, cb = std::move(cb)](const drogon::orm::Result&) mutable {
                    if (seatId > 0) {
                        auto db3 = drogon::app().getDbClient();
                        db3->execSqlAsync(
                            "UPDATE seat SET status='OCCUPIED' WHERE id = ?",
                            [cb](const drogon::orm::Result&) mutable { cb(utils::ApiResponse::ok()); },
                            [cb](const drogon::orm::DrogonDbException&) mutable { cb(utils::ApiResponse::ok()); },
                            seatId
                        );
                    } else {
                        cb(utils::ApiResponse::ok());
                    }
                },
                [cb](const drogon::orm::DrogonDbException& e) mutable {
                    LOG_ERROR << e.base().what();
                    cb(utils::ApiResponse::error(utils::ERR_INTERNAL.status,
                                                 utils::ERR_INTERNAL.code,
                                                 utils::ERR_INTERNAL.message));
                },
                id
            );
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

// ── POST /api/v1/reservations/{id}/leave ─────────────────────────────────────
void ReservationCtrl::confirmLeave(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                    long long id) {
    long long userId = std::stoll(req->getAttributes()->get<std::string>("userId"));
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT seat_id, status FROM reservation WHERE id = ? AND user_id = ?",
        [=, cb = std::move(cb)](const drogon::orm::Result& r) mutable {
            if (r.empty() || r[0]["status"].as<std::string>() != "PICKED_UP") {
                cb(utils::ApiResponse::error(utils::ERR_SEAT_STATUS.status,
                                             utils::ERR_SEAT_STATUS.code,
                                             utils::ERR_SEAT_STATUS.message));
                return;
            }
            long long seatId = r[0]["seat_id"].isNull() ? 0 : r[0]["seat_id"].as<long long>();

            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(
                "UPDATE reservation SET status='COMPLETED' WHERE id = ?",
                [=, cb = std::move(cb)](const drogon::orm::Result&) mutable {
                    if (seatId > 0) {
                        auto db3 = drogon::app().getDbClient();
                        db3->execSqlAsync(
                            "UPDATE seat SET status='AVAILABLE' WHERE id = ?",
                            [cb](const drogon::orm::Result&) mutable { cb(utils::ApiResponse::ok()); },
                            [cb](const drogon::orm::DrogonDbException&) mutable { cb(utils::ApiResponse::ok()); },
                            seatId
                        );
                    } else {
                        cb(utils::ApiResponse::ok());
                    }
                },
                [cb](const drogon::orm::DrogonDbException& e) mutable {
                    LOG_ERROR << e.base().what();
                    cb(utils::ApiResponse::error(utils::ERR_INTERNAL.status,
                                                 utils::ERR_INTERNAL.code,
                                                 utils::ERR_INTERNAL.message));
                },
                id
            );
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
