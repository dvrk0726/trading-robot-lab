from __future__ import annotations

import os
from dataclasses import dataclass

from trading_robot_lab.core.models import TradingMode


@dataclass(frozen=True)
class AppConfig:
    trading_mode: TradingMode = "backtest"
    live_trading_enabled: bool = False

    max_position_abs: int = 1
    max_order_qty: int = 1
    max_daily_loss: float = 1000.0
    max_orders_per_minute: int = 30

    max_market_data_staleness_ms: int = 1000
    max_allowed_price_deviation_pct: float = 5.0

    @property
    def live_allowed(self) -> bool:
        """Live requires two explicit flags, not one accidental setting."""

        return self.trading_mode == "live" and self.live_trading_enabled


def _get_bool(name: str, default: bool) -> bool:
    value = os.getenv(name)
    if value is None:
        return default
    return value.strip().lower() in {"1", "true", "yes", "y", "on"}


def _get_int(name: str, default: int) -> int:
    value = os.getenv(name)
    if value is None or value.strip() == "":
        return default
    return int(value)


def _get_float(name: str, default: float) -> float:
    value = os.getenv(name)
    if value is None or value.strip() == "":
        return default
    return float(value)


def load_config_from_env() -> AppConfig:
    mode = os.getenv("TRADING_MODE", "backtest").strip().lower()
    if mode not in {"backtest", "paper", "live"}:
        raise ValueError("TRADING_MODE must be one of: backtest, paper, live")

    config = AppConfig(
        trading_mode=mode,  # type: ignore[arg-type]
        live_trading_enabled=_get_bool("LIVE_TRADING_ENABLED", False),
        max_position_abs=_get_int("MAX_POSITION_ABS", 1),
        max_order_qty=_get_int("MAX_ORDER_QTY", 1),
        max_daily_loss=_get_float("MAX_DAILY_LOSS", 1000.0),
        max_orders_per_minute=_get_int("MAX_ORDERS_PER_MINUTE", 30),
        max_market_data_staleness_ms=_get_int("MAX_MARKET_DATA_STALENESS_MS", 1000),
        max_allowed_price_deviation_pct=_get_float("MAX_ALLOWED_PRICE_DEVIATION_PCT", 5.0),
    )

    if config.trading_mode == "live" and not config.live_trading_enabled:
        raise ValueError(
            "Refusing live mode: LIVE_TRADING_ENABLED must be true. "
            "This is a deliberate safety gate."
        )

    return config
