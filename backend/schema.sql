CREATE DATABASE IF NOT EXISTS canteen_db CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE canteen_db;

CREATE TABLE IF NOT EXISTS `user` (
    `id`                 BIGINT       NOT NULL AUTO_INCREMENT,
    `student_id`         VARCHAR(20)  NOT NULL UNIQUE,
    `password_hash`      VARCHAR(255) NOT NULL,
    `name`               VARCHAR(50)  NOT NULL,
    `role`               VARCHAR(10)  NOT NULL DEFAULT 'USER',
    `failed_login_count` INT          NOT NULL DEFAULT 0,
    `locked_until`       DATETIME     NULL,
    `created_at`         DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_at`         DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `canteen` (
    `id`          BIGINT       NOT NULL AUTO_INCREMENT,
    `name`        VARCHAR(50)  NOT NULL,
    `location`    VARCHAR(100) NOT NULL,
    `total_seats` INT          NOT NULL DEFAULT 0,
    `status`      TINYINT      NOT NULL DEFAULT 1,
    `created_at`  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_at`  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `window` (
    `id`          BIGINT       NOT NULL AUTO_INCREMENT,
    `canteen_id`  BIGINT       NOT NULL,
    `name`        VARCHAR(50)  NOT NULL,
    `description` VARCHAR(200) NULL,
    `status`      TINYINT      NOT NULL DEFAULT 1,
    `created_at`  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_at`  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    KEY `idx_window_canteen` (`canteen_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `dish` (
    `id`              BIGINT       NOT NULL AUTO_INCREMENT,
    `window_id`       BIGINT       NOT NULL,
    `name`            VARCHAR(50)  NOT NULL,
    `price`           DECIMAL(8,2) NOT NULL,
    `description`     TEXT         NULL,
    `available_today` TINYINT      NOT NULL DEFAULT 1,
    `stock_count`     INT          NOT NULL DEFAULT 0,
    `created_at`      DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_at`      DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    FULLTEXT INDEX `idx_dish_name` (`name`),
    KEY `idx_dish_window` (`window_id`, `available_today`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `seat` (
    `id`              BIGINT      NOT NULL AUTO_INCREMENT,
    `canteen_id`      BIGINT      NOT NULL,
    `floor`           INT         NOT NULL DEFAULT 1,
    `area`            VARCHAR(20) NULL,
    `status`          VARCHAR(10) NOT NULL DEFAULT 'AVAILABLE',
    `last_updated_at` DATETIME    NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    KEY `idx_seat_canteen_status` (`canteen_id`, `status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `queue_record` (
    `id`                     BIGINT   NOT NULL AUTO_INCREMENT,
    `window_id`              BIGINT   NOT NULL,
    `canteen_id`             BIGINT   NOT NULL,
    `queue_count`            INT      NOT NULL DEFAULT 0,
    `estimated_wait_minutes` INT      NOT NULL DEFAULT 0,
    `collected_at`           DATETIME NOT NULL,
    `created_at`             DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    UNIQUE KEY `idx_queue_dedup` (`canteen_id`, `window_id`, `collected_at`),
    KEY `idx_queue_window_time` (`window_id`, `collected_at` DESC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `reservation` (
    `id`              BIGINT      NOT NULL AUTO_INCREMENT,
    `user_id`         BIGINT      NOT NULL,
    `dish_id`         BIGINT      NOT NULL,
    `seat_id`         BIGINT      NULL,
    `reservation_no`  VARCHAR(32) NOT NULL UNIQUE,
    `status`          VARCHAR(15) NOT NULL DEFAULT 'PENDING',
    `pickup_deadline` DATETIME    NOT NULL,
    `pickup_at`       DATETIME    NULL,
    `leave_deadline`  DATETIME    NULL,
    `created_at`      DATETIME    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_at`      DATETIME    NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    KEY `idx_reservation_user`   (`user_id`, `status`, `created_at`),
    KEY `idx_reservation_dish`   (`dish_id`, `status`),
    KEY `idx_reservation_seat`   (`seat_id`, `status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `weekly_report` (
    `id`                   BIGINT        NOT NULL AUTO_INCREMENT,
    `user_id`              BIGINT        NOT NULL,
    `week_start`           DATE          NOT NULL,
    `week_end`             DATE          NOT NULL,
    `meal_count`           INT           NOT NULL DEFAULT 0,
    `dish_count`           INT           NOT NULL DEFAULT 0,
    `top_canteen_id`       BIGINT        NULL,
    `top_dishes`           JSON          NULL,
    `total_amount`         DECIMAL(10,2) NOT NULL DEFAULT 0.00,
    `daily_meal_counts`    JSON          NULL,
    `canteen_distribution` JSON          NULL,
    `generated_at`         DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    UNIQUE KEY `idx_report_user_week` (`user_id`, `week_start`),
    KEY `idx_report_user` (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
