# 实施计划：北京交通大学食堂就餐服务平台

## 概述

本计划按开发顺序分为六个阶段：基础设施搭建 → 后端核心服务 → 数据采集与历史数据 → 移动端 → 集成测试。
三人团队分工：**成员1**（Flutter 移动端）、**成员2**（C++ / Drogon 后端）、**成员3**（Python State_Generator）。

后端技术栈：C++17、Drogon 框架、MySQL 8.0、Redis（hiredis）、RabbitMQ（AMQP-CPP）、jwt-cpp、bcrypt、CMake。

## 任务

- [x] 1. 搭建项目基础设施与开发环境（成员2 + 成员3协作）
  - 初始化 Git 仓库，配置 `.gitignore`（Java/Flutter/Python）
  - 编写 `docker-compose.yml`，包含 MySQL 8.0、Redis 7.0、RabbitMQ 3.12.x 服务
  - 配置 MySQL 初始化脚本：创建数据库、用户、字符集（utf8mb4）
  - 配置 RabbitMQ：创建 `canteen.queue`、`canteen.seat` 交换机和队列，配置死信队列（DLQ）
  - _需求：8.1, 9.2, 9.3_


- [-] 2. 初始化 Backend 项目结构与公共组件（成员2）
  - 使用 Spring Initializr 创建 Spring Boot 3.0.x / Java 17 项目
  - 配置 `pom.xml`：引入 Spring Web、Spring Security、Spring Data JPA、Spring AMQP、Spring Data Redis、jqwik、Testcontainers 依赖
  - 创建包结构：`auth/`、`queue/`、`reservation/`、`seat/`、`search/`、`recommendation/`、`report/`、`collector/`、`common/`
  - 实现统一响应封装 `ApiResponse<T>` 和全局异常处理器 `GlobalExceptionHandler`（含所有错误码定义）
  - 配置 `application.yml`：数据源、Redis、RabbitMQ 连接参数
  - _需求：9.1_

- [x] 3. 实现数据库 Schema 与 JPA 实体（成员2）
  - 编写 `schema.sql`：创建 `user`、`canteen`、`window`、`dish`、`queue_record`、`reservation`、`seat`、`weekly_report` 表
  - 添加所有索引：`idx_queue_window_time`、`idx_queue_dedup`（唯一）、`idx_reservation_user`、`idx_reservation_dish`、`idx_reservation_seat`、`idx_dish_name`（全文）、`idx_dish_window`、`idx_seat_canteen_status`、`idx_report_user_week`
  - 创建对应 JPA 实体类（含 `Reservation.status` 枚举：PENDING/PICKED_UP/COMPLETED/CANCELLED/EXPIRED；`Seat.status` 枚举：AVAILABLE/RESERVED/OCCUPIED）
  - 创建各实体的 `JpaRepository` 接口
  - _需求：3.1, 5.1, 7.2, 8.2_


- [x] 4. 实现 Auth_Service（成员2）
  - [x] 4.1 实现 JWT 工具类与令牌黑名单
    - 实现 `JwtUtil`：签发 access_token（30min TTL）和 refresh_token（7d TTL），验证签名，提取 claims
    - 实现 Redis 刷新令牌黑名单：`auth:blacklist:{jti}` 键，TTL 与令牌剩余有效期一致
    - _需求：1.1, 1.4, 1.5_
  - [x] 4.2 实现登录、刷新、登出接口
    - `POST /api/v1/auth/login`：验证学号密码，检查账户锁定，返回令牌对，记录失败次数（`login:fail:{student_id}`，TTL 15min）
    - `POST /api/v1/auth/refresh`：验证刷新令牌，检查黑名单，签发新令牌，旧令牌加入黑名单（轮换）
    - `POST /api/v1/auth/logout`：将刷新令牌加入黑名单
    - _需求：1.1, 1.2, 1.3, 1.4, 1.5_
  - [x] 4.3 配置 Spring Security 与 JWT 过滤器
    - 配置 `SecurityFilterChain`：白名单路径（`/api/v1/auth/**`），实现 `JwtAuthenticationFilter`
    - _需求：1.6_
  - [ ]* 4.4 编写属性测试：属性 1 — 有效凭据登录返回令牌对
    - **属性 1：有效凭据登录返回令牌对**
    - **验证需求：1.1**
  - [ ]* 4.5 编写属性测试：属性 2 — 无效凭据登录不返回令牌
    - **属性 2：无效凭据登录不返回令牌**
    - **验证需求：1.2**
  - [ ]* 4.6 编写属性测试：属性 3 — 账户锁定机制
    - **属性 3：连续 5 次失败后任何登录请求均被拒绝**
    - **验证需求：1.3**
  - [ ]* 4.7 编写属性测试：属性 4 — 刷新令牌轮换 Round-Trip
    - **属性 4：同一刷新令牌第二次调用返回 HTTP 401**
    - **验证需求：1.4**
  - [ ]* 4.8 编写属性测试：属性 5 — 登出使刷新令牌失效
    - **属性 5：登出后刷新令牌不可再用**
    - **验证需求：1.5**
  - [ ]* 4.9 编写属性测试：属性 6 — 无效 JWT 令牌被拒绝
    - **属性 6：签名无效或被篡改的 JWT 访问受保护接口返回 HTTP 401**
    - **验证需求：1.6**

- [x] 5. 检查点 — 认证模块验证（成员2）
  - 确保所有测试通过，如有疑问请向用户确认。


- [x] 6. 实现 Queue_Service 与 WebSocket 推送（成员2）
  - [x] 6.1 实现排队数据查询接口
    - `GET /api/v1/canteens`：查询所有食堂，从 Redis `queue:canteen:{id}` 读取拥挤程度，缓存未命中时从 MySQL 读取
    - `GET /api/v1/canteens/{id}/queues`：返回指定食堂所有窗口最新 QueueRecord，含 `estimated_wait_minutes` 和 `is_stale` 标志（`collected_at` 距当前超过 60 秒则为 true）
    - 实现 `CrowdLevelUtil.classify(queueCount)`：`<5` → GREEN，`5-15` → YELLOW，`>15` → RED
    - _需求：2.1, 2.2, 2.5, 2.6_
  - [x] 6.2 实现 WebSocket 推送服务
    - 配置 `WebSocketConfig`，注册 `/ws/queues` 端点
    - 实现 `QueuePushScheduler`：非高峰期每 30 秒、高峰期（工作日 11:00–13:00、17:00–19:00）每 15 秒推送 `QUEUE_UPDATE` 消息
    - 同步推送 `SEAT_UPDATE` 消息（含 AVAILABLE/RESERVED/OCCUPIED 三态统计）
    - 客户端断开时清理订阅关系，重连后立即推送最新数据
    - _需求：2.3, 2.4, 5.8_
  - [ ]* 6.3 编写属性测试：属性 7 — 排队数据 API 完整性
    - **属性 7：对任意存在的 canteen_id，返回该食堂所有 Window 的排队记录**
    - **验证需求：2.2**
  - [ ]* 6.4 编写属性测试：属性 8 — 数据过期判断
    - **属性 8：collected_at 距当前超过 60 秒时 is_stale 为 true，否则为 false**
    - **验证需求：2.5**
  - [ ]* 6.5 编写属性测试：属性 9 — 拥挤程度颜色编码完备性
    - **属性 9：三区间覆盖所有非负整数且互不重叠**
    - **验证需求：2.6**

- [x] 7. 实现 Seat_Service（成员2）
  - [x] 7.1 实现座位查询接口
    - `GET /api/v1/canteens/{id}/seats`：返回总座位数、AVAILABLE/RESERVED/OCCUPIED 各分量，从 Redis `seat:canteen:{id}` 读取（TTL 120s）
    - _需求：5.1, 5.8_
  - [x] 7.2 实现座位自动分配逻辑
    - 预订时使用 `SELECT ... FOR UPDATE` 从该 Canteen 中选取一个 AVAILABLE 座位，更新为 RESERVED，关联至 Reservation
    - 无 AVAILABLE 座位时返回 `NO_AVAILABLE_SEAT` 错误
    - _需求：3.1, 3.3, 5.2_
  - [x] 7.3 实现座位状态转换方法
    - `confirmPickup(reservationId)`：RESERVED → OCCUPIED，更新 Reservation 状态为 PICKED_UP，记录 `pickup_at`，设置 `leave_deadline = pickup_at + 60min`
    - `confirmLeave(reservationId)`：OCCUPIED → AVAILABLE，更新 Reservation 状态为 COMPLETED
    - `releaseReserved(reservationId)`：RESERVED → AVAILABLE（取消或预订超时）
    - `releaseOccupied(reservationId)`：OCCUPIED → AVAILABLE（就餐超时）
    - _需求：5.3, 5.4, 5.5, 5.6, 5.7_
  - [ ]* 7.4 编写属性测试：属性 15 — 座位容量不变量
    - **属性 15：任意时刻 AVAILABLE + RESERVED + OCCUPIED = 总座位数，各分量均不小于 0**
    - **验证需求：5.1, 5.8**
  - [ ]* 7.5 编写属性测试：属性 14 — 座位状态完整 Round-Trip
    - **属性 14：预订→确认取餐→确认离开后座位依次经历 AVAILABLE→RESERVED→OCCUPIED→AVAILABLE**
    - **验证需求：5.3, 5.4, 5.6**


- [x] 8. 实现 Reservation_Service（成员2）
  - [x] 8.1 实现预订创建接口
    - `POST /api/v1/reservations`：使用 Redis 分布式锁（`SETNX`）扣减库存，调用 Seat_Service 自动分配座位，生成 `reservation_no`，设置 `pickup_deadline = created_at + 15min`，3 秒内返回
    - 检查单用户同时段有效预订数量（≤3），超限返回 `RESERVATION_LIMIT_EXCEEDED`
    - 库存为 0 时返回 `STOCK_INSUFFICIENT`，无可用座位时返回 `NO_AVAILABLE_SEAT`
    - _需求：3.1, 3.2, 3.3, 3.7_
  - [x] 8.2 实现取餐确认与离开确认接口
    - `POST /api/v1/reservations/{id}/pickup`：调用 `Seat_Service.confirmPickup()`，Reservation 状态 PENDING → PICKED_UP
    - `POST /api/v1/reservations/{id}/leave`：调用 `Seat_Service.confirmLeave()`，Reservation 状态 PICKED_UP → COMPLETED
    - _需求：5.3, 5.4_
  - [x] 8.3 实现预订取消接口与查询接口
    - `DELETE /api/v1/reservations/{id}`：取消 PENDING 状态预订，恢复 `stock_count`，调用 `Seat_Service.releaseReserved()`，幂等处理
    - `GET /api/v1/reservations/my`：查询当前用户预订列表
    - _需求：3.6, 5.7_
  - [x] 8.4 实现超时定时任务
    - `ReservationExpiryScheduler`（每分钟执行）：扫描 `pickup_deadline` 已过期的 PENDING 预订，更新为 EXPIRED，恢复库存，调用 `Seat_Service.releaseReserved()`
    - `DiningTimeoutScheduler`（每分钟执行）：扫描 `leave_deadline` 已过期的 PICKED_UP 预订，更新为 COMPLETED，调用 `Seat_Service.releaseOccupied()`
    - _需求：3.5, 5.5, 5.6_
  - [ ]* 8.5 编写属性测试：属性 10 — 预订成功后座位状态变更
    - **属性 10：库存>0且有 AVAILABLE 座位时，预订成功返回非空 reservation_no 和 seat_id，对应 Seat 变为 RESERVED**
    - **验证需求：3.1, 5.2**
  - [ ]* 8.6 编写属性测试：属性 11 — 取消/超时后库存与座位恢复 Round-Trip
    - **属性 11：取消或超时后 stock_count 恢复，Seat 变回 AVAILABLE；多次取消幂等**
    - **验证需求：3.5, 3.6, 5.5, 5.7**
  - [ ]* 8.7 编写属性测试：属性 12 — 单用户预订数量限制
    - **属性 12：同一就餐时段有效预订已达 3 条时，第 4 次返回 RESERVATION_LIMIT_EXCEEDED**
    - **验证需求：3.7**
  - [ ]* 8.8 编写属性测试：属性 13 — 并发预订安全性（库存不变量）
    - **属性 13：并发预订成功总数不超过初始库存，stock_count 不小于 0**
    - 使用 `ExecutorService` 模拟 100 个并发请求
    - **验证需求：3.8**

- [x] 9. 实现 Search_Service（成员2）
  - [x] 9.1 实现菜品搜索接口
    - `GET /api/v1/dishes/search?q={keyword}&canteen_id={id}`：使用 MySQL 全文索引（`MATCH AGAINST`）搜索，关键词长度 >50 返回 HTTP 400
    - 结果缓存至 Redis `dish:search:{hash}`（TTL 30s），含 `available_today` 状态
    - _需求：4.1, 4.2, 4.4, 4.5, 4.6_
  - [ ]* 9.2 编写属性测试：属性 16 — 搜索结果正确性与完整性
    - **属性 16：结果中每个 Dish 名称包含关键词，且数据库中所有匹配 Dish 均出现（召回率 100%）**
    - **验证需求：4.1, 4.2**
  - [ ]* 9.3 编写属性测试：属性 17 — 按食堂筛选搜索结果
    - **属性 17：带 canteen_id 筛选时，结果中所有 Dish 的 canteen_id 等于筛选值**
    - **验证需求：4.4**
  - [ ]* 9.4 编写属性测试：属性 18 — 搜索关键词长度验证
    - **属性 18：关键词超过 50 字符时返回 HTTP 400**
    - **验证需求：4.6**

- [x] 10. 检查点 — 后端核心服务验证（成员2）
  - 确保所有测试通过，如有疑问请向用户确认。


- [x] 11. 实现 Recommendation_Service（成员2）
  - [x] 11.1 实现推荐算法核心逻辑
    - 实现 `RecommendationEngine`：查询当前用户历史预订频次（按 dish_id 分组计数），按频次降序排列
    - 新用户（无历史记录）降级为全局热门：按全体用户预订量降序排列
    - 对排队人数超过 15 人的 Window 下的 Dish 施加降权（乘以权重系数 < 1.0）
    - 仅返回 `available_today = 1` 且 `stock_count > 0` 的 Dish，最多 10 条
    - 结果缓存至 Redis `recommend:user:{id}`（TTL 60s）
    - _需求：6.1, 6.2, 6.3, 6.4, 6.5_
  - [x] 11.2 实现推荐接口
    - `GET /api/v1/recommendations`：返回个性化推荐菜品列表，含库存状态（库存为 0 时标注 `sold_out: true`）
    - _需求：6.1, 6.6_
  - [ ]* 11.3 编写属性测试：属性 19 — 推荐列表长度不变量
    - **属性 19：推荐接口返回的菜品列表长度不超过 10 条**
    - **验证需求：6.1**
  - [ ]* 11.4 编写属性测试：属性 20 — 推荐排序综合属性
    - **属性 20：历史频次高的 Dish 排在频次低的前面（排队权重相同时）；排队>15人的 Window 的 Dish 不高于同等频次但排队≤15人的 Dish**
    - **验证需求：6.2, 6.4**
  - [ ]* 11.5 编写属性测试：属性 21 — 推荐有效性不变量
    - **属性 21：推荐列表中所有 Dish 均满足 available_today=1 且所属 Window 存在**
    - **验证需求：6.5**

- [x] 12. 实现 Report_Service（成员2）
  - [x] 12.1 实现周报生成定时任务
    - `WeeklyReportScheduler`：每周日 23:59 执行，查询本周有预订记录的所有用户，为每人生成 WeeklyReport
    - 计算字段：`meal_count`（PICKED_UP 或 COMPLETED 的 Reservation 数量）、`dish_count`（预订菜品总数）、`top_canteen_id`（最常去食堂）、`top_dishes`（Top3 菜品 JSON）、`total_amount`（有效预订菜品价格总和）、`daily_meal_counts`（每日就餐次数 JSON）、`canteen_distribution`（各食堂占比 JSON）
    - 本周无预订记录的用户不生成周报
    - _需求：7.1, 7.2, 7.5_
  - [x] 12.2 实现周报查询接口
    - `GET /api/v1/reports/weekly`：返回当前用户所有历史周报列表，按 `week_start` 降序排列
    - `GET /api/v1/reports/weekly/{id}`：返回指定周报详情，含 `daily_meal_counts`（折线图数据）和 `canteen_distribution`（饼图数据）
    - _需求：7.3, 7.4_
  - [ ]* 12.3 编写属性测试：属性 22 — 周报数据一致性
    - **属性 22：meal_count 等于本周 PICKED_UP 或 COMPLETED 的 Reservation 数量；total_amount 等于有效预订菜品价格总和**
    - **验证需求：7.2**
  - [ ]* 12.4 编写属性测试：属性 23 — 历史周报列表完整性
    - **属性 23：列表接口返回该用户所有已生成的 WeeklyReport，按 week_start 降序排列**
    - **验证需求：7.4**

- [x] 13. 实现 IP 限流中间件（成员2）
  - 实现 `RateLimitFilter`：使用 Redis `ratelimit:ip:{ip}` 计数器，60 秒滑动窗口，超过 300 次返回 HTTP 429
  - _需求：9.4_
  - [ ]* 13.1 编写属性测试：属性 27 — IP 限流不变量
    - **属性 27：任意 60 秒窗口内同一 IP 成功请求数不超过 300，第 301 次返回 HTTP 429**
    - **验证需求：9.4**

- [x] 14. 实现 Backend RabbitMQ 消息消费者（成员2）
  - 实现 `QueueRecordConsumer`：消费 `QUEUE_RECORD` 消息，利用唯一索引 `idx_queue_dedup` 去重写入 MySQL，更新 Redis 缓存，触发 WebSocket 推送
  - 实现 `SeatRecordConsumer`：消费 `SEAT_RECORD` 消息，更新 Redis `seat:canteen:{id}` 缓存
  - 消费失败时消息进入 DLQ，记录错误日志，不阻塞正常消息
  - 格式不符合 Schema 的消息拒绝并记录日志，不影响后续消息
  - _需求：8.1, 8.7, 8.8_
  - [ ]* 14.1 编写属性测试：属性 25 — 无效消息格式拒绝
    - **属性 25：不符合 Schema 的消息被拒绝并记录错误，不影响后续有效消息**
    - **验证需求：8.7**
  - [ ]* 14.2 编写属性测试：属性 26 — 消息去重幂等性
    - **属性 26：相同 (canteen_id, window_id, collected_at) 的重复消息，数据库记录数始终为 1**
    - **验证需求：8.8**

- [x] 15. 检查点 — 后端全量服务验证（成员2）
  - 确保所有测试通过，如有疑问请向用户确认。


- [ ] 16. 初始化 State_Generator 项目结构（成员3）
  - 创建 `state_generator/` 目录，初始化 `requirements.txt`
  - 引入依赖：`pika`（RabbitMQ）、`hypothesis`（属性测试）、`pydantic`（Schema 验证）、`pytest`、`sqlalchemy`（直接写入历史数据）
  - 定义消息 Schema：`QueueRecordMessage` 和 `SeatRecordMessage`（Pydantic 模型，含所有必需字段）
  - 支持通过 YAML 配置文件定义食堂数量、窗口数量、座位总数、Demo 账号列表等参数
  - _需求：8.1, 8.6_

- [ ] 17. 实现历史数据预生成脚本（成员3）
  - [ ] 17.1 实现基础数据初始化
    - 编写 `seed_base_data.py`：向 MySQL 写入食堂、窗口、菜品、座位、Demo 账号基础数据
    - Demo 账号至少 3 个，每个账号密码已哈希，可直接登录
    - _需求：8.2_
  - [ ] 17.2 实现历史预订数据生成
    - 编写 `generate_history.py`：为每个 Demo 账号生成至少 4 周（28天）的历史 Reservation 记录
    - 每个 Demo 账号至少 20 条历史 Reservation，状态为 PICKED_UP 或 COMPLETED（确保推荐和周报有数据）
    - 历史数据时间分布符合就餐规律（高峰期集中，工作日多于周末）
    - 同步生成对应的历史 QueueRecord 数据（每个窗口每 30 分钟一条）
    - _需求：8.2, 8.3, 8.4_
  - [ ] 17.3 实现历史周报预生成
    - 编写 `generate_weekly_reports.py`：基于历史 Reservation 数据，为每个 Demo 账号生成至少 4 条 WeeklyReport
    - 计算字段与 Report_Service 逻辑一致（meal_count、top_dishes、total_amount 等）
    - _需求：8.4_
  - [ ]* 17.4 编写属性测试：属性 24 — 消息序列化 Round-Trip
    - **属性 24：deserialize(serialize(record)) == record，所有字段值完全一致**
    - **验证需求：8.6**

- [ ] 18. 实现实时状态模拟（成员3）
  - [ ] 18.1 实现模拟数据生成逻辑
    - 实现 `QueueStateSimulator`：按时段规则生成排队人数（高峰期偏向高值，非高峰期偏向低值）
    - 实现 `SeatStateSimulator`：按时段规则生成座位占用数，保证 `occupied_seats <= total_seats`
    - _需求：8.1_
  - [ ] 18.2 实现消息发布与断线重连
    - 实现 `RabbitMQPublisher`：连接 RabbitMQ，按配置时间间隔定期发布消息
    - 连接中断时本地缓存数据，恢复后按时间顺序补发
    - _需求：8.1, 8.5_
  - [ ]* 18.3 编写单元测试：历史数据完整性验证
    - 验证每个 Demo 账号的 Reservation 记录数 ≥ 20，WeeklyReport 数量 ≥ 4
    - _需求：8.3, 8.4_

- [ ] 19. 检查点 — 数据采集链路验证（成员3）
  - 确保所有测试通过，如有疑问请向用户确认。


- [ ] 20. 实现 Mobile_App 基础框架（成员1）
  - 使用 Flutter 创建项目，配置 `pubspec.yaml`：引入 `dio`（HTTP）、`web_socket_channel`、`provider` 或 `riverpod`（状态管理）、`flutter_local_notifications`、`fl_chart`（图表）
  - 实现 `ApiClient`：封装 HTTP 请求，自动附加 JWT，处理 401 时自动刷新令牌
  - 实现 `WebSocketService`：连接 `/ws/queues`，处理断线重连，缓存最后一次数据
  - 实现路由配置（登录页 → 主页 → 食堂详情 → 预订 → 搜索 → 推荐 → 周报）
  - _需求：1.1, 2.7_

- [ ] 21. 实现 Mobile_App 认证页面（成员1）
  - 实现登录页面：学号/密码输入，调用 `POST /api/v1/auth/login`，存储令牌至 `SecureStorage`
  - 处理错误状态：401 显示错误提示，账户锁定显示锁定提示
  - _需求：1.1, 1.2, 1.3_

- [ ] 22. 实现 Mobile_App 排队展示页面（成员1）
  - [ ] 22.1 实现食堂列表主页
    - 调用 `GET /api/v1/canteens`，展示食堂列表及颜色编码拥挤程度（绿/黄/红）
    - 网络中断时展示最后缓存数据及获取时间戳
    - _需求：2.1, 2.6, 2.7_
  - [ ] 22.2 实现食堂详情页（窗口排队列表）
    - 调用 `GET /api/v1/canteens/{id}/queues`，展示各窗口排队人数和预计等待时间
    - 订阅 WebSocket `QUEUE_UPDATE` 消息实时更新，数据过期（>60s）时显示"数据暂不可用"
    - _需求：2.2, 2.3, 2.4, 2.5_

- [ ] 23. 实现 Mobile_App 座位查看页面（成员1）
  - 调用 `GET /api/v1/canteens/{id}/seats`，展示总座位数、AVAILABLE/RESERVED/OCCUPIED 各分量
  - 订阅 WebSocket `SEAT_UPDATE` 消息实时更新三态统计
  - _需求：5.1, 5.8_

- [ ] 24. 实现 Mobile_App 菜品搜索页面（成员1）
  - 实现搜索输入框，调用 `GET /api/v1/dishes/search`，展示菜品名称、所属食堂/窗口、当日供应状态
  - 搜索结果为空时展示"未找到相关菜品"，今日不供应菜品标注"今日不供应"
  - 支持按食堂筛选（`canteen_id` 参数）
  - _需求：4.1, 4.3, 4.4, 4.5_

- [ ] 25. 实现 Mobile_App 预订功能（成员1）
  - [ ] 25.1 实现预订创建与列表页
    - 实现预订创建流程：选择菜品 → 确认 → 调用 `POST /api/v1/reservations`，展示预订编号和分配的座位信息
    - 实现预订列表页：调用 `GET /api/v1/reservations/my`，展示预订状态
    - 实现取消预订：调用 `DELETE /api/v1/reservations/{id}`
    - 实现本地推送通知：预订成功后提醒 15 分钟内取餐
    - _需求：3.1, 3.4, 3.6_
  - [ ] 25.2 实现取餐确认与离开确认操作
    - 预订详情页展示"我已取餐"按钮（PENDING 状态时可点击），调用 `POST /api/v1/reservations/{id}/pickup`
    - 展示"我已离开"按钮（PICKED_UP 状态时可点击），调用 `POST /api/v1/reservations/{id}/leave`
    - 状态变更后实时刷新预订详情
    - _需求：5.3, 5.4_

- [ ] 26. 实现 Mobile_App 个性化推荐页面（成员1）
  - 调用 `GET /api/v1/recommendations`，展示推荐菜品列表（最多 10 条）
  - 库存为 0 的菜品标注"已售罄"，不从列表中移除
  - _需求：6.1, 6.6_

- [ ] 27. 实现 Mobile_App 个人周报页面（成员1）
  - [ ] 27.1 实现历史周报列表页
    - 调用 `GET /api/v1/reports/weekly`，展示历史周报列表，按时间倒序排列
    - _需求：7.4_
  - [ ] 27.2 实现周报详情页
    - 调用 `GET /api/v1/reports/weekly/{id}`，展示本周就餐次数、预订菜品总数、最常去食堂、最爱菜品 Top3、消费金额估算
    - 使用 `fl_chart` 渲染折线图（每日就餐次数趋势）和饼图（各食堂就餐占比），2 秒内完成渲染
    - _需求：7.2, 7.3, 7.6_

- [ ] 28. 检查点 — 移动端功能验证（成员1）
  - 确保所有测试通过，如有疑问请向用户确认。


- [ ] 29. 实现集成测试（成员2 + 成员3协作）
  - [ ] 29.1 配置 Testcontainers 集成测试环境
    - 创建 `IntegrationTestBase`：使用 Testcontainers 启动 MySQL 8.0、Redis 7.0、RabbitMQ 3.12.x 容器
    - 配置测试数据初始化脚本（含 Demo 账号和基础菜品数据）
    - _需求：8.1_
  - [ ]* 29.2 编写集成测试：State_Generator → RabbitMQ → Backend → MySQL 完整数据流
    - 运行 State_Generator 发布 `QUEUE_RECORD` 消息，验证 Backend 消费后 MySQL 中记录正确写入，Redis 缓存同步更新
    - _需求：8.1, 8.8, 9.2_
  - [ ]* 29.3 编写集成测试：预订完整流程（含座位三态变更）
    - 执行"创建预订 → 确认取餐 → 确认离开"完整流程，验证 Seat 状态依次经历 AVAILABLE→RESERVED→OCCUPIED→AVAILABLE
    - _需求：3.1, 5.3, 5.4_
  - [ ]* 29.4 编写集成测试：超时任务触发后的库存和座位恢复
    - 模拟预订超时（15 分钟）和就餐超时（60 分钟），验证库存恢复和座位状态回归 AVAILABLE
    - _需求：3.5, 5.5, 5.6_
  - [ ]* 29.5 编写集成测试：并发预订安全性端到端
    - 使用 `ExecutorService` 发起 100 个并发预订请求，验证成功数不超过初始库存
    - _需求：3.8_
  - [ ]* 29.6 编写集成测试：WebSocket 推送端到端验证
    - 建立 WebSocket 连接，触发数据更新，验证客户端收到 `QUEUE_UPDATE` 和 `SEAT_UPDATE` 消息
    - _需求：2.3, 2.4_

- [ ] 30. 最终检查点 — 全量测试通过
  - 确保所有测试通过，如有疑问请向用户确认。

## 备注

- 标注 `*` 的子任务为可选项，可在 MVP 阶段跳过以加快交付速度
- 每个属性测试最少运行 100 次迭代，并在注释中标注对应属性编号，例如：
  ```java
  // Feature: bjtu-canteen-service, Property 13: 并发预订安全性（库存不变量）
  @Property(tries = 100)
  void concurrentReservationSafety(...) { ... }
  ```
- Python 属性测试使用 Hypothesis 框架，Java 使用 jqwik 框架
- State_Generator 通过 YAML 配置文件控制模拟参数，历史数据预生成脚本（任务 17）需在系统首次启动前执行
- 任务依赖关系：任务 3 依赖任务 2；任务 4–14 依赖任务 3；任务 16–18 依赖任务 1；任务 20–27 依赖任务 6–12；任务 29 依赖所有后端和 State_Generator 任务
