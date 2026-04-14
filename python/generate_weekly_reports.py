"""
任务 17.3 — 历史周报预生成
基于历史 Reservation 数据为每个 Demo 账号生成 WeeklyReport
"""
import json
from datetime import datetime, timedelta
from collections import defaultdict
from sqlalchemy import text
from db import load_config, get_session


def get_week_start(dt: datetime) -> datetime:
    """返回该日期所在周的周一"""
    return (dt - timedelta(days=dt.weekday())).replace(hour=0, minute=0, second=0, microsecond=0)


def generate_weekly_reports(config_path: str = "config.yaml"):
    config = load_config(config_path)
    session = get_session(config)

    try:
        users = session.execute(text("SELECT id FROM user WHERE role='USER'")).fetchall()

        for (user_id,) in users:
            reservations = session.execute(text("""
                SELECT r.id, r.dish_id, r.created_at, r.status,
                       d.price, d.name, w.canteen_id
                FROM reservation r
                JOIN dish d ON r.dish_id = d.id
                JOIN window w ON d.window_id = w.id
                WHERE r.user_id = :uid
                  AND r.status IN ('PICKED_UP', 'COMPLETED')
                ORDER BY r.created_at
            """), {"uid": user_id}).fetchall()

            if not reservations:
                continue

            # 按周分组
            weeks: dict = defaultdict(list)
            for row in reservations:
                ws = get_week_start(row[2])
                weeks[ws].append(row)

            for week_start, rows in weeks.items():
                week_end = week_start + timedelta(days=6)

                meal_count = len(rows)
                dish_count = len(rows)
                total_amount = sum(float(r[4]) for r in rows)

                # 最常去食堂
                canteen_counts: dict = defaultdict(int)
                for r in rows:
                    canteen_counts[r[6]] += 1
                top_canteen_id = max(canteen_counts, key=lambda k: canteen_counts[k])

                # Top3 菜品
                dish_counts: dict = defaultdict(lambda: {"count": 0, "name": ""})
                for r in rows:
                    dish_counts[r[1]]["count"] += 1
                    dish_counts[r[1]]["name"] = r[5]
                top_dishes = sorted(
                    [{"dish_id": k, "dish_name": v["name"], "count": v["count"]}
                     for k, v in dish_counts.items()],
                    key=lambda x: -x["count"]
                )[:3]

                # 每日就餐次数
                daily: dict = defaultdict(int)
                for r in rows:
                    daily[r[2].strftime("%Y-%m-%d")] += 1

                # 各食堂占比
                canteen_dist = {}
                canteen_names = {
                    row[0]: row[1]
                    for row in session.execute(text("SELECT id, name FROM canteen")).fetchall()
                }
                for cid, cnt in canteen_counts.items():
                    name = canteen_names.get(cid, str(cid))
                    canteen_dist[name] = cnt

                session.execute(text("""
                    INSERT INTO weekly_report
                      (user_id, week_start, week_end, meal_count, dish_count,
                       top_canteen_id, top_dishes, total_amount,
                       daily_meal_counts, canteen_distribution)
                    VALUES
                      (:user_id, :week_start, :week_end, :meal_count, :dish_count,
                       :top_canteen_id, :top_dishes, :total_amount,
                       :daily_meal_counts, :canteen_distribution)
                    ON DUPLICATE KEY UPDATE
                      meal_count=VALUES(meal_count),
                      total_amount=VALUES(total_amount)
                """), {
                    "user_id": user_id,
                    "week_start": week_start.date(),
                    "week_end": week_end.date(),
                    "meal_count": meal_count,
                    "dish_count": dish_count,
                    "top_canteen_id": top_canteen_id,
                    "top_dishes": json.dumps(top_dishes, ensure_ascii=False),
                    "total_amount": round(total_amount, 2),
                    "daily_meal_counts": json.dumps(dict(daily), ensure_ascii=False),
                    "canteen_distribution": json.dumps(canteen_dist, ensure_ascii=False),
                })

            print(f"用户 {user_id}: 生成 {len(weeks)} 条周报")

        session.commit()
        print("周报生成完成")
    except Exception as e:
        session.rollback()
        raise e
    finally:
        session.close()


if __name__ == "__main__":
    generate_weekly_reports()
