"""
任务 17.1 — 基础数据初始化
向 MySQL 写入食堂、窗口、菜品、座位、Demo 账号
"""
import bcrypt
from sqlalchemy import text
from db import load_config, get_session

CANTEENS = [
    {"id": 1, "name": "第一食堂", "location": "北区", "total_seats": 200},
    {"id": 2, "name": "第二食堂", "location": "南区", "total_seats": 150},
]

WINDOWS = [
    {"id": 1, "canteen_id": 1, "name": "窗口1-炒菜", "description": "家常炒菜"},
    {"id": 2, "canteen_id": 1, "name": "窗口2-面食", "description": "各类面条"},
    {"id": 3, "canteen_id": 1, "name": "窗口3-盖浇饭", "description": "盖浇饭"},
    {"id": 4, "canteen_id": 2, "name": "窗口4-清真", "description": "清真食品"},
    {"id": 5, "canteen_id": 2, "name": "窗口5-西餐", "description": "西式快餐"},
]

DISHES = [
    {"window_id": 1, "name": "宫保鸡丁", "price": 12.00, "stock_count": 50},
    {"window_id": 1, "name": "鱼香肉丝", "price": 10.00, "stock_count": 50},
    {"window_id": 1, "name": "麻婆豆腐", "price": 8.00,  "stock_count": 60},
    {"window_id": 1, "name": "红烧肉",   "price": 15.00, "stock_count": 30},
    {"window_id": 2, "name": "牛肉面",   "price": 12.00, "stock_count": 40},
    {"window_id": 2, "name": "炸酱面",   "price": 9.00,  "stock_count": 40},
    {"window_id": 2, "name": "阳春面",   "price": 7.00,  "stock_count": 50},
    {"window_id": 3, "name": "番茄鸡蛋饭", "price": 10.00, "stock_count": 60},
    {"window_id": 3, "name": "红烧排骨饭", "price": 14.00, "stock_count": 40},
    {"window_id": 4, "name": "手抓饭",   "price": 13.00, "stock_count": 35},
    {"window_id": 4, "name": "羊肉汤",   "price": 11.00, "stock_count": 30},
    {"window_id": 5, "name": "汉堡套餐", "price": 18.00, "stock_count": 25},
    {"window_id": 5, "name": "意大利面", "price": 16.00, "stock_count": 20},
]


def hash_password(password: str) -> str:
    return bcrypt.hashpw(password.encode(), bcrypt.gensalt()).decode()


def seed(config_path: str = "config.yaml"):
    config = load_config(config_path)
    session = get_session(config)

    try:
        # 食堂
        for c in CANTEENS:
            session.execute(text("""
                INSERT INTO canteen (id, name, location, total_seats)
                VALUES (:id, :name, :location, :total_seats)
                ON DUPLICATE KEY UPDATE name=VALUES(name)
            """), c)

        # 窗口
        for w in WINDOWS:
            session.execute(text("""
                INSERT INTO window (id, canteen_id, name, description)
                VALUES (:id, :canteen_id, :name, :description)
                ON DUPLICATE KEY UPDATE name=VALUES(name)
            """), w)

        # 菜品
        for d in DISHES:
            session.execute(text("""
                INSERT INTO dish (window_id, name, price, stock_count, available_today)
                VALUES (:window_id, :name, :price, :stock_count, 1)
                ON DUPLICATE KEY UPDATE stock_count=VALUES(stock_count)
            """), d)

        # 座位
        for canteen in CANTEENS:
            existing = session.execute(
                text("SELECT COUNT(*) FROM seat WHERE canteen_id=:cid"),
                {"cid": canteen["id"]}
            ).scalar()
            if existing == 0:
                for i in range(canteen["total_seats"]):
                    floor = (i // 50) + 1
                    area = ["A", "B", "C", "D"][i % 4]
                    session.execute(text("""
                        INSERT INTO seat (canteen_id, floor, area, status)
                        VALUES (:canteen_id, :floor, :area, 'AVAILABLE')
                    """), {"canteen_id": canteen["id"], "floor": floor, "area": area})

        # Demo 账号
        for acc in config["demo_accounts"]:
            pw_hash = hash_password(acc["password"])
            session.execute(text("""
                INSERT INTO user (student_id, password_hash, name, role)
                VALUES (:student_id, :password_hash, :name, 'USER')
                ON DUPLICATE KEY UPDATE name=VALUES(name)
            """), {"student_id": acc["student_id"], "password_hash": pw_hash, "name": acc["name"]})

        session.commit()
        print("基础数据初始化完成")
    except Exception as e:
        session.rollback()
        raise e
    finally:
        session.close()


if __name__ == "__main__":
    seed()
