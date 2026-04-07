# 需求规格文档

## 简介

本文档描述北京交通大学食堂就餐服务平台（BJTU Canteen Service）的需求规格。该平台以 Flutter 移动端为主，为北京交通大学师生提供实时排队展示、菜品预订、菜品搜索、座位自动分配、个性化推荐及个人周报等功能，以缓解就餐高峰期拥堵、排队时间长、座位难找等问题。

系统由三个子系统组成：
- **Mobile_App**（Flutter）：面向师生用户的移动端应用
- **Backend**（Spring Boot）：提供 REST API、WebSocket 推送及消息处理
- **State_Generator**（Python）：状态模拟脚本，预生成历史数据并持续模拟排队与座位状态，通过 RabbitMQ 上报至 Backend

---

## 词汇表

- **System**：整个北京交通大学食堂就餐服务平台
- **Mobile_App**：基于 Flutter 的移动端应用，面向师生用户
- **Backend**：基于 Spring Boot 的后端服务，处理业务逻辑与数据持久化
- **State_Generator**：基于 Python 的状态模拟脚本，负责生成排队和座位的模拟数据并通过 RabbitMQ 上报至 Backend
- **Queue_Service**：后端中负责排队数据处理的服务模块
- **Reservation_Service**：后端中负责预订业务逻辑的服务模块
- **Seat_Service**：后端中负责座位状态管理的服务模块
- **Search_Service**：后端中负责菜品搜索的服务模块
- **Auth_Service**：后端中负责用户认证与授权的服务模块
- **Recommendation_Service**：后端中负责菜品个性化推荐的服务模块
- **Report_Service**：后端中负责生成用户个人周报的服务模块
- **Canteen**：食堂，包含多个 Window 和多个 Seat
- **Window**：食堂内的供餐窗口，提供若干 Dish
- **Dish**：窗口提供的具体菜品
- **QueueRecord**：某一时刻某窗口的排队人数记录
- **Reservation**：用户对特定 Dish 的预订记录，包含自动分配的 Seat
- **Seat**：食堂内的座位，具有 AVAILABLE、RESERVED、OCCUPIED 三种状态
- **WeeklyReport**：系统每周自动生成的用户个人就餐周报
- **User**：已注册的师生用户
- **Peak_Hour**：就餐高峰时段（工作日 11:00–13:00、17:00–19:00）
- **Stale_Data**：超过 60 秒未更新的排队数据
- **Demo_Account**：用于演示的预设账号，拥有预生成的历史数据

---

## 需求

### 需求 1：用户认证与权限管理

**用户故事：** 作为师生用户，我希望使用学号和密码登录系统，以便访问个人预订记录和就餐服务。

#### 验收标准

1. WHEN 用户提交有效的学号和密码，THE Auth_Service SHALL 在 2 秒内返回 JWT 访问令牌和刷新令牌。
2. WHEN 用户提交无效的学号或密码，THE Auth_Service SHALL 返回 HTTP 401 状态码及错误描述，且不返回任何令牌。
3. WHEN 用户连续 5 次登录失败，THE Auth_Service SHALL 锁定该账户 15 分钟，并拒绝后续登录请求。
4. WHEN JWT 访问令牌过期，THE Auth_Service SHALL 接受有效的刷新令牌并签发新的访问令牌。
5. WHEN 用户主动登出，THE Auth_Service SHALL 将当前刷新令牌加入黑名单，使其不可再用。
6. IF 请求携带的 JWT 令牌签名无效或已被篡改，THEN THE Auth_Service SHALL 拒绝该请求并返回 HTTP 401。

#### 正确性属性

- **幂等性**：对同一刷新令牌多次调用刷新接口，仅第一次成功，后续调用返回 HTTP 401（令牌轮换机制）。
- **不变量**：任意时刻，系统中处于有效状态的 JWT 令牌数量不超过活跃用户会话数量。

---

### 需求 2：实时排队情况展示

**用户故事：** 作为师生用户，我希望实时查看各食堂窗口的排队人数和预计等待时间，以便选择排队较短的窗口就餐。

#### 验收标准

1. THE Mobile_App SHALL 在主界面展示所有 Canteen 的列表及各 Canteen 当前整体拥挤程度。
2. WHEN 用户选择某个 Canteen，THE Mobile_App SHALL 展示该 Canteen 下所有 Window 的当前排队人数和预计等待时间。
3. THE Queue_Service SHALL 每 30 秒向 Mobile_App 推送一次最新的 QueueRecord 数据。
4. WHILE 处于 Peak_Hour，THE Queue_Service SHALL 每 15 秒向 Mobile_App 推送一次最新的 QueueRecord 数据。
5. IF QueueRecord 数据超过 60 秒未更新（Stale_Data），THEN THE Mobile_App SHALL 在对应 Window 旁显示"数据暂不可用"提示。
6. THE Mobile_App SHALL 以颜色编码（绿/黄/红）直观展示各 Window 的拥挤程度，绿色表示排队人数少于 5 人，黄色表示 5–15 人，红色表示超过 15 人。
7. WHEN 网络连接中断，THE Mobile_App SHALL 展示最后一次成功获取的数据，并显示数据获取时间戳。

#### 正确性属性

- **数据时效性**：在网络正常情况下，QueueRecord 从 State_Generator 上报到 Mobile_App 展示的端到端延迟不超过 5 秒。
- **颜色编码完备性**：绿/黄/红三个区间覆盖所有非负整数排队人数且互不重叠。

---

### 需求 3：菜品预订功能

**用户故事：** 作为师生用户，我希望提前预订菜品并自动获得座位，以便在就餐时直接取餐，减少排队等待时间。

#### 验收标准

1. WHEN 用户选择某个 Dish 并提交预订，THE Reservation_Service SHALL 在 3 秒内创建 Reservation 记录、返回预订编号，并从该 Dish 所在 Canteen 自动分配一个 AVAILABLE 状态的 Seat，将其状态更新为 RESERVED。
2. WHEN 用户提交预订时该 Dish 的可预订数量为 0，THE Reservation_Service SHALL 拒绝预订并返回"库存不足"错误，不分配任何 Seat。
3. WHEN 用户提交预订时该 Dish 所在 Canteen 无 AVAILABLE 状态的 Seat，THE Reservation_Service SHALL 拒绝预订并返回"无可用座位"错误。
4. WHEN 用户成功创建 Reservation，THE Mobile_App SHALL 通过推送通知提醒用户预订的取餐时间窗口（预订成功后 15 分钟内取餐）。
5. WHEN 预订成功后 15 分钟内用户未在 App 上点击"我已取餐"，THE Reservation_Service SHALL 自动将该 Reservation 状态更新为"已超时"，恢复对应 Dish 的可预订数量，并将分配的 Seat 状态从 RESERVED 恢复为 AVAILABLE。
6. THE Reservation_Service SHALL 支持用户在取餐截止时间前取消 Reservation，取消后恢复对应 Dish 的可预订数量，并将分配的 Seat 状态从 RESERVED 恢复为 AVAILABLE。
7. THE Reservation_Service SHALL 限制单个 User 在同一就餐时段内的有效 Reservation 数量不超过 3 条。
8. WHEN 多个用户同时预订同一 Dish 的最后一份，THE Reservation_Service SHALL 保证仅一个预订成功，其余返回"库存不足"错误。

#### 正确性属性

- **库存不变量**：任意时刻，某 Dish 的可预订数量 = 初始库存 - 有效 Reservation 数量，且可预订数量不小于 0。
- **幂等性**：对同一 Reservation 多次调用取消接口，结果与调用一次相同，不会导致库存或座位被多次恢复。
- **并发安全**：在并发场景下，同一 Dish 的成功预订总数不超过其初始库存数量。

---

### 需求 4：菜品搜索功能

**用户故事：** 作为师生用户，我希望通过关键词搜索菜品，以便快速找到目标菜品所在的食堂和窗口。

#### 验收标准

1. WHEN 用户输入至少 1 个字符的关键词并触发搜索，THE Search_Service SHALL 在 1 秒内返回匹配的 Dish 列表，包含菜品名称、所属 Canteen 名称、所属 Window 名称及当日供应状态。
2. THE Search_Service SHALL 支持对 Dish 名称的模糊匹配，匹配规则为关键词包含在菜品名称中。
3. WHEN 搜索结果为空，THE Mobile_App SHALL 展示"未找到相关菜品"提示，而非空白页面。
4. THE Search_Service SHALL 支持按 Canteen 筛选搜索结果。
5. WHEN 用户搜索的 Dish 当日不供应，THE Mobile_App SHALL 在搜索结果中标注"今日不供应"，而非从结果中隐藏。
6. THE Search_Service SHALL 对搜索关键词进行长度限制，超过 50 个字符的关键词返回 HTTP 400 错误。

#### 正确性属性

- **完整性**：对于数据库中名称包含关键词的所有 Dish，搜索结果不遗漏任何一条（召回率 = 100%）。
- **无副作用**：对同一关键词多次调用搜索接口，返回结果相同，且不修改任何数据库状态。

---

### 需求 5：座位自动分配与状态管理

**用户故事：** 作为师生用户，我希望预订菜品时系统自动为我分配座位，并能通过 App 更新就餐状态，以便合理利用食堂座位资源。

#### 验收标准

1. THE Seat_Service SHALL 维护每个 Seat 的三种状态：AVAILABLE（空闲）、RESERVED（已预留）、OCCUPIED（占用中）。
2. WHEN 用户成功预订菜品，THE Seat_Service SHALL 从该菜品所在 Canteen 中自动选取一个 AVAILABLE 状态的 Seat，将其状态更新为 RESERVED，并将该 Seat 信息关联至对应 Reservation。
3. WHEN 用户在 App 上点击"我已取餐"，THE Seat_Service SHALL 将该 Reservation 关联的 Seat 状态从 RESERVED 更新为 OCCUPIED，并将 Reservation 状态更新为"已取餐"。
4. WHEN 用户在 App 上点击"我已离开"，THE Seat_Service SHALL 将该 Reservation 关联的 Seat 状态从 OCCUPIED 更新为 AVAILABLE。
5. WHEN 预订成功后 15 分钟内用户未点击"我已取餐"（预订超时），THE Seat_Service SHALL 将对应 Seat 状态从 RESERVED 恢复为 AVAILABLE。
6. WHEN 用户点击"我已取餐"后 60 分钟内未点击"我已离开"（就餐超时），THE Seat_Service SHALL 自动将对应 Seat 状态从 OCCUPIED 恢复为 AVAILABLE。
7. WHEN 用户取消预订，THE Seat_Service SHALL 将对应 Seat 状态从 RESERVED 恢复为 AVAILABLE。
8. THE Mobile_App SHALL 展示各 Canteen 的座位总数、AVAILABLE 数量、RESERVED 数量和 OCCUPIED 数量。

#### 正确性属性

- **容量不变量**：任意时刻，某 Canteen 的 `AVAILABLE 数量 + RESERVED 数量 + OCCUPIED 数量 = 总座位数`，且各分量均不小于 0。
- **状态转换完备性**：Seat 状态仅在以下合法路径间转换：`AVAILABLE → RESERVED`（预订成功）、`RESERVED → OCCUPIED`（取餐确认）、`OCCUPIED → AVAILABLE`（离开或就餐超时）、`RESERVED → AVAILABLE`（取消预订或预订超时）。

---

### 需求 6：菜品个性化推荐

**用户故事：** 作为师生用户，我希望在菜品预订页面看到个性化推荐，以便快速发现适合自己口味的菜品。

#### 验收标准

1. WHEN 用户进入菜品预订页面，THE Recommendation_Service SHALL 在 1 秒内返回个性化推荐菜品列表，列表长度不超过 10 条。
2. WHEN 用户存在历史预订记录，THE Recommendation_Service SHALL 优先推荐该用户历史预订频次较高的 Dish。
3. WHEN 用户无历史预订记录（新用户），THE Recommendation_Service SHALL 展示全局热门菜品列表，按全体用户预订量降序排列。
4. THE Recommendation_Service SHALL 将当前排队人数超过 15 人的 Window 下的 Dish 降低推荐权重，使其在推荐列表中排名靠后。
5. THE Recommendation_Service SHALL 仅推荐当日供应（available_today = 1）且库存大于 0 的 Dish。
6. WHEN 推荐列表中的某 Dish 库存变为 0，THE Mobile_App SHALL 在该菜品旁标注"已售罄"，而非从列表中移除。

#### 正确性属性

- **推荐有效性不变量**：推荐列表中的所有 Dish 均满足当日供应且所属 Window 存在，不出现已下架或不存在的菜品。
- **降权单调性**：对于排队人数超过 15 人的 Window，其 Dish 在推荐列表中的排名不高于同等历史频次但排队人数不超过 15 人的 Window 的 Dish。

---

### 需求 7：个人就餐周报

**用户故事：** 作为师生用户，我希望每周收到个人就餐周报，以便了解自己的就餐习惯和消费情况。

#### 验收标准

1. THE Report_Service SHALL 在每周日 23:59 自动为所有在本周内有预订记录的 User 生成 WeeklyReport。
2. THE WeeklyReport SHALL 包含以下内容：本周就餐次数、预订菜品总数、最常去的 Canteen、最爱的菜品 Top3、本周消费金额估算。
3. THE Mobile_App SHALL 以折线图展示本周每日就餐次数趋势，以饼图展示各 Canteen 就餐占比。
4. THE Mobile_App SHALL 提供历史周报列表页面，用户可查看过去所有已生成的 WeeklyReport。
5. WHEN 用户本周无任何预订记录，THE Report_Service SHALL 不为该用户生成本周 WeeklyReport。
6. WHEN 用户查看某周报时，THE Mobile_App SHALL 在 2 秒内完成图表渲染并展示完整内容。

#### 正确性属性

- **数据一致性**：WeeklyReport 中的就餐次数等于该用户本周状态为"已取餐"或"已完成"的 Reservation 数量。
- **消费金额准确性**：WeeklyReport 中的消费金额估算等于本周所有有效 Reservation 对应 Dish 价格的总和。

---

### 需求 8：数据模拟与历史数据预生成

**用户故事：** 作为演示人员，我希望 State_Generator 能预生成足够的历史数据，以便推荐和周报功能在演示时立即有内容展示。

#### 验收标准

1. THE State_Generator SHALL 通过 RabbitMQ 消息队列将 QueueRecord 和 Seat 状态数据上报至 Backend。
2. THE State_Generator SHALL 在系统初始化时预生成至少 4 周的历史预订数据和排队数据，覆盖所有 Demo_Account。
3. THE State_Generator 预生成的历史数据 SHALL 使每个 Demo_Account 拥有不少于 20 条历史 Reservation 记录，以确保推荐功能有足够的个人历史数据。
4. THE State_Generator 预生成的历史数据 SHALL 覆盖至少 4 个完整自然周，以确保每个 Demo_Account 拥有至少 4 条历史 WeeklyReport。
5. WHEN State_Generator 与 RabbitMQ 的连接中断，THE State_Generator SHALL 在本地缓存采集数据，并在连接恢复后按时间顺序补发缓存数据。
6. THE State_Generator 上报的每条消息 SHALL 包含 `canteen_id`、`window_id`（排队数据）或 `canteen_id`（座位数据）、采集时间戳和数值字段。
7. IF State_Generator 上报的消息格式不符合 Schema 定义，THEN THE Backend SHALL 拒绝该消息并将错误记录至日志，不影响正常消息的处理。
8. THE Backend SHALL 对 State_Generator 上报的数据进行去重处理，相同 `canteen_id`、`window_id` 和时间戳的消息仅处理一次。

#### 正确性属性

- **消息格式 Round-Trip**：State_Generator 序列化的消息，经 Backend 反序列化后，所有字段值与序列化前完全一致（`deserialize(serialize(record)) == record`）。
- **幂等消费**：同一条消息被 Backend 消费多次（如因网络重试），数据库中的结果与消费一次相同，不产生重复记录。

---

### 需求 9：系统性能与可靠性

**用户故事：** 作为师生用户，我希望系统在就餐高峰期保持稳定响应，以便在繁忙时段也能顺畅使用各项功能。

#### 验收标准

1. WHILE 处于 Peak_Hour，THE Backend SHALL 支持不少于 500 个并发用户的 API 请求，P95 响应时间不超过 2 秒。
2. THE Backend SHALL 对频繁访问的排队数据和菜品数据使用 Redis 缓存，缓存命中率不低于 80%。
3. IF Backend 某个服务实例发生异常崩溃，THEN THE System SHALL 在 30 秒内自动重启该实例，期间其他实例继续提供服务。
4. THE Backend SHALL 对单个 IP 的 API 请求实施限流，每分钟不超过 300 次请求，超出后返回 HTTP 429。
5. THE System SHALL 保证核心数据（Reservation、User、WeeklyReport）的持久化存储，数据库异常恢复后数据零丢失。

#### 正确性属性

- **缓存一致性**：Redis 缓存中的 QueueRecord 数据与 MySQL 数据库中对应记录的值一致，或缓存已过期（TTL 到期后强制从数据库读取）。
- **限流不变量**：在任意 60 秒滑动窗口内，来自同一 IP 的成功处理请求数不超过 300 次。
