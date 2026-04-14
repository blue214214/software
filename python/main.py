"""
State_Generator 主入口
持续模拟排队与座位状态并发布至 RabbitMQ
"""
import time
import logging
import yaml
from datetime import datetime
from simulator import QueueStateSimulator, SeatStateSimulator
from publisher import RabbitMQPublisher

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
logger = logging.getLogger(__name__)

CANTEENS = [
    {"id": 1, "total_seats": 200, "windows": [1, 2, 3]},
    {"id": 2, "total_seats": 150, "windows": [4, 5]},
]


def is_peak_hour() -> bool:
    h = datetime.now().hour
    return (11 <= h < 13) or (17 <= h < 19)


def run(config_path: str = "config.yaml"):
    with open(config_path, "r", encoding="utf-8") as f:
        config = yaml.safe_load(f)

    sim_cfg = config["simulation"]
    publisher = RabbitMQPublisher(config)
    queue_sim = QueueStateSimulator()
    seat_sim = SeatStateSimulator()

    logger.info("State_Generator 启动")

    try:
        while True:
            interval = (
                sim_cfg["peak_hour_interval_seconds"]
                if is_peak_hour()
                else sim_cfg["publish_interval_seconds"]
            )

            for canteen in CANTEENS:
                # 发布排队数据
                for window_id in canteen["windows"]:
                    msg = queue_sim.generate(canteen["id"], window_id)
                    publisher.publish(
                        config["rabbitmq"]["queue_record_exchange"],
                        msg.model_dump_json()
                    )

                # 发布座位数据
                seat_msg = seat_sim.generate(canteen["id"], canteen["total_seats"])
                publisher.publish(
                    config["rabbitmq"]["seat_record_exchange"],
                    seat_msg.model_dump_json()
                )

            logger.info(f"数据已发布，下次发布间隔 {interval}s")
            time.sleep(interval)
    except KeyboardInterrupt:
        logger.info("State_Generator 停止")
    finally:
        publisher.close()


if __name__ == "__main__":
    run()
