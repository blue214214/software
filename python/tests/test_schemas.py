"""
属性测试：属性 24 — 消息序列化 Round-Trip
deserialize(serialize(record)) == record
"""
import json
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))

import pytest
from datetime import datetime
from hypothesis import given, settings
from hypothesis import strategies as st
from schemas import QueueRecordMessage, SeatRecordMessage

# Feature: bjtu-canteen-service, Property 24: 消息序列化 Round-Trip
@settings(max_examples=100)
@given(
    canteen_id=st.integers(min_value=1, max_value=100),
    window_id=st.integers(min_value=1, max_value=100),
    queue_count=st.integers(min_value=0, max_value=200),
)
def test_queue_record_serialization_roundtrip(canteen_id, window_id, queue_count):
    """属性 24：QueueRecordMessage 序列化后反序列化，所有字段值完全一致"""
    original = QueueRecordMessage(
        canteen_id=canteen_id,
        window_id=window_id,
        collected_at=datetime(2024, 1, 15, 12, 0, 0),
        queue_count=queue_count,
    )
    serialized = original.model_dump_json()
    restored = QueueRecordMessage.model_validate_json(serialized)

    assert restored.canteen_id == original.canteen_id
    assert restored.window_id == original.window_id
    assert restored.queue_count == original.queue_count
    assert restored.message_type == original.message_type


# Feature: bjtu-canteen-service, Property 24: 消息序列化 Round-Trip
@settings(max_examples=100)
@given(
    canteen_id=st.integers(min_value=1, max_value=100),
    available=st.integers(min_value=0, max_value=100),
    reserved=st.integers(min_value=0, max_value=50),
    occupied=st.integers(min_value=0, max_value=100),
)
def test_seat_record_serialization_roundtrip(canteen_id, available, reserved, occupied):
    """属性 24：SeatRecordMessage 序列化后反序列化，所有字段值完全一致"""
    original = SeatRecordMessage(
        canteen_id=canteen_id,
        collected_at=datetime(2024, 1, 15, 12, 0, 0),
        available_seats=available,
        reserved_seats=reserved,
        occupied_seats=occupied,
    )
    serialized = original.model_dump_json()
    restored = SeatRecordMessage.model_validate_json(serialized)

    assert restored.canteen_id == original.canteen_id
    assert restored.available_seats == original.available_seats
    assert restored.reserved_seats == original.reserved_seats
    assert restored.occupied_seats == original.occupied_seats
    assert restored.message_type == original.message_type


def test_invalid_queue_record_rejected():
    """属性 25：不符合 Schema 的消息（缺少必需字段）应被拒绝"""
    with pytest.raises(Exception):
        QueueRecordMessage.model_validate({"canteen_id": 1})  # 缺少必需字段


def test_negative_queue_count_rejected():
    """属性 25：queue_count 为负数应被拒绝"""
    with pytest.raises(Exception):
        QueueRecordMessage(
            canteen_id=1,
            window_id=1,
            collected_at=datetime.now(),
            queue_count=-1,
        )


def test_negative_seat_counts_rejected():
    """属性 25：座位数量为负数应被拒绝"""
    with pytest.raises(Exception):
        SeatRecordMessage(
            canteen_id=1,
            collected_at=datetime.now(),
            available_seats=-1,
            reserved_seats=0,
            occupied_seats=0,
        )
