"""
任务 18.3 — 历史数据完整性验证
验证每个 Demo 账号的 Reservation >= 20，WeeklyReport >= 4
"""
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))

import pytest
from sqlalchemy import text
from db import load_config, get_session


@pytest.fixture(scope="module")
def session():
    config = load_config(os.path.join(os.path.dirname(os.path.dirname(__file__)), "config.yaml"))
    s = get_session(config)
    yield s
    s.close()


@pytest.fixture(scope="module")
def config():
    return load_config(os.path.join(os.path.dirname(os.path.dirname(__file__)), "config.yaml"))


def test_demo_accounts_exist(session, config):
    """每个 Demo 账号都存在于数据库"""
    for acc in config["demo_accounts"]:
        row = session.execute(
            text("SELECT id FROM user WHERE student_id=:sid"),
            {"sid": acc["student_id"]}
        ).fetchone()
        assert row is not None, f"Demo 账号 {acc['student_id']} 不存在"


def test_reservation_count_per_demo_account(session, config):
    """每个 Demo 账号至少有 20 条历史 Reservation（需求 8.3）"""
    for acc in config["demo_accounts"]:
        row = session.execute(text("""
            SELECT COUNT(*) FROM reservation r
            JOIN user u ON r.user_id = u.id
            WHERE u.student_id = :sid
              AND r.status IN ('PICKED_UP', 'COMPLETED')
        """), {"sid": acc["student_id"]}).fetchone()
        count = row[0] if row else 0
        assert count >= 20, (
            f"Demo 账号 {acc['student_id']} 只有 {count} 条记录，需要至少 20 条"
        )


def test_weekly_report_count_per_demo_account(session, config):
    """每个 Demo 账号至少有 4 条 WeeklyReport（需求 8.4）"""
    for acc in config["demo_accounts"]:
        row = session.execute(text("""
            SELECT COUNT(*) FROM weekly_report wr
            JOIN user u ON wr.user_id = u.id
            WHERE u.student_id = :sid
        """), {"sid": acc["student_id"]}).fetchone()
        count = row[0] if row else 0
        assert count >= 4, (
            f"Demo 账号 {acc['student_id']} 只有 {count} 条周报，需要至少 4 条"
        )
