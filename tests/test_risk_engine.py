from __future__ import annotations

from datetime import datetime, timezone

from trading_robot_lab.core.config import AppConfig
from trading_robot_lab.core.models import OrderIntent, PortfolioState, Side
from trading_robot_lab.risk.risk_engine import RiskEngine


def test_risk_allows_valid_backtest_order() -> None:
    config = AppConfig(trading_mode="backtest", max_position_abs=1, max_order_qty=1)
    risk = RiskEngine(config)
    intent = OrderIntent(
        ts=datetime.now(timezone.utc),
        side=Side.BUY,
        qty=1,
        limit_price=100_000,
        reason="test",
    )

    decision = risk.check_order(intent, PortfolioState())

    assert decision.allowed is True
    assert decision.reason == "allowed"


def test_risk_blocks_position_limit_exceeded() -> None:
    config = AppConfig(trading_mode="backtest", max_position_abs=1, max_order_qty=1)
    risk = RiskEngine(config)
    intent = OrderIntent(
        ts=datetime.now(timezone.utc),
        side=Side.BUY,
        qty=1,
        limit_price=100_000,
        reason="test",
    )

    decision = risk.check_order(intent, PortfolioState(position=1))

    assert decision.allowed is False
    assert decision.reason == "position_limit_exceeded"


def test_config_blocks_live_without_explicit_flag() -> None:
    config = AppConfig(trading_mode="live", live_trading_enabled=False)
    risk = RiskEngine(config)
    intent = OrderIntent(
        ts=datetime.now(timezone.utc),
        side=Side.BUY,
        qty=1,
        limit_price=100_000,
        reason="test",
    )

    decision = risk.check_order(intent, PortfolioState())

    assert decision.allowed is False
    assert decision.reason == "live_mode_blocked_by_safety_gate"
