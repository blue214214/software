#include "TimeoutScheduler.h"
#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>

void TimeoutScheduler::initAndStart(const Json::Value&) {
    // Schedule every 60 seconds
    drogon::app().getLoop()->runEvery(60.0, [this] { runExpiryCheck(); });
    LOG_INFO << "TimeoutScheduler started";
}

void TimeoutScheduler::runExpiryCheck() {
    auto db = drogon::app().getDbClient();

    // 1. Expire PENDING reservations past pickup_deadline (15 min)
    db->execSqlAsync(
        "SELECT id, dish_id, seat_id FROM reservation "
        "WHERE status = 'PENDING' AND pickup_deadline < NOW()",
        [](const drogon::orm::Result& r) {
            for (auto& row : r) {
                long long resId  = row["id"].as<long long>();
                long long dishId = row["dish_id"].as<long long>();
                long long seatId = row["seat_id"].isNull() ? 0 : row["seat_id"].as<long long>();

                auto db2 = drogon::app().getDbClient();
                db2->execSqlAsync(
                    "UPDATE reservation SET status='EXPIRED' WHERE id = ? AND status='PENDING'",
                    [dishId, seatId](const drogon::orm::Result&) {
                        // Restore stock
                        auto db3 = drogon::app().getDbClient();
                        db3->execSqlAsync(
                            "UPDATE dish SET stock_count = stock_count + 1 WHERE id = ?",
                            [](const drogon::orm::Result&) {},
                            [](const drogon::orm::DrogonDbException& e) { LOG_ERROR << e.base().what(); },
                            dishId
                        );
                        // Release seat
                        if (seatId > 0) {
                            auto db4 = drogon::app().getDbClient();
                            db4->execSqlAsync(
                                "UPDATE seat SET status='AVAILABLE' WHERE id = ? AND status='RESERVED'",
                                [](const drogon::orm::Result&) {},
                                [](const drogon::orm::DrogonDbException& e) { LOG_ERROR << e.base().what(); },
                                seatId
                            );
                        }
                    },
                    [](const drogon::orm::DrogonDbException& e) { LOG_ERROR << e.base().what(); },
                    resId
                );
            }
        },
        [](const drogon::orm::DrogonDbException& e) { LOG_ERROR << "Expiry check error: " << e.base().what(); }
    );

    // 2. Expire PICKED_UP reservations past leave_deadline (60 min dining timeout)
    db->execSqlAsync(
        "SELECT id, seat_id FROM reservation "
        "WHERE status = 'PICKED_UP' AND leave_deadline < NOW()",
        [](const drogon::orm::Result& r) {
            for (auto& row : r) {
                long long resId  = row["id"].as<long long>();
                long long seatId = row["seat_id"].isNull() ? 0 : row["seat_id"].as<long long>();

                auto db2 = drogon::app().getDbClient();
                db2->execSqlAsync(
                    "UPDATE reservation SET status='COMPLETED' WHERE id = ? AND status='PICKED_UP'",
                    [seatId](const drogon::orm::Result&) {
                        if (seatId > 0) {
                            auto db3 = drogon::app().getDbClient();
                            db3->execSqlAsync(
                                "UPDATE seat SET status='AVAILABLE' WHERE id = ? AND status='OCCUPIED'",
                                [](const drogon::orm::Result&) {},
                                [](const drogon::orm::DrogonDbException& e) { LOG_ERROR << e.base().what(); },
                                seatId
                            );
                        }
                    },
                    [](const drogon::orm::DrogonDbException& e) { LOG_ERROR << e.base().what(); },
                    resId
                );
            }
        },
        [](const drogon::orm::DrogonDbException& e) { LOG_ERROR << "Dining timeout check error: " << e.base().what(); }
    );
}
