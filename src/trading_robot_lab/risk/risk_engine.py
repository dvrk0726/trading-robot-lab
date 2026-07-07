from __future__ import annotations

from dataclasses import dataclass

from trading_robot_lab.core.config import AppConfig
from trading_robot_lab.core.models import OrderIntent, PortfolioState


@dataclass(frozen=True)
class RiskDecision:
    allowed: bool
    reason: str


class RiskEngine:
    """Minimal pre-trade risk checks.

    This is deliberately strict. It is designed for research and paper trading first,
    not for maximizing fill rate.
    """

    def __init__(self, config: AppConfig) -> None:
        self.config = config

    def check_order(self, intent: OrderIntent, portfolio: PortfolioState) -> RiskDecision:
        if intent.qty <= 0:
            return RiskDecision(False, "qty_must_be_positive")

        if intent.limit_price <= 0:
            return RiskDecision(False, "limit_price_must_be_positive")

        if intent.qty > self.config.max_order_qty:
            return RiskDecision(False, "order_qty_exceeds_limit")

        signed_qty = intent.qty if intent.side.value == "buy" else -intent.qty
        next_position = portfolio.position + signed_qty
        if abs(next_position) > self.config.max_position_abs:
            return RiskDecision(False, "position_limit_exceeded")

        if portfolio.realized_pnl <= -abs(self.config.max_daily_loss):
            return RiskDecision(False, "daily_loss_limit_reached")

        if self.config.trading_mode == "live" and not self.config.live_allowed:
            return RiskDecision(False, "live_mode_blocked_by_safety_gate")

        return RiskDecision(True, "allowed")
