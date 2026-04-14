# State_Generator（成员3）

Python 状态模拟脚本，负责预生成历史数据并持续模拟排队与座位状态。

## 快速开始

```bash
cd python
pip install -r requirements.txt
```

## 使用步骤

1. 确保 MySQL/RabbitMQ/Redis 已通过 docker-compose 启动
2. 初始化基础数据（食堂、菜品、Demo 账号）：
   ```bash
   python seed_base_data.py
   ```
3. 生成历史预订数据：
   ```bash
   python generate_history.py
   ```
4. 生成历史周报：
   ```bash
   python generate_weekly_reports.py
   ```
5. 启动实时模拟：
   ```bash
   python main.py
   ```

## 运行测试

```bash
pytest tests/ -v
```

## 文件说明

| 文件 | 说明 |
|------|------|
| `config.yaml` | 配置文件（数据库、RabbitMQ、Demo 账号） |
| `schemas.py` | Pydantic 消息 Schema |
| `db.py` | 数据库连接工具 |
| `seed_base_data.py` | 基础数据初始化 |
| `generate_history.py` | 历史预订和排队数据生成 |
| `generate_weekly_reports.py` | 历史周报生成 |
| `simulator.py` | 实时状态模拟器 |
| `publisher.py` | RabbitMQ 消息发布（含断线重连） |
| `main.py` | 主入口 |
