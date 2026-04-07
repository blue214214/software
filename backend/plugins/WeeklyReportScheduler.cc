#include "WeeklyReportScheduler.h"
#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>
#include <json/json.h>
#include <sstream>
#include <map>

void WeeklyReportScheduler::initAndStart(const Json::Value&) {
    // Check every 60s; generate when it's Sunday 23:59
    drogon::app().getLoop()->runEvery(60.0, [this] {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm* tm = std::localtime(&t);
        // Sunday=0, hour=23, minute=59
        if (tm->tm_wday == 0 && tm->tm_hour == 23 && tm->tm_min == 59) {
            generateReports();
        }
    });
    LOG_INFO << "WeeklyReportScheduler started";
}

void WeeklyReportScheduler::generateReports() {
    auto db = drogon::app().getDbClient();

    // Find all users with reservations this week
    db->execSqlAsync(
        "SELECT DISTINCT user_id FROM reservation "
        "WHERE created_at >= DATE_SUB(CURDATE(), INTERVAL WEEKDAY(CURDATE()) DAY) "
        "AND status IN ('PICKED_UP','COMPLETED')",
        [](const drogon::orm::Result& users) {
            for (auto& urow : users) {
                long long userId = urow["user_id"].as<long long>();

                auto db2 = drogon::app().getDbClient();
                db2->execSqlAsync(
                    "SELECT r.id, r.dish_id, r.created_at, d.price, d.name AS dish_name, "
                    "  w.canteen_id, c.name AS canteen_name "
                    "FROM reservation r "
                    "JOIN dish d ON d.id = r.dish_id "
                    "JOIN window w ON w.id = d.window_id "
                    "JOIN canteen c ON c.id = w.canteen_id "
                    "WHERE r.user_id = ? "
                    "AND r.status IN ('PICKED_UP','COMPLETED') "
                    "AND r.created_at >= DATE_SUB(CURDATE(), INTERVAL WEEKDAY(CURDATE()) DAY)",
                    [userId](const drogon::orm::Result& rows) {
                        if (rows.empty()) return;

                        int mealCount = (int)rows.size();
                        double totalAmount = 0.0;
                        std::map<std::string, int> dailyCounts;
                        std::map<long long, int> canteenCounts;
                        std::map<long long, std::pair<std::string,int>> dishCounts;

                        for (auto& row : rows) {
                            totalAmount += row["price"].as<double>();
                            std::string date = row["created_at"].as<std::string>().substr(0, 10);
                            dailyCounts[date]++;
                            long long cid = row["canteen_id"].as<long long>();
                            canteenCounts[cid]++;
                            long long did = row["dish_id"].as<long long>();
                            dishCounts[did].first  = row["dish_name"].as<std::string>();
                            dishCounts[did].second++;
                        }

                        // Top canteen
                        long long topCanteenId = 0;
                        int topCanteenCount = 0;
                        for (auto& [cid, cnt] : canteenCounts) {
                            if (cnt > topCanteenCount) { topCanteenCount = cnt; topCanteenId = cid; }
                        }

                        // Top 3 dishes
                        std::vector<std::pair<int,long long>> dishVec;
                        for (auto& [did, p] : dishCounts) dishVec.push_back({p.second, did});
                        std::sort(dishVec.rbegin(), dishVec.rend());

                        Json::Value topDishes(Json::arrayValue);
                        for (int i = 0; i < std::min((int)dishVec.size(), 3); ++i) {
                            long long did = dishVec[i].second;
                            Json::Value d;
                            d["dishId"]   = (Json::Int64)did;
                            d["dishName"] = dishCounts[did].first;
                            d["count"]    = dishCounts[did].second;
                            topDishes.append(d);
                        }

                        // daily_meal_counts JSON
                        Json::Value dailyJson;
                        for (auto& [date, cnt] : dailyCounts) dailyJson[date] = cnt;

                        // canteen_distribution JSON
                        Json::Value canteenJson;
                        for (auto& [cid, cnt] : canteenCounts)
                            canteenJson[std::to_string(cid)] = cnt;

                        Json::FastWriter fw;
                        std::string topDishesStr   = fw.write(topDishes);
                        std::string dailyStr        = fw.write(dailyJson);
                        std::string canteenDistStr  = fw.write(canteenJson);

                        auto db3 = drogon::app().getDbClient();
                        db3->execSqlAsync(
                            "INSERT INTO weekly_report "
                            "(user_id, week_start, week_end, meal_count, dish_count, "
                            " top_canteen_id, top_dishes, total_amount, daily_meal_counts, canteen_distribution) "
                            "VALUES (?, DATE_SUB(CURDATE(), INTERVAL WEEKDAY(CURDATE()) DAY), CURDATE(), "
                            "?, ?, ?, ?, ?, ?, ?) "
                            "ON DUPLICATE KEY UPDATE "
                            "meal_count=VALUES(meal_count), dish_count=VALUES(dish_count), "
                            "top_canteen_id=VALUES(top_canteen_id), top_dishes=VALUES(top_dishes), "
                            "total_amount=VALUES(total_amount), daily_meal_counts=VALUES(daily_meal_counts), "
                            "canteen_distribution=VALUES(canteen_distribution)",
                            [](const drogon::orm::Result&) {},
                            [](const drogon::orm::DrogonDbException& e) { LOG_ERROR << e.base().what(); },
                            userId, mealCount, mealCount, topCanteenId,
                            topDishesStr, totalAmount, dailyStr, canteenDistStr
                        );
                    },
                    [](const drogon::orm::DrogonDbException& e) { LOG_ERROR << e.base().what(); },
                    userId
                );
            }
        },
        [](const drogon::orm::DrogonDbException& e) { LOG_ERROR << "WeeklyReport error: " << e.base().what(); }
    );
}
