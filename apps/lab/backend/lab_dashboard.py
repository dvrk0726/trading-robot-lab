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
    ("charts", "Charts"),
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


# ---------------------------------------------------------------------------
# Chart Data Serialization
# ---------------------------------------------------------------------------

def _json_encode(obj):
    """Encode Python object to JSON string for embedding in HTML."""
    return json.dumps(obj, separators=(",", ":"))


# ---------------------------------------------------------------------------
# Interactive Canvas Price Chart
# ---------------------------------------------------------------------------

def _canvas_price_chart_html(prices, trades):
    """Generate interactive canvas-based price chart with controls."""
    if not prices:
        return '<p style="color:var(--text-muted)">No price data.</p>'

    price_data = [{"ts": p["ts"], "p": p["price"]} for p in prices]
    trade_data = []
    for t in trades:
        td = {
            "id": t["trade_id"],
            "side": t["side"],
            "entry_ts": t["entry_ts"],
            "entry_price": t["entry_price"],
            "qty": t["quantity"],
        }
        if t["exit_ts"] and t["exit_price"] is not None:
            td["exit_ts"] = t["exit_ts"]
            td["exit_price"] = t["exit_price"]
        if t["pnl"] is not None:
            td["pnl"] = t["pnl"]
        trade_data.append(td)

    price_json = _json_encode(price_data)
    trade_json = _json_encode(trade_data)

    controls = """
<div class="chart-controls">
  <div class="chart-controls-group">
    <button class="chart-ctrl-btn" id="chartZoomIn" title="Zoom in (W / scroll up)">+ Zoom</button>
    <button class="chart-ctrl-btn" id="chartZoomOut" title="Zoom out (S / scroll down)">- Zoom</button>
    <button class="chart-ctrl-btn" id="chartPanLeft" title="Pan left (A / left arrow)">&larr; Left</button>
    <button class="chart-ctrl-btn" id="chartPanRight" title="Pan right (D / right arrow)">Right &rarr;</button>
    <button class="chart-ctrl-btn" id="chartReset" title="Reset view (R)">Reset</button>
  </div>
  <div class="chart-controls-group">
    <label class="chart-ctrl-check"><input type="checkbox" id="chartAutoScale" checked> Auto Y</label>
    <label class="chart-ctrl-check"><input type="checkbox" id="chartShowTrades" checked> Trades</label>
    <label class="chart-ctrl-check"><input type="checkbox" id="chartShowExits" checked> Exits</label>
    <label class="chart-ctrl-check"><input type="checkbox" id="chartShowGrid" checked> Grid</label>
  </div>
  <div class="chart-controls-group chart-controls-hint">
    Drag to pan | Scroll to zoom | Hover for details
  </div>
</div>"""

    return f"""{controls}
<div class="chart-canvas-wrap">
  <canvas id="priceChartCanvas" width="900" height="400"></canvas>
  <div id="chartTooltip" class="chart-tooltip"></div>
  <div id="chartRangeSel" class="chart-range-sel"></div>
</div>
<script>
(function() {{
  var PRICE_DATA = {price_json};
  var TRADE_DATA = {trade_json};
  var canvas = document.getElementById('priceChartCanvas');
  var ctx = canvas.getContext('2d');
  var tooltip = document.getElementById('chartTooltip');
  var rangeSel = document.getElementById('chartRangeSel');
  var W, H, ML=80, MR=30, MT=40, MB=50, PW, PH;
  var C = {{
    bg: '#0f1117', plotBg: '#12141c', grid: '#1e2030', gridMaj: '#252840',
    line: '#4a9eff', text: '#8a8f9e', textBright: '#c8ccd8',
    buy: '#33cc66', sell: '#ff4d4d', exit: '#ffaa33',
    fill: 'rgba(74,158,255,0.06)', crosshair: '#4a9eff40',
    font: '11px Consolas,Monaco,monospace',
    fontSmall: '10px Consolas,Monaco,monospace'
  }};
  var tsMin = PRICE_DATA[0].ts, tsMax = PRICE_DATA[PRICE_DATA.length-1].ts;
  var allPrices = PRICE_DATA.map(function(d) {{ return d.p; }});
  var pMin0 = Math.min.apply(null, allPrices), pMax0 = Math.max.apply(null, allPrices);
  var pPad = (pMax0 - pMin0) * 0.05 || 100;
  var view = {{ ts0: tsMin, ts1: tsMax, p0: pMin0 - pPad, p1: pMax0 + pPad }};
  var defs = {{ ts0: tsMin, ts1: tsMax, p0: pMin0 - pPad, p1: pMax0 + pPad }};
  var showExits = true, showTrades = true, showGrid = true, autoScale = true;
  var mouseX = -1, mouseY = -1, dragging = false, dragX0, dragTs0, dragTs1;
  var rangeDragging = false, rangeX0;

  function resize() {{
    var r = canvas.parentElement.getBoundingClientRect();
    var dpr = window.devicePixelRatio || 1;
    W = Math.floor(r.width); H = 400;
    canvas.width = W * dpr; canvas.height = H * dpr;
    canvas.style.width = W + 'px'; canvas.style.height = H + 'px';
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    PW = W - ML - MR; PH = H - MT - MB;
    draw();
  }}

  function ts2x(ts) {{ return ML + PW * (ts - view.ts0) / (view.ts1 - view.ts0); }}
  function p2y(p) {{ return MT + PH * (1 - (p - view.p0) / (view.p1 - view.p0)); }}
  function x2ts(x) {{ return view.ts0 + (x - ML) / PW * (view.ts1 - view.ts0); }}

  function niceStep(range, maxTicks) {{
    var rough = range / maxTicks, mag = Math.pow(10, Math.floor(Math.log10(rough)));
    var norm = rough / mag;
    var step = (norm < 1.5 ? 1 : norm < 3 ? 2 : norm < 7 ? 5 : 10) * mag;
    return step || 1;
  }}

  function fmtTs(iso) {{
    var d = new Date(iso);
    var hh = ('0'+d.getUTCHours()).slice(-2);
    var mm = ('0'+d.getUTCMinutes()).slice(-2);
    var dd = ('0'+d.getUTCDate()).slice(-2);
    var mon = ('0'+(d.getUTCMonth()+1)).slice(-2);
    return dd+'/'+mon+' '+hh+':'+mm;
  }}

  function fmtPrice(p) {{ return Math.abs(p) >= 1 ? p.toLocaleString('en',{{maximumFractionDigits:0}}) : p.toFixed(2); }}

  function visibleRange() {{
    var vs = view.ts0, ve = view.ts1;
    var lo = -1, hi = -1;
    for (var i = 0; i < PRICE_DATA.length; i++) {{
      var ts = PRICE_DATA[i].ts;
      if (lo < 0 && ts >= vs) lo = i;
      if (ts <= ve) hi = i;
    }}
    if (lo < 0) lo = 0;
    if (hi < 0) hi = PRICE_DATA.length - 1;
    return [lo, hi];
  }}

  function updateAutoScale() {{
    if (!autoScale) {{ view.p0 = defs.p0; view.p1 = defs.p1; return; }}
    var r = visibleRange();
    var lo = r[0], hi = r[1];
    var mn = Infinity, mx = -Infinity;
    for (var i = lo; i <= hi; i++) {{
      if (PRICE_DATA[i].p < mn) mn = PRICE_DATA[i].p;
      if (PRICE_DATA[i].p > mx) mx = PRICE_DATA[i].p;
    }}
    if (showTrades) {{
      for (var j = 0; j < TRADE_DATA.length; j++) {{
        var t = TRADE_DATA[j];
        if (t.entry_price < mn) mn = t.entry_price;
        if (t.entry_price > mx) mx = t.entry_price;
        if (t.exit_price !== undefined) {{
          if (t.exit_price < mn) mn = t.exit_price;
          if (t.exit_price > mx) mx = t.exit_price;
        }}
      }}
    }}
    var pad = (mx - mn) * 0.08 || 100;
    view.p0 = mn - pad; view.p1 = mx + pad;
  }}

  function draw() {{
    ctx.clearRect(0, 0, W, H);
    ctx.fillStyle = C.bg; ctx.fillRect(0, 0, W, H);
    ctx.fillStyle = C.plotBg; ctx.fillRect(ML, MT, PW, PH);

    if (showGrid) {{
      var pRange = view.p1 - view.p0;
      var pStep = niceStep(pRange, 8);
      var pStart = Math.ceil(view.p0 / pStep) * pStep;
      ctx.strokeStyle = C.grid; ctx.lineWidth = 1;
      for (var pv = pStart; pv <= view.p1; pv += pStep) {{
        var yy = p2y(pv);
        ctx.beginPath(); ctx.moveTo(ML, yy); ctx.lineTo(ML+PW, yy); ctx.stroke();
        ctx.fillStyle = C.text; ctx.font = C.fontSmall; ctx.textAlign = 'right';
        ctx.fillText(fmtPrice(pv), ML-6, yy+3);
      }}
      var tsRange = view.ts1 - view.ts0;
      var tsStep = niceStep(tsRange, 10);
      var t0 = new Date(view.ts0).getTime();
      var tStart = Math.ceil(t0 / tsStep) * tsStep;
      ctx.strokeStyle = C.grid;
      for (var tv = tStart; tv <= new Date(view.ts1).getTime(); tv += tsStep) {{
        var xx = ts2x(new Date(tv).toISOString());
        if (xx < ML || xx > ML+PW) continue;
        ctx.beginPath(); ctx.moveTo(xx, MT); ctx.lineTo(xx, MT+PH); ctx.stroke();
        ctx.fillStyle = C.text; ctx.font = C.fontSmall; ctx.textAlign = 'center';
        ctx.fillText(fmtTs(new Date(tv).toISOString()), xx, H - MB + 16);
      }}
    }}

    var r = visibleRange(), lo = r[0], hi = r[1];
    var pts = [];
    for (var i = lo; i <= hi; i++) pts.push([ts2x(PRICE_DATA[i].ts), p2y(PRICE_DATA[i].p)]);

    if (pts.length > 1) {{
      var baseY = p2y(view.p0);
      ctx.beginPath();
      ctx.moveTo(pts[0][0], baseY);
      for (var k = 0; k < pts.length; k++) ctx.lineTo(pts[k][0], pts[k][1]);
      ctx.lineTo(pts[pts.length-1][0], baseY);
      ctx.closePath(); ctx.fillStyle = C.fill; ctx.fill();
    }}

    if (pts.length > 1) {{
      ctx.beginPath(); ctx.moveTo(pts[0][0], pts[0][1]);
      for (var k = 1; k < pts.length; k++) ctx.lineTo(pts[k][0], pts[k][1]);
      ctx.strokeStyle = C.line; ctx.lineWidth = 2; ctx.stroke();
    }}

    if (showTrades) {{
      var tsMap = {{}};
      for (var i = lo; i <= hi; i++) tsMap[PRICE_DATA[i].ts] = i;
      for (var j = 0; j < TRADE_DATA.length; j++) {{
        var t = TRADE_DATA[j];
        if (t.entry_ts in tsMap) {{
          var idx = tsMap[t.entry_ts];
          var ex = ts2x(t.entry_ts), ey = p2y(t.entry_price);
          var col = t.side === 'BUY' ? C.buy : C.sell;
          var dir = t.side === 'BUY' ? -1 : 1;
          ctx.beginPath();
          ctx.moveTo(ex, ey + dir * 9);
          ctx.lineTo(ex - 7, ey - dir * 5);
          ctx.lineTo(ex + 7, ey - dir * 5);
          ctx.closePath();
          ctx.fillStyle = col; ctx.fill();
        }}
        if (showExits && t.exit_ts && t.exit_ts in tsMap) {{
          var idx2 = tsMap[t.exit_ts];
          var xx = ts2x(t.exit_ts), xy = p2y(t.exit_price);
          ctx.beginPath();
          ctx.moveTo(xx, xy + 9);
          ctx.lineTo(xx - 7, xy - 5);
          ctx.lineTo(xx + 7, xy - 5);
          ctx.closePath();
          ctx.fillStyle = C.exit; ctx.fill();
        }}
      }}
    }}

    if (mouseX >= ML && mouseX <= ML+PW && mouseY >= MT && mouseY <= MT+PH) {{
      ctx.strokeStyle = C.crosshair; ctx.lineWidth = 1;
      ctx.beginPath(); ctx.moveTo(mouseX, MT); ctx.lineTo(mouseX, MT+PH); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(ML, mouseY); ctx.lineTo(ML+PW, mouseY); ctx.stroke();
      var hoverP = view.p1 - (mouseY - MT) / PH * (view.p1 - view.p0);
      ctx.fillStyle = C.text; ctx.font = C.fontSmall; ctx.textAlign = 'right';
      ctx.fillText(fmtPrice(hoverP), ML-6, mouseY+3);
      var hoverTs = x2ts(mouseX);
      ctx.textAlign = 'center';
      ctx.fillText(fmtTs(new Date(hoverTs).toISOString()), mouseX, H - MB + 16);
    }}

    ctx.strokeStyle = '#2a2d3a'; ctx.lineWidth = 1;
    ctx.strokeRect(ML, MT, PW, PH);

    if (rangeSel) {{
      var fullL = 10, fullW = W - 20, fullH = 6;
      var ratio0 = (view.ts0 - defs.ts0) / (defs.ts1 - defs.ts0);
      var ratio1 = (view.ts1 - defs.ts0) / (defs.ts1 - defs.ts0);
      rangeSel.innerHTML =
        '<div style="position:relative;width:100%;height:100%">' +
        '<div class="range-bar" style="position:absolute;top:0;left:0;width:100%;height:100%"></div>' +
        '<div class="range-handle" style="position:absolute;top:0;left:' + (ratio0*100) + '%;width:' + ((ratio1-ratio0)*100) + '%;height:100%"></div>' +
        '</div>';
    }}
  }}

  function showTooltip(mx, my) {{
    var nearI = -1, nearDist = 14;
    for (var i = 0; i < PRICE_DATA.length; i++) {{
      var px = ts2x(PRICE_DATA[i].ts), py = p2y(PRICE_DATA[i].p);
      var dx = mx - px, dy = my - py;
      var dist = Math.sqrt(dx*dx + dy*dy);
      if (dist < nearDist) {{ nearDist = dist; nearI = i; }}
    }}
    if (nearI < 0) {{ tooltip.style.display = 'none'; return; }}
    var d = PRICE_DATA[nearI];
    var html = '<b>' + fmtTs(d.ts) + '</b><br>Price: <b>' + fmtPrice(d.p) + '</b>';
    var nearTrade = null;
    for (var j = 0; j < TRADE_DATA.length; j++) {{
      var t = TRADE_DATA[j];
      if (t.entry_ts === d.ts) {{ nearTrade = t; break; }}
      if (t.exit_ts === d.ts) {{ nearTrade = t; break; }}
    }}
    if (nearTrade) {{
      html += '<br>Trade: <b>' + nearTrade.id + '</b> ' + nearTrade.side;
      if (nearTrade.pnl !== undefined) html += ' PnL: ' + (nearTrade.pnl >= 0 ? '+' : '') + nearTrade.pnl.toLocaleString();
    }}
    tooltip.innerHTML = html;
    tooltip.style.display = 'block';
    var tx = mx + 15, ty = my - 10;
    if (tx + 180 > W) tx = mx - 185;
    if (ty < 0) ty = 10;
    tooltip.style.left = tx + 'px'; tooltip.style.top = ty + 'px';
  }}

  canvas.addEventListener('mousemove', function(e) {{
    var r = canvas.getBoundingClientRect();
    mouseX = e.clientX - r.left; mouseY = e.clientY - r.top;
    if (dragging) {{
      var dx = mouseX - dragX0;
      var tsPerPixel = (dragTs1 - dragTs0) / PW;
      view.ts0 = dragTs0 - dx * tsPerPixel;
      view.ts1 = dragTs1 - dx * tsPerPixel;
      if (autoScale) updateAutoScale();
      draw();
      return;
    }}
    draw();
    if (mouseX >= ML && mouseX <= ML+PW && mouseY >= MT && mouseY <= MT+PH)
      showTooltip(mouseX, mouseY);
    else tooltip.style.display = 'none';
  }});

  canvas.addEventListener('mouseleave', function() {{
    mouseX = -1; mouseY = -1;
    tooltip.style.display = 'none';
    draw();
  }});

  canvas.addEventListener('mousedown', function(e) {{
    var r = canvas.getBoundingClientRect();
    var x = e.clientX - r.left, y = e.clientY - r.top;
    if (x >= ML && x <= ML+PW && y >= MT && y <= MT+PH) {{
      dragging = true; dragX0 = x;
      dragTs0 = view.ts0; dragTs1 = view.ts1;
      canvas.style.cursor = 'grabbing';
    }}
  }});

  document.addEventListener('mouseup', function() {{
    dragging = false;
    canvas.style.cursor = 'crosshair';
  }});

  canvas.addEventListener('wheel', function(e) {{
    e.preventDefault();
    var r = canvas.getBoundingClientRect();
    var mx = e.clientX - r.left;
    var ratio = (mx - ML) / PW;
    if (ratio < 0) ratio = 0; if (ratio > 1) ratio = 1;
    var factor = e.deltaY > 0 ? 1.15 : 1 / 1.15;
    var anchor = view.ts0 + ratio * (view.ts1 - view.ts0);
    view.ts0 = anchor - ratio * (view.ts1 - view.ts0) * factor;
    view.ts1 = anchor + (1 - ratio) * (view.ts1 - view.ts0) * factor;
    if (autoScale) updateAutoScale();
    draw();
  }}, {{ passive: false }});

  document.addEventListener('keydown', function(e) {{
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
    var k = e.key, handled = true;
    if (k === 'w' || k === 'W' || k === '=') {{ var c = (view.ts0+view.ts1)/2; var h = (view.ts1-view.ts0)/2/1.25; view.ts0 = c-h; view.ts1 = c+h; }}
    else if (k === 's' || k === 'S') {{ var c = (view.ts0+view.ts1)/2; var h = (view.ts1-view.ts0)/2*1.25; view.ts0 = c-h; view.ts1 = c+h; }}
    else if (k === 'a' || k === 'A' || k === 'ArrowLeft') {{ var d = (view.ts1-view.ts0)*0.2; view.ts0 -= d; view.ts1 -= d; }}
    else if (k === 'd' || k === 'D' || k === 'ArrowRight') {{ var d = (view.ts1-view.ts0)*0.2; view.ts0 += d; view.ts1 += d; }}
    else if (k === 'r' || k === 'R') {{ view.ts0 = defs.ts0; view.ts1 = defs.ts1; view.p0 = defs.p0; view.p1 = defs.p1; }}
    else handled = false;
    if (handled) {{ e.preventDefault(); if (autoScale) updateAutoScale(); draw(); }}
  }});

  document.getElementById('chartZoomIn').onclick = function() {{
    var c = (view.ts0+view.ts1)/2; var h = (view.ts1-view.ts0)/2/1.5;
    view.ts0 = c-h; view.ts1 = c+h;
    if (autoScale) updateAutoScale(); draw();
  }};
  document.getElementById('chartZoomOut').onclick = function() {{
    var c = (view.ts0+view.ts1)/2; var h = (view.ts1-view.ts0)/2*1.5;
    view.ts0 = c-h; view.ts1 = c+h;
    if (autoScale) updateAutoScale(); draw();
  }};
  document.getElementById('chartPanLeft').onclick = function() {{
    var d = (view.ts1-view.ts0)*0.3; view.ts0 -= d; view.ts1 -= d;
    if (autoScale) updateAutoScale(); draw();
  }};
  document.getElementById('chartPanRight').onclick = function() {{
    var d = (view.ts1-view.ts0)*0.3; view.ts0 += d; view.ts1 += d;
    if (autoScale) updateAutoScale(); draw();
  }};
  document.getElementById('chartReset').onclick = function() {{
    view.ts0 = defs.ts0; view.ts1 = defs.ts1; view.p0 = defs.p0; view.p1 = defs.p1;
    autoScale = true; document.getElementById('chartAutoScale').checked = true;
    draw();
  }};
  document.getElementById('chartShowTrades').onchange = function() {{
    showTrades = this.checked; draw();
  }};
  document.getElementById('chartShowExits').onchange = function() {{
    showExits = this.checked; draw();
  }};
  document.getElementById('chartShowGrid').onchange = function() {{
    showGrid = this.checked; draw();
  }};
  document.getElementById('chartAutoScale').onchange = function() {{
    autoScale = this.checked;
    if (autoScale) updateAutoScale();
    else {{ view.p0 = defs.p0; view.p1 = defs.p1; }}
    draw();
  }};

  canvas.style.cursor = 'crosshair';
  resize();
  window.addEventListener('resize', resize);
}})();
</script>"""


# ---------------------------------------------------------------------------
# Static SVG Charts (Equity, Drawdown)
# ---------------------------------------------------------------------------

def _svg_polyline(points, color="#4a9eff", stroke_width=2):
    """Generate SVG polyline from list of (x, y) tuples in viewBox coords."""
    coords = " ".join(f"{x:.1f},{y:.1f}" for x, y in points)
    return f'<polyline points="{coords}" fill="none" stroke="{color}" stroke-width="{stroke_width}" />'


def _svg_polygon(points, color="#ff4d4d33", stroke="none"):
    """Generate SVG polygon (filled area)."""
    coords = " ".join(f"{x:.1f},{y:.1f}" for x, y in points)
    return f'<polygon points="{coords}" fill="{color}" stroke="{stroke}" />'


def _svg_text(x, y, text, font_size=10, fill="#8a8f9e", anchor="end"):
    """Generate SVG text element."""
    return f'<text x="{x}" y="{y}" font-size="{font_size}" fill="{fill}" font-family="Consolas,Monaco,monospace" text-anchor="{anchor}">{esc(str(text))}</text>'


def _svg_line(x1, y1, x2, y2, color="#2a2d3a", stroke_width=1):
    return f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{color}" stroke-width="{stroke_width}" />'


def _chart_frame(width, height, margin, title, y_min, y_max, y_steps=5):
    """Generate common SVG elements: background, grid, axes, title. Returns (svg_parts, x_scale_func, y_scale_func)."""
    ml, mt, mr, mb = margin
    plot_w = width - ml - mr
    plot_h = height - mt - mb

    parts = []
    parts.append(f'<rect width="{width}" height="{height}" fill="#0f1117" rx="4" />')
    parts.append(_svg_text(ml, mt - 8, title, font_size=13, fill="#e0e0e0", anchor="start"))
    parts.append(f'<rect x="{ml}" y="{mt}" width="{plot_w}" height="{plot_h}" fill="#12141c" />')

    y_range = y_max - y_min if y_max != y_min else 1
    for i in range(y_steps + 1):
        y_val = y_min + (y_range * i / y_steps)
        y_pos = mt + plot_h - (plot_h * i / y_steps)
        parts.append(_svg_line(ml, y_pos, ml + plot_w, y_pos, "#1e2030"))
        label = f"{y_val:,.0f}" if abs(y_val) >= 1 else f"{y_val:.2f}"
        parts.append(_svg_text(ml - 4, y_pos + 3, label, font_size=9))

    def x_scale(i, n):
        if n <= 1:
            return ml + plot_w / 2
        return ml + (plot_w * i / (n - 1))

    def y_scale(val):
        return mt + plot_h - (plot_h * (val - y_min) / y_range)

    return parts, x_scale, y_scale


def svg_equity_chart(equity_points):
    """Generate inline SVG equity curve chart."""
    if not equity_points:
        return '<p style="color:var(--text-muted)">No equity data.</p>'

    values = [e["equity"] for e in equity_points]
    y_min = min(values) * 0.998
    y_max = max(values) * 1.002
    width, height = 700, 280
    margin = (70, 30, 20, 30)

    parts, x_scale, y_scale = _chart_frame(width, height, margin,
                                            "Equity Curve — dummy_visual_strategy (DEMO)", y_min, y_max)

    pts = [(x_scale(i, len(equity_points)), y_scale(e["equity"])) for i, e in enumerate(equity_points)]
    parts.append(_svg_polyline(pts, "#33cc66", 2))

    base_y = y_scale(y_min)
    area_pts = [(pts[0][0], base_y)] + pts + [(pts[-1][0], base_y)]
    parts.append(_svg_polygon(area_pts, "#33cc6615"))

    svg = f'<svg viewBox="0 0 {width} {height}" xmlns="http://www.w3.org/2000/svg" style="width:100%;height:auto">'
    svg += "".join(parts)
    svg += "</svg>"
    return svg


def svg_drawdown_chart(dd_points):
    """Generate inline SVG drawdown area chart."""
    if not dd_points:
        return '<p style="color:var(--text-muted)">No drawdown data.</p>'

    values = [d["drawdown_pct"] for d in dd_points]
    y_min = min(values) * 1.1
    y_max = 0.0
    width, height = 700, 250
    margin = (55, 30, 20, 30)

    parts, x_scale, y_scale = _chart_frame(width, height, margin,
                                            "Drawdown — dummy_visual_strategy (DEMO)", y_min, y_max, y_steps=4)

    pts = [(x_scale(i, len(dd_points)), y_scale(d["drawdown_pct"])) for i, d in enumerate(dd_points)]
    parts.append(_svg_polyline(pts, "#ff4d4d", 1.5))

    zero_y = y_scale(0)
    area_pts = [(pts[0][0], zero_y)] + pts + [(pts[-1][0], zero_y)]
    parts.append(_svg_polygon(area_pts, "#ff4d4d20"))

    parts.append(_svg_line(margin[0], zero_y, width - margin[2], zero_y, "#555a6e", 1))

    svg = f'<svg viewBox="0 0 {width} {height}" xmlns="http://www.w3.org/2000/svg" style="width:100%;height:auto">'
    svg += "".join(parts)
    svg += "</svg>"
    return svg


# ---------------------------------------------------------------------------
# Charts Section Renderer
# ---------------------------------------------------------------------------

def render_charts():
    prices = fetch_all("SELECT * FROM demo_price_series ORDER BY ts")
    equity = fetch_all("SELECT * FROM demo_equity_curve ORDER BY ts")
    drawdown = fetch_all("SELECT * FROM demo_drawdown ORDER BY ts")
    trades = fetch_all("SELECT * FROM demo_trades ORDER BY entry_ts")

    trades_rows = ""
    for t in trades:
        pnl_str = f"{t['pnl']:+,.0f}" if t['pnl'] is not None else "\u2014"
        pnl_cls = "color:var(--success)" if t['pnl'] and t['pnl'] > 0 else "color:var(--danger)" if t['pnl'] and t['pnl'] < 0 else ""
        exit_price_str = f"{t['exit_price']:,.0f}" if t['exit_price'] else "\u2014"
        trades_rows += f"""<tr>
  <td><code>{esc(t['trade_id'])}</code></td>
  <td>{esc(t['side'])}</td>
  <td>{esc(t['instrument'])}</td>
  <td>{esc(t['entry_ts'])}</td>
  <td>{t['entry_price']:,.0f}</td>
  <td>{esc(t['exit_ts'] or '\u2014')}</td>
  <td>{exit_price_str}</td>
  <td>{t['quantity']}</td>
  <td style="{pnl_cls}">{pnl_str}</td>
  <td>{status_badge(t['status'])}</td>
</tr>"""

    banner = """
<div class="chart-banner">
  <strong>DEMO DATA ONLY</strong> &mdash; NO REAL MARKET DATA &mdash; NO BROKER &mdash; LIVE DISABLED
</div>"""

    return f"""
{banner}
<h2 class="section-title">Charts — Demo Visualizations</h2>
<p style="color:var(--text-secondary);margin-bottom:16px;font-size:12px">
  Synthetic price series, equity curve, and drawdown for <code>RI_demo</code> /
  <code>dummy_visual_strategy</code>. All data is generated, not real.
</p>

<div class="chart-container">
  <div class="chart-title">Price Chart with Trade Markers</div>
  {_canvas_price_chart_html(prices, trades)}
</div>

<div class="chart-container">
  <div class="chart-title">Equity Curve</div>
  {svg_equity_chart(equity)}
</div>

<div class="chart-container">
  <div class="chart-title">Drawdown</div>
  {svg_drawdown_chart(drawdown)}
</div>

<h3 class="subsection-title">Demo Trades</h3>
<table>
<thead><tr>
  <th>Trade ID</th><th>Side</th><th>Instrument</th>
  <th>Entry Time</th><th>Entry Price</th>
  <th>Exit Time</th><th>Exit Price</th>
  <th>Qty</th><th>PnL</th><th>Status</th>
</tr></thead>
<tbody>{trades_rows}</tbody>
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
    "charts": render_charts,
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
