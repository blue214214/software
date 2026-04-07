#!/bin/sh
set -e

# Generate config.json from environment variables
cat > /app/config.json <<EOF
{
  "listeners": [
    {
      "address": "0.0.0.0",
      "port": 8080,
      "https": false
    }
  ],
  "db_clients": [
    {
      "name": "default",
      "rdbms": "mysql",
      "host": "${MYSQL_HOST:-127.0.0.1}",
      "port": ${MYSQL_PORT:-3306},
      "dbname": "${MYSQL_DATABASE:-canteen_db}",
      "user": "${MYSQL_USER:-canteen}",
      "passwd": "${MYSQL_PASSWORD:-canteen123}",
      "connection_number": 10,
      "charset": "utf8mb4"
    }
  ],
  "app": {
    "threads_num": 4,
    "enable_session": false,
    "log_level": "WARN",
    "log_path": "./logs"
  },
  "plugins": [
    {"name": "TimeoutScheduler"},
    {"name": "WeeklyReportScheduler"},
    {"name": "MqConsumer"}
  ],
  "custom_config": {
    "jwt_secret": "${JWT_SECRET:-bjtu-canteen-secret-change-me}",
    "access_token_expiry_seconds": 1800,
    "refresh_token_expiry_seconds": 604800,
    "redis_host": "${REDIS_HOST:-127.0.0.1}",
    "redis_port": ${REDIS_PORT:-6379},
    "rabbitmq_host": "${RABBITMQ_HOST:-127.0.0.1}",
    "rabbitmq_port": ${RABBITMQ_PORT:-5672},
    "rabbitmq_user": "${RABBITMQ_USER:-canteen}",
    "rabbitmq_pass": "${RABBITMQ_PASS:-canteen123}",
    "pickup_timeout_minutes": 15,
    "dining_timeout_minutes": 60,
    "recommendation_max_results": 10,
    "queue_penalty_threshold": 15,
    "queue_penalty_weight": 0.3
  }
}
EOF

exec ./canteen_backend
