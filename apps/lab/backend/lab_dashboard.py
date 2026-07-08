#!/usr/bin/env python3
"""Trading Lab — local demo dashboard.

Runs a local HTTP server reading from the demo SQLite database.
Uses only Python standard library.

Usage:
    python apps/lab/backend/lab_dashboard.py
    Open: http://127.0.0.1:8000

Requirements: Python 3.8+, seed_demo_db.py must be run first.
"""

import sqlite3
import http.server
import socketserver
import json
import os
from pathlib import Path
from urllib.parse import parse_qs, urlparse
from datetime import datetime, timezone

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent
DB_PATH = REPO_ROOT / "data" / "lab" / "trading_lab_demo.sqlite"
CSS_PATH = REPO_ROOT / "apps" / "lab" / "frontend" / "static" / "lab.css"
PORT = 8000


# ---------------------------------------------------------------------------
# HTML Templates
# ---------------------------------------------------------------------------

PAGE_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Trading Lab — {page_title}</title>
<link rel="stylesheet" href="/lab.css">
</head>
<body>
<div class="header">
  <h1>Trading Lab — {page_title}</h1>
  <div class="header-badges">
    <span class="badge-live">LIVE DISABLED</span>
    <span class="badge-demo">DEMO DATA ONLY</span>
    <span class="badge-runtime">NO BROKER</span>
  </div>
</div>
<div class="layout">
<div class="nav">
  <div class="nav-title">Sections</div>
  {nav_links}
</div>
<div class="content">
{content}
</div>
</div>
<div class="footer">
  trading-robot-lab | Demo Dashboard | No real market data | No broker | No secrets | {timestamp}
</div>
</body>
</html>"""

NAV_ITEMS = [
    ("overview", "Overview"),
    ("schemas", "Schemas"),
    ("packages", "Strategy Packages"),
    ("vectors", "Test Vectors"),
    ("reports", "MiMo Reports"),
    ("sources", "Data Sources"),
    ("backtests", "Backtests"),
    ("runtime", "Runtime Status"),
]


def esc(text):
    """Escape HTML special characters."""
    if text is None:
        return ""
    return (str(text)
            .replace("&", "&amp;")
            .replace("<", "&lt;")
            .replace(">", "&gt;")
            .replace('"', "&quot;"))


def status_badge(status):
    """Return HTML span with appropriate status badge class."""
    s = str(status).lower().replace(" ", "_").replace("-", "_")
    cls = "status-badge"
    if s in ("valid", "active", "completed", "allowed", "all_valid"):
        cls += " status-valid"
    elif s in ("disabled", "rejected", "not_connected", "not_implemented", "not_started"):
        cls += " status-disabled"
    elif s in ("demo", "not_connected"):
        cls += " status-demo"
    else:
        cls += " status-demo"
    return f'<span class="{cls}">{esc(status)}</span>'


def build_nav(active):
    """Build sidebar navigation HTML."""
    parts = []
    for key, label in NAV_ITEMS:
        css = ' class="active"' if key == active else ""
        parts.append(f'<a href="/?section={key}"{css}>{esc(label)}</a>')
    return "\n  ".join(parts)


def wrap_page(title, section, content):
    """Wrap content in full page template."""
    return PAGE_TEMPLATE.format(
        page_title=esc(title),
        nav_links=build_nav(section),
        content=content,
        timestamp=datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S UTC"),
    )


# ---------------------------------------------------------------------------
# Database Helpers
# ---------------------------------------------------------------------------

def get_db():
    """Open a new database connection."""
    return sqlite3.connect(str(DB_PATH))


def fetch_one(query, params=()):
    """Fetch a single row as dict."""
    conn = get_db()
    conn.row_factory = sqlite3.Row
    row = conn.execute(query, params).fetchone()
    result = dict(row) if row else None
    conn.close()
    return result


def fetch_all(query, params=()):
    """Fetch all rows as list of dicts."""
    conn = get_db()
    conn.row_factory = sqlite3.Row
    rows = conn.execute(query, params).fetchall()
    result = [dict(r) for r in rows]
    conn.close()
    return result


# ---------------------------------------------------------------------------
# Section Renderers
# ---------------------------------------------------------------------------

def render_overview():
    status = fetch_one("SELECT * FROM project_status WHERE id = 1")
    schema_count = fetch_one("SELECT COUNT(*) AS c FROM schemas")["c"]
    pkg_count = fetch_one("SELECT COUNT(*) AS c FROM strategy_packages")["c"]
    vec_count = fetch_one("SELECT COUNT(*) AS c FROM test_vectors")["c"]
    report_count = fetch_one("SELECT COUNT(*) AS c FROM mimo_reports")["c"]
    src_count = fetch_one("SELECT COUNT(*) AS c FROM data_sources")["c"]
    bt_count = fetch_one("SELECT COUNT(*) AS c FROM backtest_runs")["c"]
    rules = [
        "No broker connection",
        "No live trading",
        "No real API keys",
        "No secrets",
        "No real order sending code",
        "No bypassing RiskEngine",
        "No live_approved=true",
        "No external paid dependencies unless clearly documented",
    ]
    rules_html = "".join(f"<li>{esc(r)}</li>" for r in rules)
    return f"""
<h2 class="section-title">Overview</h2>
<div class="card-grid">
  <div class="card">
    <div class="card-title">Live Status</div>
    <div class="card-value">{status_badge(status['live_status'])}</div>
    <div class="card-detail">live_enabled = {status['live_enabled']} | LIVE DISABLED</div>
  </div>
  <div class="card">
    <div class="card-title">Runtime</div>
    <div class="card-value">{status_badge(status['runtime_status'])}</div>
    <div class="card-detail">Not implemented. No execution gateway.</div>
  </div>
  <div class="card">
    <div class="card-title">Database</div>
    <div class="card-value">{esc(status['database_type'])}</div>
    <div class="card-detail">Status: {status_badge(status['database_status'])}</div>
  </div>
  <div class="card">
    <div class="card-title">Environment</div>
    <div class="card-value">{esc(status['environment'])}</div>
    <div class="card-detail">Version: {esc(status['project_version'])}</div>
  </div>
</div>
<div class="stat-grid">
  <div class="stat-card"><span class="stat-number">{schema_count}</span><span class="stat-label">Schemas</span></div>
  <div class="stat-card"><span class="stat-number">{pkg_count}</span><span class="stat-label">Strategy Packages</span></div>
  <div class="stat-card"><span class="stat-number">{vec_count}</span><span class="stat-label">Test Vectors</span></div>
  <div class="stat-card"><span class="stat-number">{report_count}</span><span class="stat-label">MiMo Reports</span></div>
  <div class="stat-card"><span class="stat-number">{src_count}</span><span class="stat-label">Data Sources</span></div>
  <div class="stat-card"><span class="stat-number">{bt_count}</span><span class="stat-label">Backtest Runs</span></div>
</div>
<div class="pipeline-flow">
  <span class="pipeline-step">MarketEvent</span><span class="pipeline-arrow">-></span>
  <span class="pipeline-step">FeatureSnapshot</span><span class="pipeline-arrow">-></span>
  <span class="pipeline-step">StrategySignal</span><span class="pipeline-arrow">-></span>
  <span class="pipeline-step">OrderIntent</span><span class="pipeline-arrow">-></span>
  <span class="pipeline-step">RiskDecision</span>
</div>
<h3 class="subsection-title">Safety Rules</h3>
<ul class="rules-list">{rules_html}</ul>
"""


def render_schemas():
    schemas = fetch_all("SELECT * FROM schemas ORDER BY pipeline_order")
    rows = ""
    for s in schemas:
        rows += f"""<tr>
  <td>{esc(s['pipeline_order'])}</td>
  <td><code>{esc(s['name'])}</code></td>
  <td>{esc(s['version'])}</td>
  <td>{esc(s['description'])}</td>
  <td><code>{esc(s['file_path'])}</code></td>
  <td>{status_badge(s['validation_status'])}</td>
</tr>"""
    return f"""
<h2 class="section-title">Schemas — Shared Contracts</h2>
<p style="color:var(--text-secondary);margin-bottom:16px;font-size:12px">
  Core data pipeline contracts shared between Trading Lab and Trading Runtime.
  All schemas use JSON Schema draft-07. Validated by
  <code>python shared/schemas/validate_examples.py</code>.
</p>
<div class="pipeline-flow">
  <span class="pipeline-step">MarketEvent</span><span class="pipeline-arrow">-></span>
  <span class="pipeline-step">FeatureSnapshot</span><span class="pipeline-arrow">-></span>
  <span class="pipeline-step">StrategySignal</span><span class="pipeline-arrow">-></span>
  <span class="pipeline-step">OrderIntent</span><span class="pipeline-arrow">-></span>
  <span class="pipeline-step">RiskDecision</span>
</div>
<table>
<thead><tr>
  <th>#</th><th>Schema</th><th>Version</th><th>Description</th><th>File</th><th>Status</th>
</tr></thead>
<tbody>{rows}</tbody>
</table>
"""


def render_packages():
    packages = fetch_all("SELECT * FROM strategy_packages")
    rows = ""
    for p in packages:
        rows += f"""<tr>
  <td><code>{esc(p['strategy_id'])}</code></td>
  <td>{esc(p['strategy_version'])}</td>
  <td>{esc(p['author'])}</td>
  <td>{esc(p['allowed_modes'])}</td>
  <td>{status_badge("ALLOWED" if p['backtest_approved'] else "REJECTED")}</td>
  <td>{status_badge("DISABLED" if not p['live_approved'] else "ALLOWED")}</td>
  <td>{status_badge(p['status'])}</td>
</tr>"""
    return f"""
<h2 class="section-title">Strategy Packages</h2>
<p style="color:var(--text-secondary);margin-bottom:16px;font-size:12px">
  Strategy Package is the only allowed way to move a strategy from Lab to Runtime.
  Every package must pass validation, hash check, and have <code>live_approved=false</code>.
</p>
<table>
<thead><tr>
  <th>Strategy ID</th><th>Version</th><th>Author</th><th>Allowed Modes</th>
  <th>Backtest</th><th>Live</th><th>Status</th>
</tr></thead>
<tbody>{rows}</tbody>
</table>
"""


def render_vectors():
    vectors = fetch_all("SELECT * FROM test_vectors")
    rows = ""
    for v in vectors:
        files_list = v['files'].split(", ")
        files_html = "<br>".join(f"<code>{esc(f)}</code>" for f in files_list)
        rows += f"""<tr>
  <td><code>{esc(v['name'])}</code></td>
  <td>{esc(v['description'])}</td>
  <td>{v['file_count']}</td>
  <td style="font-size:11px">{files_html}</td>
  <td>{status_badge(v['status'])}</td>
</tr>"""
    return f"""
<h2 class="section-title">Test Vectors</h2>
<p style="color:var(--text-secondary);margin-bottom:16px;font-size:12px">
  Synthetic test data for the core pipeline.
  Validated by <code>python shared/schemas/validate_examples.py</code>.
</p>
<table>
<thead><tr>
  <th>Name</th><th>Description</th><th>Files</th><th>File List</th><th>Status</th>
</tr></thead>
<tbody>{rows}</tbody>
</table>
"""


def render_reports():
    reports = fetch_all("SELECT * FROM mimo_reports ORDER BY id")
    rows = ""
    for r in reports:
        rows += f"""<tr>
  <td>{esc(r['report_id'])}</td>
  <td>{esc(r['title'])}</td>
  <td>{esc(r['issue'])}</td>
  <td>{esc(r['agent'])}</td>
  <td>{esc(r['completed_at'])}</td>
  <td>{status_badge(r['status'])}</td>
  <td style="font-size:11px">{esc(r['description'])}</td>
</tr>"""
    return f"""
<h2 class="section-title">MiMo Reports</h2>
<p style="color:var(--text-secondary);margin-bottom:16px;font-size:12px">
  Work completed by MiMo agent. Full reports in
  <code>agent_workspaces/mimo/reports/</code>.
</p>
<table>
<thead><tr>
  <th>Report ID</th><th>Title</th><th>Issue</th><th>Agent</th>
  <th>Date</th><th>Status</th><th>Description</th>
</tr></thead>
<tbody>{rows}</tbody>
</table>
"""


def render_sources():
    sources = fetch_all("SELECT * FROM data_sources")
    rows = ""
    for s in sources:
        rows += f"""<tr>
  <td><code>{esc(s['name'])}</code></td>
  <td>{esc(s['source_type'])}</td>
  <td>{esc(s['description'])}</td>
  <td>{esc(s['instruments'])}</td>
  <td>{esc(s['date_range'])}</td>
  <td>{status_badge(s['status'])}</td>
</tr>"""
    return f"""
<h2 class="section-title">Data Sources</h2>
<p style="color:var(--text-secondary);margin-bottom:16px;font-size:12px">
  Demo/synthetic data only. No real market data connected. No broker feeds.
</p>
<table>
<thead><tr>
  <th>Name</th><th>Type</th><th>Description</th>
  <th>Instruments</th><th>Date Range</th><th>Status</th>
</tr></thead>
<tbody>{rows}</tbody>
</table>
"""


def render_backtests():
    runs = fetch_all("SELECT * FROM backtest_runs ORDER BY id")
    if not runs:
        rows = '<tr><td colspan="7" style="text-align:center;color:var(--text-muted);padding:20px">No backtest runs yet.</td></tr>'
    else:
        rows = ""
        for r in runs:
            rows += f"""<tr>
  <td>{esc(r['run_id'])}</td>
  <td>{esc(r['strategy_id'])}</td>
  <td>{esc(r['started_at'])}</td>
  <td>{status_badge(r['status'])}</td>
  <td>{r['total_trades']}</td>
  <td>{r['pnl'] if r['pnl'] is not None else 'N/A'}</td>
  <td style="font-size:11px">{esc(r['notes'])}</td>
</tr>"""
    return f"""
<h2 class="section-title">Backtest Runs</h2>
<p style="color:var(--text-secondary);margin-bottom:16px;font-size:12px">
  No backtests executed yet. Will be populated after research data and strategy code are ready.
</p>
<table>
<thead><tr>
  <th>Run ID</th><th>Strategy</th><th>Started</th><th>Status</th>
  <th>Trades</th><th>PnL</th><th>Notes</th>
</tr></thead>
<tbody>{rows}</tbody>
</table>
"""


def render_runtime():
    components = fetch_all("SELECT * FROM runtime_status ORDER BY id")
    rows = ""
    for c in components:
        rows += f"""<tr>
  <td><strong>{esc(c['component'])}</strong></td>
  <td>{status_badge(c['status'])}</td>
  <td style="font-size:11px">{esc(c['description'])}</td>
</tr>"""
    return f"""
<h2 class="section-title">Runtime Status</h2>
<p style="color:var(--text-secondary);margin-bottom:16px;font-size:12px">
  Trading Runtime is not implemented. All components show NOT IMPLEMENTED or DISABLED.
</p>
<table>
<thead><tr>
  <th>Component</th><th>Status</th><th>Description</th>
</tr></thead>
<tbody>{rows}</tbody>
</table>
"""


SECTION_RENDERERS = {
    "overview": render_overview,
    "schemas": render_schemas,
    "packages": render_packages,
    "vectors": render_vectors,
    "reports": render_reports,
    "sources": render_sources,
    "backtests": render_backtests,
    "runtime": render_runtime,
}


# ---------------------------------------------------------------------------
# HTTP Handler
# ---------------------------------------------------------------------------

class LabDashboardHandler(http.server.BaseHTTPRequestHandler):
    """HTTP request handler for the Trading Lab dashboard."""

    def log_message(self, format, *args):
        """Suppress default access log for cleaner output."""
        pass

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path

        # Serve CSS
        if path == "/lab.css":
            self._serve_css()
            return

        # Parse section from query string
        query = parse_qs(parsed.query)
        section = query.get("section", ["overview"])[0]
        if section not in SECTION_RENDERERS:
            section = "overview"

        self._serve_page(section)

    def _serve_css(self):
        try:
            content = CSS_PATH.read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", "text/css; charset=utf-8")
            self.send_header("Content-Length", str(len(content)))
            self.end_headers()
            self.wfile.write(content)
        except FileNotFoundError:
            self.send_error(404, "CSS not found")

    def _serve_page(self, section):
        titles = {k: v for k, v in NAV_ITEMS}
        title = titles.get(section, "Dashboard")
        renderer = SECTION_RENDERERS.get(section, render_overview)
        content = renderer()
        html = wrap_page(title, section, content)
        encoded = html.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    if not DB_PATH.exists():
        print("ERROR: Demo database not found.")
        print()
        print("Run the seed script first:")
        print("  python apps/lab/backend/seed_demo_db.py")
        print()
        print(f"Expected database at: {DB_PATH}")
        return

    if not CSS_PATH.exists():
        print("WARNING: CSS file not found. Dashboard will render without styles.")
        print(f"  Expected: {CSS_PATH}")
        print()

    with socketserver.TCPServer(("127.0.0.1", PORT), LabDashboardHandler) as server:
        server.allow_reuse_address = True
        print("=" * 60)
        print("  Trading Lab — Demo Dashboard")
        print("=" * 60)
        print()
        print(f"  Status:     LIVE DISABLED")
        print(f"  Runtime:    NOT IMPLEMENTED")
        print(f"  Database:   DEMO (SQLite)")
        print(f"  Broker:     NONE")
        print(f"  Data:       DEMO / SYNTHETIC ONLY")
        print()
        print(f"  URL:  http://127.0.0.1:{PORT}")
        print()
        print("  Press Ctrl+C to stop.")
        print("=" * 60)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            print("\nDashboard stopped.")


if __name__ == "__main__":
    main()
