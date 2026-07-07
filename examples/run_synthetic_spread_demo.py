from __future__ import annotations

from datetime import datetime, timedelta, timezone
from math import sin

from trading_robot_lab.backtest.simple_backtester import SimpleBacktester
from trading_robot_lab.core.config import AppConfig
from trading_robot_lab.core.models import MarketSnapshot
from trading_robot_lab.strategy.synthetic_spread import SyntheticSpreadStrategy


def build_demo_snapshots() -> list[MarketSnapshot]:
    """Generate synthetic demo data.

    This is fake data for checking that the architecture works end-to-end.
    It is not market data and must not be used to evaluate profitability.
    """

    start = datetime(2026, 1, 1, tzinfo=timezone.utc)
    snapshots: list[MarketSnapshot] = []

    for i in range(250):
        synthetic_index = 100_000 + i * 1.5
        normal_noise = sin(i / 7) * 30
        temporary_dislocation = 0

        if 90 <= i <= 105:
            temporary_dislocation = 220
        elif 160 <= i <= 175:
            temporary_dislocation = -220

        futures_price = synthetic_index + normal_noise + temporary_dislocation
        snapshots.append(
            MarketSnapshot(
                ts=start + timedelta(seconds=i),
                futures_price=futures_price,
                synthetic_index=synthetic_index,
            )
        )

    return snapshots


def main() -> None:
    config = AppConfig(
        trading_mode="backtest",
        live_trading_enabled=False,
        max_position_abs=1,
        max_order_qty=1,
        max_daily_loss=1000,
    )
    strategy = SyntheticSpreadStrategy(window=50, entry_z_score=2.0, exit_z_score=0.4)
    backtester = SimpleBacktester(config=config, strategy=strategy)
    result = backtester.run(build_demo_snapshots())

    print("Final position:", result.final_portfolio.position)
    print("Realized PnL:", round(result.final_portfolio.realized_pnl, 2))
    print("Events:")
    for event in result.events:
        print(event)


if __name__ == "__main__":
    main()
