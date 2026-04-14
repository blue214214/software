"""
任务 17.2 — 历史预订数据生成
为每个 Demo 账号生成 28 天历史 Reservation 和 QueueRecord
"""
import random
import uuid
from datetime import datetime, timedelta
from sqlalchemy import text
from db import load_config, get_session

# 就餐高峰时段（小时）
PEAK_HOURS = [(11, 13), (17, 19)]


def is_peak(hour: int) -> bool:
    return any(s <= hour < e for s, e in PEAK_HOURS)


def random_meal_time(date: datetime, is_workday: bool) -> datetime | None:
    """按就餐规律随机生成一个就餐时间，非工作日概率降低"""
    if not is_workday and random.random() < 0.4:
        return None
    # 高峰期概率更高
    if random.random() < 0.75:
        peak = random.choice(PEAK_HOURS)
        hour = random.randint(peak[0], peak[1] - 1)
    else:
        hour = random.choice([7, 8, 12, 18, 19])
    minute = random.randint(0, 59)
    return date.replace(hour=hour, minute=minute, second=0, microsecond=0)


def generate_history(config_path: str = "config.yaml"):
    config = load_config(config_path)
    session = get_session(config)

    try:
        # 获取 demo 用户 id
        users = {}
        for acc in config["demo_accounts"]:
            row = session.execute(
                text("SELECT id FROM user WHERE student_id=:sid"),
                {"sid": acc["student_id"]}
            ).fetchone()
            if row:
                users[acc["student_id"]] = row[0]

        # 获取所有菜品
        dishes = session.execute(
            text("SELECT id, window_id, price FROM dish")
        ).fetchall()

        # 获取窗口->食堂映射
        windows = {
            row[0]: row[1]
            for row in session.execute(text("SELECT id, canteen_id FROM window")).fetchall()
        }

        # 获取每个食堂的座位
        seats = {}
        for row in session.execute(text("SELECT id, canteen_id FROM seat")).fetchall():
            seats.setdefault(row[1], []).append(row[0])

        today = datetime.now().replace(hour=0, minute=0, second=0, microsecond=0)
        start_date = today - timedelta(days=28)

        for student_id, user_id in users.items():
            reservations_created = 0
            for day_offset in range(28):
                date = start_date + timedelta(days=day_offset)
                is_workday = date.weekday() < 5

                # 每天随机 1-3 次就餐
                meal_count = random.randint(1, 3) if is_workday else random.randint(0, 2)
                for _ in range(meal_count):
                    meal_time = random_meal_time(date, is_workday)
                    if meal_time is None:
                        continue

                    dish = random.choice(dishes)
                    dish_id, window_id, price = dish
                    canteen_id = windows.get(window_id)
                    if not canteen_id or canteen_id not in seats:
                        continue

                    seat_id = random.choice(seats[canteen_id])
                    reservation_no = uuid.uuid4().hex[:16].upper()
                    pickup_at = meal_time + timedelta(minutes=random.randint(5, 14))
                    leave_time = pickup_at + timedelta(minutes=random.randint(15, 55))
                    status = random.choice(["PICKED_UP", "COMPLETED"])

                    session.execute(text("""
                        INSERT INTO reservation
                          (user_id, dish_id, seat_id, reservation_no, status,
                           pickup_deadline, pickup_at, leave_deadline, created_at, updated_at)
                        VALUES
                          (:user_id, :dish_id, :seat_id, :reservation_no, :status,
                           :pickup_deadline, :pickup_at, :leave_deadline, :created_at, :updated_at)
                    """), {
                        "user_id": user_id,
                        "dish_id": dish_id,
                        "seat_id": seat_id,
                        "reservation_no": reservation_no,
                        "status": status,
                        "pickup_deadline": meal_time + timedelta(minutes=15),
                        "pickup_at": pickup_at,
                        "leave_deadline": pickup_at + timedelta(hours=1),
                        "created_at": meal_time,
                        "updated_at": leave_time,
                    })
                    reservations_created += 1

            print(f"{student_id}: 生成 {reservations_created} 条预订记录")

        # 生成历史 QueueRecord（每个窗口每 30 分钟一条）
        for day_offset in range(28):
            date = start_date + timedelta(days=day_offset)
            for hour in range(7, 21):
                for minute in [0, 30]:
                    collected_at = date.replace(hour=hour, minute=minute, second=0)
                    peak = is_peak(hour)
                    for w in windows:
                        canteen_id = windows[w]
                        queue_count = (
                            random.randint(5, 25) if peak else random.randint(0, 10)
                        )
                        wait = max(1, queue_count // 3)
                        try:
                            session.execute(text("""
                                INSERT IGNORE INTO queue_record
                                  (window_id, canteen_id, queue_count, estimated_wait_minutes, collected_at)
                                VALUES (:wid, :cid, :qc, :wait, :cat)
                            """), {
                                "wid": w, "cid": canteen_id,
                                "qc": queue_count, "wait": wait,
                                "cat": collected_at
                            })
                        except Exception:
                            pass

        session.commit()
        print("历史数据生成完成")
    except Exception as e:
        session.rollback()
        raise e
    finally:
        session.close()


if __name__ == "__main__":
    generate_history()
