from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
from enum import Enum
from typing import Literal


class Side(str, Enum):
    BUY = "buy"
    SELL = "sell"


class SignalAction(str, Enum):
    BUY = "buy"
    SELL = "sell"
    HOLD = "hold"
    EXIT = "exit"


class OrderStatus(str, Enum):
    NEW = "new"
    ACCEPTED = "accepted"
    REJECTED = "rejected"
    FILLED = "filled"
    CANCELLED = "cancelled"


TradingMode = Literal["backtest", "paper", "live"]


@dataclass(frozen=True)
class MarketSnapshot:
    """Minimal market snapshot for research and demo backtests."""

    ts: datetime
    futures_price: float
    synthetic_index: float

    def __post_init__(self) -> None:
        if self.ts.tzinfo is None:
            raise ValueError("MarketSnapshot.ts must be timezone-aware")
        if self.futures_price <= 0:
            raise ValueError("futures_price must be positive")
        if self.synthetic_index <= 0:
            raise ValueError("synthetic_index must be positive")


@dataclass(frozen=True)
class StrategySignal:
    ts: datetime
    action: SignalAction
    reason: str
    spread: float
    z_score: float | None = None
    target_qty: int = 0


@dataclass(frozen=True)
class OrderIntent:
    """Strategy output after risk checks may become an order.

    This is not a broker order yet. It is an internal intent.
    """

    ts: datetime
    side: Side
    qty: int
    limit_price: float
    reason: str


@dataclass(frozen=True)
class Order:
    id: str
    ts: datetime
    side: Side
    qty: int
    limit_price: float
    status: OrderStatus = OrderStatus.NEW
    reason: str = ""


@dataclass
class PortfolioState:
    position: int = 0
    cash: float = 0.0
    realized_pnl: float = 0.0
    orders_sent_today: int = 0
    trading_day: datetime | None = None

    @property
    def is_flat(self) -> bool:
        return self.position == 0


def utc_now() -> datetime:
    return datetime.now(timezone.utc)
