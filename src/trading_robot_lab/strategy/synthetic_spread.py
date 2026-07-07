from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field
from statistics import mean, pstdev

from trading_robot_lab.core.models import MarketSnapshot, SignalAction, StrategySignal


@dataclass
class SyntheticSpreadStrategy:
    """Research version of the old synthetic-index spread idea.

    It compares futures price with a coefficient-adjusted synthetic index.
    This implementation is intentionally simple and safe: it only produces signals,
    never sends orders.
    """

    coefficient: float = 1.0
    entry_z_score: float = 2.0
    exit_z_score: float = 0.5
    window: int = 50
    target_qty: int = 1
    _spreads: deque[float] = field(default_factory=deque, init=False)

    def __post_init__(self) -> None:
        if self.coefficient <= 0:
            raise ValueError("coefficient must be positive")
        if self.entry_z_score <= 0:
            raise ValueError("entry_z_score must be positive")
        if self.exit_z_score < 0:
            raise ValueError("exit_z_score must be non-negative")
        if self.window < 2:
            raise ValueError("window must be >= 2")
        if self.target_qty <= 0:
            raise ValueError("target_qty must be positive")

    def calculate_spread(self, snapshot: MarketSnapshot) -> float:
        return snapshot.futures_price - self.coefficient * snapshot.synthetic_index

    def on_snapshot(self, snapshot: MarketSnapshot, current_position: int) -> StrategySignal:
        spread = self.calculate_spread(snapshot)
        self._spreads.append(spread)
        if len(self._spreads) > self.window:
            self._spreads.popleft()

        if len(self._spreads) < self.window:
            return StrategySignal(
                ts=snapshot.ts,
                action=SignalAction.HOLD,
                reason="warming_up",
                spread=spread,
                z_score=None,
                target_qty=0,
            )

        spread_mean = mean(self._spreads)
        spread_std = pstdev(self._spreads)
        if spread_std == 0:
            return StrategySignal(
                ts=snapshot.ts,
                action=SignalAction.HOLD,
                reason="zero_spread_std",
                spread=spread,
                z_score=0.0,
                target_qty=0,
            )

        z_score = (spread - spread_mean) / spread_std

        if current_position == 0:
            if z_score >= self.entry_z_score:
                return StrategySignal(
                    ts=snapshot.ts,
                    action=SignalAction.SELL,
                    reason="spread_too_high_sell_futures",
                    spread=spread,
                    z_score=z_score,
                    target_qty=-self.target_qty,
                )
            if z_score <= -self.entry_z_score:
                return StrategySignal(
                    ts=snapshot.ts,
                    action=SignalAction.BUY,
                    reason="spread_too_low_buy_futures",
                    spread=spread,
                    z_score=z_score,
                    target_qty=self.target_qty,
                )
            return StrategySignal(
                ts=snapshot.ts,
                action=SignalAction.HOLD,
                reason="no_entry_signal",
                spread=spread,
                z_score=z_score,
                target_qty=0,
            )

        if abs(z_score) <= self.exit_z_score:
            return StrategySignal(
                ts=snapshot.ts,
                action=SignalAction.EXIT,
                reason="spread_normalized_exit",
                spread=spread,
                z_score=z_score,
                target_qty=0,
            )

        return StrategySignal(
            ts=snapshot.ts,
            action=SignalAction.HOLD,
            reason="position_open_waiting_exit",
            spread=spread,
            z_score=z_score,
            target_qty=current_position,
        )
