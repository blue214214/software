"""
任务 18.2 — RabbitMQ 消息发布与断线重连
"""
import json
import time
import logging
from collections import deque
from datetime import datetime
import pika
import yaml

logger = logging.getLogger(__name__)


class RabbitMQPublisher:
    def __init__(self, config: dict):
        self.config = config["rabbitmq"]
        self._connection = None
        self._channel = None
        self._cache: deque = deque()  # 断线时本地缓存

    def _connect(self):
        credentials = pika.PlainCredentials(
            self.config["user"], self.config["password"]
        )
        params = pika.ConnectionParameters(
            host=self.config["host"],
            port=self.config["port"],
            credentials=credentials,
            heartbeat=60,
        )
        self._connection = pika.BlockingConnection(params)
        self._channel = self._connection.channel()
        self._channel.exchange_declare(
            exchange=self.config["queue_record_exchange"],
            exchange_type="fanout",
            durable=True,
        )
        self._channel.exchange_declare(
            exchange=self.config["seat_record_exchange"],
            exchange_type="fanout",
            durable=True,
        )
        logger.info("RabbitMQ 连接成功")

    def _ensure_connected(self):
        if self._connection is None or self._connection.is_closed:
            self._connect()

    def publish(self, exchange: str, message_json: str):
        """发布消息，失败时缓存"""
        try:
            self._ensure_connected()
            # 先补发缓存
            while self._cache:
                cached_exchange, cached_msg = self._cache.popleft()
                self._channel.basic_publish(
                    exchange=cached_exchange,
                    routing_key="",
                    body=cached_msg.encode(),
                    properties=pika.BasicProperties(delivery_mode=2),
                )
            self._channel.basic_publish(
                exchange=exchange,
                routing_key="",
                body=message_json.encode(),
                properties=pika.BasicProperties(delivery_mode=2),
            )
        except Exception as e:
            logger.warning(f"发布失败，缓存消息: {e}")
            self._cache.append((exchange, message_json))
            self._connection = None

    def close(self):
        if self._connection and not self._connection.is_closed:
            self._connection.close()
