# M8_HFT

Do this instead of M8.md.

Goal: add a Trading Lab import skeleton for microstructure data. Do not use 1 minute candles as the main format.

Read first:
- AI_CONTEXT.md
- PROJECT_STATE.md
- SECURITY.md
- docs/current_mimo_workflow_and_state.md
- docs/mimo_developer_workflow.md
- strategy_knowledge_base/research_notes/NOTE-20260708-002-market-maker-algorithms-parts-1-4.md
- strategy_knowledge_base/research_notes/NOTE-20260708-003-market-maker-algorithms-parts-5-8.md
- apps/lab/backend/lab_dashboard.py
- apps/lab/backend/seed_demo_db.py

Create files:
- apps/lab/backend/import_hft_csv.py
- apps/lab/research/hft_market_data_format.md
- data/sample/README.md
- data/sample/ri_demo_trades.csv
- data/sample/ri_demo_quotes_l1.csv
- data/sample/ri_demo_book_l2.csv
- data/sample/ri_demo_book_events.csv

Synthetic CSV formats:

trades:
- ts_ns,instrument,trade_id,price,qty,side,aggressor_side

quotes_l1:
- ts_ns,instrument,bid_price,bid_qty,ask_price,ask_qty

book_l2:
- ts_ns,instrument,level,bid_price,bid_qty,ask_price,ask_qty

book_events:
- ts_ns,instrument,event_id,event_type,side,price,qty,ref_id

SQLite tables:
- market_trades
- market_quotes_l1
- market_book_l2
- market_book_events
- data_import_batches
- data_quality_checks

Importer requirements:
- read several CSV paths from command line
- detect type by columns
- validate required columns
- validate ts_ns integer
- validate price and qty numeric
- validate bid_price < ask_price
- validate book level >= 1
- reject bad rows
- insert valid rows into the right table
- write batch summary and quality checks

Dashboard:
- add Data Quality section
- show file, type, instrument, rows, rejected rows, ts_ns range, status, warnings
- update Data Sources with HFT sample sources

Expected workflow:
```powershell
python apps/lab/backend/seed_demo_db.py
python apps/lab/backend/import_hft_csv.py data/sample/ri_demo_trades.csv data/sample/ri_demo_quotes_l1.csv data/sample/ri_demo_book_l2.csv data/sample/ri_demo_book_events.csv
python apps/lab/backend/lab_dashboard.py
```

Rules:
- demo data only
- no broker integration
- no live mode
- no keys or secrets
- no external paid dependencies

Report:
agent_workspaces/mimo/reports/2026-07-08-issue-012-hft-market-data-import.md
