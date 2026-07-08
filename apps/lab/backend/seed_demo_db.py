# seed_demo_db.py — populate Trading Lab demo SQLite database
# Usage: python apps/lab/backend/seed_demo_db.py
# Requirements: Python standard library only

import sqlite3
import os
import math
import random
from pathlib import Path
from datetime import datetime, timezone, timedelta

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent
DB_PATH = REPO_ROOT / "data" / "lab" / "trading_lab_demo.sqlite"


def create_tables(cursor):
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS project_status (
            id INTEGER PRIMARY KEY,
            project_name TEXT NOT NULL,
            project_version TEXT NOT NULL,
            repo TEXT NOT NULL,
            live_enabled INTEGER NOT NULL DEFAULT 0,
            live_status TEXT NOT NULL,
            runtime_implemented INTEGER NOT NULL DEFAULT 0,
            runtime_status TEXT NOT NULL,
            database_type TEXT NOT NULL,
            database_status TEXT NOT NULL,
            environment TEXT NOT NULL,
            last_updated TEXT NOT NULL
        )
    """)

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS schemas (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL UNIQUE,
            version TEXT NOT NULL,
            file_path TEXT NOT NULL,
            description TEXT NOT NULL,
            pipeline_order INTEGER NOT NULL,
            required_fields TEXT NOT NULL,
            validation_status TEXT NOT NULL
        )
    """)

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS strategy_packages (
            id INTEGER PRIMARY KEY,
            strategy_id TEXT NOT NULL UNIQUE,
            strategy_version TEXT NOT NULL,
            author TEXT NOT NULL,
            created_at TEXT NOT NULL,
            allowed_modes TEXT NOT NULL,
            live_approved INTEGER NOT NULL DEFAULT 0,
            backtest_approved INTEGER NOT NULL DEFAULT 0,
            paper_approved INTEGER NOT NULL DEFAULT 0,
            status TEXT NOT NULL,
            description TEXT NOT NULL
        )
    """)

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS test_vectors (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL UNIQUE,
            description TEXT NOT NULL,
            schema_validated INTEGER NOT NULL DEFAULT 0,
            file_count INTEGER NOT NULL,
            files TEXT NOT NULL,
            status TEXT NOT NULL
        )
    """)

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS mimo_reports (
            id INTEGER PRIMARY KEY,
            report_id TEXT NOT NULL,
            title TEXT NOT NULL,
            issue TEXT NOT NULL,
            agent TEXT NOT NULL,
            completed_at TEXT NOT NULL,
            status TEXT NOT NULL,
            description TEXT NOT NULL
        )
    """)

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS data_sources (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL UNIQUE,
            source_type TEXT NOT NULL,
            status TEXT NOT NULL,
            description TEXT NOT NULL,
            instruments TEXT NOT NULL,
            date_range TEXT NOT NULL
        )
    """)

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS backtest_runs (
            id INTEGER PRIMARY KEY,
            run_id TEXT NOT NULL UNIQUE,
            strategy_id TEXT NOT NULL,
            started_at TEXT NOT NULL,
            completed_at TEXT,
            status TEXT NOT NULL,
            total_trades INTEGER NOT NULL DEFAULT 0,
            pnl REAL,
            max_drawdown REAL,
            win_rate REAL,
            notes TEXT NOT NULL
        )
    """)

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS runtime_status (
            id INTEGER PRIMARY KEY,
            component TEXT NOT NULL UNIQUE,
            status TEXT NOT NULL,
            description TEXT NOT NULL
        )
    """)

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS demo_price_series (
            id INTEGER PRIMARY KEY,
            instrument TEXT NOT NULL,
            strategy_id TEXT NOT NULL,
            ts TEXT NOT NULL,
            price REAL NOT NULL,
            volume INTEGER NOT NULL DEFAULT 0
        )
    """)

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS demo_equity_curve (
            id INTEGER PRIMARY KEY,
            strategy_id TEXT NOT NULL,
            ts TEXT NOT NULL,
            equity REAL NOT NULL
        )
    """)

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS demo_drawdown (
            id INTEGER PRIMARY KEY,
            strategy_id TEXT NOT NULL,
            ts TEXT NOT NULL,
            drawdown_pct REAL NOT NULL
        )
    """)

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS demo_trades (
            id INTEGER PRIMARY KEY,
            trade_id TEXT NOT NULL UNIQUE,
            strategy_id TEXT NOT NULL,
            instrument TEXT NOT NULL,
            side TEXT NOT NULL,
            entry_ts TEXT NOT NULL,
            entry_price REAL NOT NULL,
            exit_ts TEXT,
            exit_price REAL,
            quantity INTEGER NOT NULL,
            pnl REAL,
            status TEXT NOT NULL
        )
    """)

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS qsh_quality (
            id INTEGER PRIMARY KEY,
            file_name TEXT NOT NULL,
            sha256 TEXT NOT NULL,
            stream_type TEXT NOT NULL,
            instrument TEXT NOT NULL,
            recording_time TEXT NOT NULL,
            records_total INTEGER NOT NULL DEFAULT 0,
            records_valid INTEGER NOT NULL DEFAULT 0,
            records_rejected INTEGER NOT NULL DEFAULT 0,
            timestamp_min INTEGER NOT NULL DEFAULT 0,
            timestamp_max INTEGER NOT NULL DEFAULT 0,
            add_count INTEGER NOT NULL DEFAULT 0,
            fill_count INTEGER NOT NULL DEFAULT 0,
            cancel_count INTEGER NOT NULL DEFAULT 0,
            remove_count INTEGER NOT NULL DEFAULT 0,
            tx_count INTEGER NOT NULL DEFAULT 0,
            buy_count INTEGER NOT NULL DEFAULT 0,
            sell_count INTEGER NOT NULL DEFAULT 0,
            book_errors INTEGER NOT NULL DEFAULT 0,
            status TEXT NOT NULL,
            notes TEXT NOT NULL DEFAULT ''
        )
    """)


def seed_project_status(cursor):
    cursor.execute("""
        INSERT OR REPLACE INTO project_status
        (id, project_name, project_version, repo, live_enabled, live_status,
         runtime_implemented, runtime_status, database_type, database_status,
         environment, last_updated)
        VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, (
        "trading-robot-lab",
        "0.1.0",
        "dvrk0726/trading-robot-lab",
        0, "DISABLED",
        0, "NOT_IMPLEMENTED",
        "SQLite", "DEMO",
        "development",
        datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    ))


def seed_schemas(cursor):
    schemas = [
        ("MarketEvent", "1.0.0", "shared/schemas/market_event.schema.json",
         "Single market data event from exchange or replay stream",
         1, "event_id, timestamp_ns, instrument_id, event_type", "VALID"),
        ("FeatureSnapshot", "1.0.0", "shared/schemas/feature_snapshot.schema.json",
         "Computed feature values at a point in time",
         2, "snapshot_id, timestamp_ns, instrument_id, strategy_id, features", "VALID"),
        ("StrategySignal", "1.0.0", "shared/schemas/strategy_signal.schema.json",
         "Output signal from a TradeAgent",
         3, "signal_id, timestamp_ns, instrument_id, strategy_id, signal_type", "VALID"),
        ("OrderIntent", "1.0.0", "shared/schemas/order_intent.schema.json",
         "Order intent derived from a StrategySignal, must pass RiskEngine",
         4, "order_id, timestamp_ns, instrument_id, strategy_id, signal_id, side, order_type, quantity", "VALID"),
        ("RiskDecision", "1.0.0", "shared/schemas/risk_decision.schema.json",
         "RiskEngine output for an OrderIntent",
         5, "decision_id, timestamp_ns, order_id, decision", "VALID"),
    ]
    for s in schemas:
        cursor.execute("""
            INSERT OR REPLACE INTO schemas
            (name, version, file_path, description, pipeline_order, required_fields, validation_status)
            VALUES (?, ?, ?, ?, ?, ?, ?)
        """, s)


def seed_strategy_packages(cursor):
    cursor.execute("""
        INSERT OR REPLACE INTO strategy_packages
        (strategy_id, strategy_version, author, created_at, allowed_modes,
         live_approved, backtest_approved, paper_approved, status, description)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, (
        "dummy_no_trade_v001",
        "0.1.0",
        "mimo-agent",
        "2026-07-08",
        "research, backtest, replay",
        0, 1, 0,
        "demo",
        "No-trade package for testing Runtime loading, validation, and rejection logic"
    ))


def seed_test_vectors(cursor):
    cursor.execute("""
        INSERT OR REPLACE INTO test_vectors
        (name, description, schema_validated, file_count, files, status)
        VALUES (?, ?, ?, ?, ?, ?)
    """, (
        "basic_flow",
        "Full pipeline test: MarketEvent -> FeatureSnapshot -> StrategySignal -> OrderIntent -> RiskDecision",
        1, 5,
        "01_market_event.json, 02_feature_snapshot.json, 03_strategy_signal.json, 04_order_intent.json, 05_risk_decision.json",
        "ALL_VALID"
    ))


def seed_mimo_reports(cursor):
    reports = [
        ("initial-context-read", "Initial MiMo Context Read", "-", "mimo-agent",
         "2026-07-08", "completed", "Read AI_CONTEXT, PROJECT_STATE, ROADMAP, SECURITY"),
        ("issue-002", "Project Skeleton", "#2", "mimo-agent",
         "2026-07-08", "completed", "Created apps/lab, apps/runtime, shared, strategy_packages directories"),
        ("issue-006", "Shared Contracts", "#6", "mimo-agent",
         "2026-07-08", "completed", "Created 5 JSON schemas for core pipeline contracts"),
        ("issue-007", "Test Vectors and Validation", "#7", "mimo-agent",
         "2026-07-08", "completed", "Created basic_flow test vectors and validate_examples.py"),
        ("issue-008", "Strategy Package Standard", "#8", "mimo-agent",
         "2026-07-08", "completed", "Defined Strategy Package standard and created dummy_no_trade_v001"),
        ("m9-cpp-qsh", "C++ QSH/OrdLog Data Layer", "#13", "mimo-agent",
         "2026-07-08", "completed", "Built C++20 qsh_ingest engine: QSH parser, OrdLog reader, L3->L2 book, Data Quality"),
    ]
    for r in reports:
        cursor.execute("""
            INSERT OR REPLACE INTO mimo_reports
            (report_id, title, issue, agent, completed_at, status, description)
            VALUES (?, ?, ?, ?, ?, ?, ?)
        """, r)


def seed_data_sources(cursor):
    sources = [
        ("demo_synthetic", "synthetic", "active",
         "Synthetic demo data for testing dashboard and pipeline",
         "demo", "N/A"),
        ("test_vectors", "synthetic", "active",
         "Schema validation test vectors in shared/test_vectors/basic_flow",
         "RIU5", "N/A"),
        ("qsh_ordlog", "historical", "engine_ready",
         "C++ qsh_ingest engine for QSH v4 OrdLog/Quotes/Deals/AuxInfo parsing",
         "RTS-3.21", "2021-01-05"),
        ("moex_historical", "exchange", "not_connected",
         "MOEX historical data feed (not implemented)",
         "RI, SBER, IMOEX", "N/A"),
        ("paper_feed", "simulated", "not_implemented",
         "Paper trading data feed (not implemented)",
         "N/A", "N/A"),
    ]
    for s in sources:
        cursor.execute("""
            INSERT OR REPLACE INTO data_sources
            (name, source_type, status, description, instruments, date_range)
            VALUES (?, ?, ?, ?, ?, ?)
        """, s)


def seed_runtime_status(cursor):
    components = [
        ("Live Trading", "DISABLED",
         "No broker connection. No real order sending code. live_approved=false."),
        ("Broker Adapter", "NOT_IMPLEMENTED",
         "No broker adapter exists. Cannot send orders to any exchange or broker."),
        ("Risk Engine", "NOT_IMPLEMENTED",
         "RiskEngine not implemented yet. Required before any execution."),
        ("Runtime Core", "NOT_IMPLEMENTED",
         "Strategy Package loader and runtime core not implemented."),
        ("Order Manager", "NOT_IMPLEMENTED",
         "Order lifecycle management not implemented."),
        ("Telemetry", "NOT_IMPLEMENTED",
         "Runtime telemetry and monitoring not implemented."),
        ("Execution Gateway", "NOT_IMPLEMENTED",
         "Execution gateway not implemented. No order routing."),
    ]
    for c in components:
        cursor.execute("""
            INSERT OR REPLACE INTO runtime_status
            (component, status, description)
            VALUES (?, ?, ?)
        """, c)


def seed_demo_price_series(cursor):
    """Generate 75 synthetic price points for RI_demo using random walk."""
    random.seed(42)
    base_price = 140000.0
    strategy_id = "dummy_visual_strategy"
    instrument = "RI_demo"
    start = datetime(2026, 7, 1, 10, 0, 0, tzinfo=timezone.utc)
    price = base_price
    points = []
    for i in range(75):
        ts = (start + timedelta(minutes=i * 5)).strftime("%Y-%m-%dT%H:%M:%SZ")
        change = random.gauss(0, 150)
        price = max(base_price * 0.95, min(base_price * 1.05, price + change))
        volume = random.randint(100, 500)
        points.append((instrument, strategy_id, ts, round(price, 2), volume))
    for p in points:
        cursor.execute("""
            INSERT OR REPLACE INTO demo_price_series
            (instrument, strategy_id, ts, price, volume)
            VALUES (?, ?, ?, ?, ?)
        """, p)


def seed_demo_equity_curve(cursor):
    """Generate 75 synthetic equity curve points starting at 1_000_000."""
    random.seed(42)
    strategy_id = "dummy_visual_strategy"
    start = datetime(2026, 7, 1, 10, 0, 0, tzinfo=timezone.utc)
    equity = 1000000.0
    points = []
    for i in range(75):
        ts = (start + timedelta(minutes=i * 5)).strftime("%Y-%m-%dT%H:%M:%SZ")
        drift = random.gauss(800, 2000)
        equity = max(900000, equity + drift)
        points.append((strategy_id, ts, round(equity, 2)))
    for p in points:
        cursor.execute("""
            INSERT OR REPLACE INTO demo_equity_curve
            (strategy_id, ts, equity)
            VALUES (?, ?, ?)
        """, p)


def seed_demo_drawdown(cursor):
    """Compute drawdown from the seeded equity curve."""
    random.seed(42)
    strategy_id = "dummy_visual_strategy"
    start = datetime(2026, 7, 1, 10, 0, 0, tzinfo=timezone.utc)
    equity = 1000000.0
    peak = equity
    points = []
    for i in range(75):
        ts = (start + timedelta(minutes=i * 5)).strftime("%Y-%m-%dT%H:%M:%SZ")
        drift = random.gauss(800, 2000)
        equity = max(900000, equity + drift)
        if equity > peak:
            peak = equity
        dd_pct = ((equity - peak) / peak) * 100 if peak > 0 else 0.0
        points.append((strategy_id, ts, round(dd_pct, 4)))
    for p in points:
        cursor.execute("""
            INSERT OR REPLACE INTO demo_drawdown
            (strategy_id, ts, drawdown_pct)
            VALUES (?, ?, ?)
        """, p)


def seed_demo_trades(cursor):
    """Seed 8 demo trades with entry/exit markers.

    Trade entry/exit timestamps must match timestamps in demo_price_series
    so that markers are placed at exact data point coordinates on the chart.
    Price series: 2026-07-01T10:00:00Z every 5 min, 75 points -> ends at 16:10.
    """
    strategy_id = "dummy_visual_strategy"
    instrument = "RI_demo"
    trades = [
        ("DEMO-001", "BUY",  "2026-07-01T10:05:00Z", 139850.0, "2026-07-01T11:30:00Z", 140200.0,  2, 700.0,   "closed"),
        ("DEMO-002", "SELL", "2026-07-01T12:00:00Z", 140400.0, "2026-07-01T13:15:00Z", 140100.0,  1, 300.0,   "closed"),
        ("DEMO-003", "BUY",  "2026-07-01T14:00:00Z", 139900.0, "2026-07-01T15:00:00Z", 139700.0,  3, -600.0,  "closed"),
        ("DEMO-004", "BUY",  "2026-07-01T15:30:00Z", 139600.0, "2026-07-01T16:00:00Z", 140050.0,  2, 900.0,   "closed"),
        ("DEMO-005", "SELL", "2026-07-01T10:30:00Z", 140100.0, "2026-07-01T11:05:00Z", 140350.0,  1, -250.0,  "closed"),
        ("DEMO-006", "BUY",  "2026-07-01T12:30:00Z", 140200.0, None,                   None,      2, None,    "open"),
        ("DEMO-007", "SELL", "2026-07-01T13:45:00Z", 140500.0, "2026-07-01T14:30:00Z", 140150.0,  1, 350.0,   "closed"),
        ("DEMO-008", "BUY",  "2026-07-01T15:05:00Z", 140050.0, None,                   None,      3, None,    "open"),
    ]
    for t in trades:
        cursor.execute("""
            INSERT OR REPLACE INTO demo_trades
            (trade_id, strategy_id, instrument, side, entry_ts, entry_price,
             exit_ts, exit_price, quantity, pnl, status)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (t[0], strategy_id, instrument, t[1], t[2], t[3], t[4], t[5], t[6], t[7], t[8]))


def seed_qsh_quality(cursor):
    """Seed placeholder for QSH quality reports generated by C++ qsh_ingest."""
    sources = [
        ("RTS-3.21.2021-01-05.OrdLog.qsh", "placeholder", "OrderLog", "RTS-3.21",
         "2021-01-05", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         "awaiting_cpp_ingest",
         "Run: qsh-ingest quality <file.qsh> to populate"),
    ]
    for s in sources:
        cursor.execute("""
            INSERT OR REPLACE INTO qsh_quality
            (file_name, sha256, stream_type, instrument, recording_time,
             records_total, records_valid, records_rejected, timestamp_min, timestamp_max,
             add_count, fill_count, cancel_count, remove_count, tx_count,
             buy_count, sell_count, book_errors, status, notes)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, s)


def main():
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)

    if DB_PATH.exists():
        os.remove(DB_PATH)

    conn = sqlite3.connect(str(DB_PATH))
    cursor = conn.cursor()

    create_tables(cursor)
    seed_project_status(cursor)
    seed_schemas(cursor)
    seed_strategy_packages(cursor)
    seed_test_vectors(cursor)
    seed_mimo_reports(cursor)
    seed_data_sources(cursor)
    seed_runtime_status(cursor)
    seed_demo_price_series(cursor)
    seed_demo_equity_curve(cursor)
    seed_demo_drawdown(cursor)
    seed_demo_trades(cursor)
    seed_qsh_quality(cursor)

    conn.commit()

    print("=== Trading Lab Demo Database ===")
    tables = ["project_status", "schemas", "strategy_packages", "test_vectors",
              "mimo_reports", "data_sources", "backtest_runs", "runtime_status",
              "demo_price_series", "demo_equity_curve", "demo_drawdown", "demo_trades",
              "qsh_quality"]
    for table in tables:
        count = cursor.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
        print(f"  {table}: {count} rows")

    conn.close()

    print()
    print(f"Database created: {DB_PATH}")
    print()
    print("Run the dashboard:")
    print("  python apps/lab/backend/lab_dashboard.py")
    print("  Open: http://127.0.0.1:8000")


if __name__ == "__main__":
    main()
