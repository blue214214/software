"""数据库连接工具"""
import yaml
from sqlalchemy import create_engine, text
from sqlalchemy.orm import sessionmaker


def load_config(path: str = "config.yaml") -> dict:
    with open(path, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)


def get_engine(config: dict):
    db = config["database"]
    url = (
        f"mysql+pymysql://{db['user']}:{db['password']}"
        f"@{db['host']}:{db['port']}/{db['name']}?charset=utf8mb4"
    )
    return create_engine(url, echo=False)


def get_session(config: dict):
    engine = get_engine(config)
    Session = sessionmaker(bind=engine)
    return Session()
