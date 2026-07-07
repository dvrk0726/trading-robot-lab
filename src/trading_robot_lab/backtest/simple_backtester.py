from __future__ import annotations

from dataclasses import dataclass, field

from trading_robot_lab.core.config import AppConfig
from trading_robot_lab.core.models import (
    MarketSnapshot,
    OrderIntent,
    PortfolioState,
    Side,
    SignalAction,
    StrategySignal,
)
from trading_robot_lab.risk.risk_engine import RiskEngine
from trading_robot_lab.strategy.synthetic_spread import SyntheticSpreadStrategy


@dataclass(frozen=True)
class BacktestEvent:
    ts: str
    event_type: str
    message: str
    position: int
    realized_pnl: float


@dataclass
class BacktestResult:
    final_portfolio: PortfolioState
    events: list[BacktestEvent] = field(default_factory=list)


class SimpleBacktester:
    """Very small backtester for architecture validation.

    Important limitation: this assumes immediate fills at snapshot futures price.
    It is not a realistic exchange simulator yet. Later it must be replaced by
    order book replay with queue, latency, commissions and partial fills.
    """

    def __init__(self, config: AppConfig, strategy: SyntheticSpreadStrategy) -> None:
        self.config = config
        self.strategy = strategy
        self.risk = RiskEngine(config)
        self.portfolio = PortfolioState()
        self.events: list[BacktestEvent] = []
        self.entry_price: float | None = None

    def run(self, snapshots: list[MarketSnapshot]) -> BacktestResult:
        for snapshot in snapshots:
            signal = self.strategy.on_snapshot(snapshot, self.portfolio.position)
            self._handle_signal(snapshot, signal)
        return BacktestResult(final_portfolio=self.portfolio, events=self.events)

    def _handle_signal(self, snapshot: MarketSnapshot, signal: StrategySignal) -> None:
        if signal.action == SignalAction.HOLD:
            return

        if signal.action in {SignalAction.BUY, SignalAction.SELL} and self.portfolio.position == 0:
            side = Side.BUY if signal.action == SignalAction.BUY else Side.SELL
            intent = OrderIntent(
                ts=snapshot.ts,
                side=side,
                qty=abs(signal.target_qty),
                limit_price=snapshot.futures_price,
                reason=signal.reason,
            )
            decision = self.risk.check_order(intent, self.portfolio)
            if not decision.allowed:
                self._record(snapshot, "risk_reject", decision.reason)
                return

            signed_qty = intent.qty if side == Side.BUY else -intent.qty
            self.portfolio.position += signed_qty
            self.entry_price = snapshot.futures_price
            self.portfolio.orders_sent_today += 1
            self._record(snapshot, "entry", f"{side.value} qty={intent.qty} price={snapshot.futures_price}")
            return

        if signal.action == SignalAction.EXIT and self.portfolio.position != 0:
            if self.entry_price is None:
                self._record(snapshot, "error", "position_without_entry_price")
                return

            pnl = (snapshot.futures_price - self.entry_price) * self.portfolio.position
            self.portfolio.realized_pnl += pnl
            self.portfolio.position = 0
            self.entry_price = None
            self._record(snapshot, "exit", f"price={snapshot.futures_price} pnl={pnl:.2f}")

    def _record(self, snapshot: MarketSnapshot, event_type: str, message: str) -> None:
        self.events.append(
            BacktestEvent(
                ts=snapshot.ts.isoformat(),
                event_type=event_type,
                message=message,
                position=self.portfolio.position,
                realized_pnl=self.portfolio.realized_pnl,
            )
        )
