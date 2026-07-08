# seed_demo_db.py — populate Trading Lab demo SQLite database
# Usage: python apps/lab/backend/seed_demo_db.py
# Requirements: Python standard library only

import sqlite3
import os
from pathlib import Path
from datetime import datetime, timezone

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

    conn.commit()

    print("=== Trading Lab Demo Database ===")
    tables = ["project_status", "schemas", "strategy_packages", "test_vectors",
              "mimo_reports", "data_sources", "backtest_runs", "runtime_status"]
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
