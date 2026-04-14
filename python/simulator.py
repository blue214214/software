"""
任务 18.1 — 模拟数据生成逻辑
QueueStateSimulator 和 SeatStateSimulator
"""
import random
from datetime import datetime
from schemas import QueueRecordMessage, SeatRecordMessage


def _is_peak(hour: int) -> bool:
    return (11 <= hour < 13) or (17 <= hour < 19)


class QueueStateSimulator:
    """按时段规则生成排队人数"""

    def generate(self, canteen_id: int, window_id: int) -> QueueRecordMessage:
        hour = datetime.now().hour
        if _is_peak(hour):
            queue_count = random.randint(5, 30)
        else:
            queue_count = random.randint(0, 10)

        return QueueRecordMessage(
            canteen_id=canteen_id,
            window_id=window_id,
            collected_at=datetime.now(),
            queue_count=queue_count,
        )


class SeatStateSimulator:
    """按时段规则生成座位占用数，保证 occupied <= total"""

    def generate(self, canteen_id: int, total_seats: int) -> SeatRecordMessage:
        hour = datetime.now().hour
        if _is_peak(hour):
            occupied_ratio = random.uniform(0.5, 0.9)
        else:
            occupied_ratio = random.uniform(0.1, 0.4)

        occupied = int(total_seats * occupied_ratio)
        reserved = random.randint(0, min(20, total_seats - occupied))
        available = total_seats - occupied - reserved

        return SeatRecordMessage(
            canteen_id=canteen_id,
            collected_at=datetime.now(),
            available_seats=available,
            reserved_seats=reserved,
            occupied_seats=occupied,
        )
