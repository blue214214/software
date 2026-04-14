"""
消息 Schema 定义（Pydantic 模型）
对应设计文档中 RabbitMQ 消息格式
"""
from datetime import datetime
from pydantic import BaseModel, field_validator


class QueueRecordMessage(BaseModel):
    """排队数据消息 Schema"""
    message_type: str = "QUEUE_RECORD"
    canteen_id: int
    window_id: int
    collected_at: datetime
    queue_count: int

    @field_validator("queue_count")
    @classmethod
    def queue_count_non_negative(cls, v):
        if v < 0:
            raise ValueError("queue_count must be non-negative")
        return v

    @field_validator("message_type")
    @classmethod
    def validate_message_type(cls, v):
        if v != "QUEUE_RECORD":
            raise ValueError("message_type must be QUEUE_RECORD")
        return v

    def model_dump_json_str(self) -> str:
        return self.model_dump_json()


class SeatRecordMessage(BaseModel):
    """座位数据消息 Schema"""
    message_type: str = "SEAT_RECORD"
    canteen_id: int
    collected_at: datetime
    available_seats: int
    reserved_seats: int
    occupied_seats: int

    @field_validator("available_seats", "reserved_seats", "occupied_seats")
    @classmethod
    def seats_non_negative(cls, v):
        if v < 0:
            raise ValueError("seat counts must be non-negative")
        return v

    @field_validator("message_type")
    @classmethod
    def validate_message_type(cls, v):
        if v != "SEAT_RECORD":
            raise ValueError("message_type must be SEAT_RECORD")
        return v
