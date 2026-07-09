# M10M: Automated real-sample validation script
# Runs build, tests, and four L3-to-L2 validation modes with --summary-out.
# Prints compact comparison table and L2 strategy-ready status.

param(
    [string]$QshPath = ".\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh",
    [string]$ReportDir = ".\data\reports\qsh\RTS-3.21\2021-01-05",
    [switch]$SkipBuild,
    [switch]$RunMissingCancelProbe,
    [switch]$RunOrphanCancelAudit,
    [switch]$RunFirstCrossedProbe,
    [switch]$RunSnapshotAudit,
    [switch]$RunCrossingWindowAudit,
    [switch]$RunCrossedPersistenceAudit
)

$ErrorActionPreference = "Continue"

# --- Detect repo root (script may be invoked from anywhere) ---
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$RepoRoot = Split-Path -Parent $ScriptDir
$OriginalDir = Get-Location
Set-Location $RepoRoot

Write-Host "=== M10M Automated Real-Sample Validation ===" -ForegroundColor Cyan
Write-Host "Repo root: $RepoRoot"
Write-Host ""

# --- 1. Check QSH file ---
if (-not (Test-Path $QshPath)) {
    Write-Host "QSH file not found: $QshPath" -ForegroundColor Yellow
    Write-Host "Real-sample validation skipped." -ForegroundColor Yellow
    Set-Location $OriginalDir
    exit 0
}

$QshFullPath = (Resolve-Path $QshPath).Path
Write-Host "QSH file found: $QshFullPath" -ForegroundColor Green
Write-Host ""

# --- 2. Build (unless -SkipBuild) ---
if (-not $SkipBuild) {
    Write-Host "Building qsh_ingest (Release)..." -ForegroundColor Cyan
    cmake --build build/qsh_ingest --config Release 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build FAILED" -ForegroundColor Red
        Set-Location $OriginalDir
        exit 1
    }
    Write-Host "Build OK" -ForegroundColor Green
    Write-Host ""

    Write-Host "Running tests..." -ForegroundColor Cyan
    ctest --test-dir build/qsh_ingest -C Release --output-on-failure 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Tests FAILED" -ForegroundColor Red
        Set-Location $OriginalDir
        exit 1
    }
    Write-Host "Tests OK" -ForegroundColor Green
    Write-Host ""
} else {
    Write-Host "Build/tests skipped (-SkipBuild)" -ForegroundColor Yellow
    Write-Host ""
}

# --- 3. Create report directory ---
if (-not (Test-Path $ReportDir)) {
    New-Item -ItemType Directory -Path $ReportDir -Force | Out-Null
}
$ReportDirFull = (Resolve-Path $ReportDir).Path

$ExePath = ".\build\qsh_ingest\Release\qsh_ingest.exe"

# --- 4. Define validation modes ---
$modes = @(
    @{
        Name     = "per-record strict"
        CsvOut   = "$ReportDirFull\l2_per_record_strict.csv"
        Summary  = "$ReportDirFull\l2_per_record_strict.summary.json"
        Args     = @("l3-to-l2", $QshFullPath,
            "--depth", "5",
            "--max-records", "100000",
            "--max-snapshots", "10000",
            "--snapshot-mode", "txend",
            "--orphan-fill-mode", "strict",
            "--out", "$ReportDirFull\l2_per_record_strict.csv",
            "--summary-out", "$ReportDirFull\l2_per_record_strict.summary.json")
    },
    @{
        Name     = "per-record reduce-same-price"
        CsvOut   = "$ReportDirFull\l2_reduce_same_price.csv"
        Summary  = "$ReportDirFull\l2_reduce_same_price.summary.json"
        Args     = @("l3-to-l2", $QshFullPath,
            "--depth", "5",
            "--max-records", "100000",
            "--max-snapshots", "10000",
            "--snapshot-mode", "txend",
            "--orphan-fill-mode", "reduce-same-price",
            "--out", "$ReportDirFull\l2_reduce_same_price.csv",
            "--summary-out", "$ReportDirFull\l2_reduce_same_price.summary.json")
    },
    @{
        Name     = "per-record orphan-cancel-ignore"
        CsvOut   = "$ReportDirFull\l2_orphan_cancel_ignore.csv"
        Summary  = "$ReportDirFull\l2_orphan_cancel_ignore.summary.json"
        Args     = @("l3-to-l2", $QshFullPath,
            "--depth", "5",
            "--max-records", "100000",
            "--max-snapshots", "10000",
            "--snapshot-mode", "txend",
            "--orphan-fill-mode", "strict",
            "--orphan-cancel-mode", "ignore",
            "--out", "$ReportDirFull\l2_orphan_cancel_ignore.csv",
            "--summary-out", "$ReportDirFull\l2_orphan_cancel_ignore.summary.json")
    },
    @{
        Name     = "per-record reduce+orphan-cancel-ignore"
        CsvOut   = "$ReportDirFull\l2_reduce_orphan_cancel_ignore.csv"
        Summary  = "$ReportDirFull\l2_reduce_orphan_cancel_ignore.summary.json"
        Args     = @("l3-to-l2", $QshFullPath,
            "--depth", "5",
            "--max-records", "100000",
            "--max-snapshots", "10000",
            "--snapshot-mode", "txend",
            "--orphan-fill-mode", "reduce-same-price",
            "--orphan-cancel-mode", "ignore",
            "--out", "$ReportDirFull\l2_reduce_orphan_cancel_ignore.csv",
            "--summary-out", "$ReportDirFull\l2_reduce_orphan_cancel_ignore.summary.json")
    },
    @{
        Name     = "snapshot-records-mode load"
        CsvOut   = "$ReportDirFull\l2_snapshot_load.csv"
        Summary  = "$ReportDirFull\l2_snapshot_load.summary.json"
        Args     = @("l3-to-l2", $QshFullPath,
            "--depth", "5",
            "--max-records", "100000",
            "--max-snapshots", "10000",
            "--snapshot-mode", "txend",
            "--snapshot-records-mode", "load",
            "--out", "$ReportDirFull\l2_snapshot_load.csv",
            "--summary-out", "$ReportDirFull\l2_snapshot_load.summary.json")
    },
    @{
        Name     = "tx-grouped"
        CsvOut   = "$ReportDirFull\l2_tx_grouped.csv"
        Summary  = "$ReportDirFull\l2_tx_grouped.summary.json"
        Args     = @("l3-to-l2", $QshFullPath,
            "--depth", "5",
            "--max-records", "100000",
            "--max-snapshots", "10000",
            "--snapshot-mode", "txend",
            "--book-update-mode", "tx-grouped",
            "--out", "$ReportDirFull\l2_tx_grouped.csv",
            "--summary-out", "$ReportDirFull\l2_tx_grouped.summary.json")
    }
)

# --- 5. Run modes and collect results ---
Write-Host "Running validation modes..." -ForegroundColor Cyan
Write-Host ""

$results = @()

foreach ($mode in $modes) {
    Write-Host "  $($mode.Name)..." -NoNewline

    $output = & $ExePath $mode.Args 2>&1 | Out-String

    # Parse summary JSON
    if (Test-Path $mode.Summary) {
        $summary = Get-Content $mode.Summary -Raw | ConvertFrom-Json
        $results += [PSCustomObject]@{
            Mode                            = $mode.Name
            missing_order_id                = $summary.missing_order_id
            missing_on_fill                 = $summary.missing_on_fill
            missing_on_cancel               = $summary.missing_on_cancel
            missing_on_remove               = $summary.missing_on_remove
            orphan_cancel_mode              = $summary.orphan_cancel_mode
            orphan_cancel_ignored           = $summary.orphan_cancel_ignored
            orphan_remove_ignored           = $summary.orphan_remove_ignored
            orphan_fill_events              = $summary.orphan_fill_events
            orphan_fill_level_reductions    = $summary.orphan_fill_level_reductions
            crossed_book_snapshots          = $summary.crossed_book_snapshots
            non_positive_spread_snapshots   = $summary.non_positive_spread_snapshots
            first_missing_order_record_index = $summary.first_missing_order_record_index
            first_crossed_book_record_index = $summary.first_crossed_book_record_index
            l2_strategy_ready               = $summary.l2_strategy_ready
        }
        Write-Host " OK" -ForegroundColor Green
    } else {
        Write-Host " FAILED (no summary)" -ForegroundColor Red
        $results += [PSCustomObject]@{
            Mode                            = $mode.Name
            missing_order_id                = "?"
            missing_on_fill                 = "?"
            missing_on_cancel               = "?"
            missing_on_remove               = "?"
            orphan_cancel_mode              = "?"
            orphan_cancel_ignored           = "?"
            orphan_remove_ignored           = "?"
            orphan_fill_events              = "?"
            orphan_fill_level_reductions    = "?"
            crossed_book_snapshots          = "?"
            non_positive_spread_snapshots   = "?"
            first_missing_order_record_index = "?"
            first_crossed_book_record_index = "?"
            l2_strategy_ready               = "?"
        }
    }
}

# --- 6. Print compact comparison table ---
Write-Host ""
Write-Host "=== Real-Sample Validation Results ===" -ForegroundColor Cyan
Write-Host ""

# Build markdown-compatible table
$header = "| mode | missing_order_id | missing_on_fill | missing_on_cancel | missing_on_remove | orphan_cancel_mode | orphan_cancel_ignored | orphan_remove_ignored | orphan_fill_events | orphan_fill_level_reductions | crossed_book_snapshots | non_positive_spread_snapshots | first_missing_order_record_index | first_crossed_book_record_index | l2_strategy_ready |"
$sep    = "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"
Write-Host $header
Write-Host $sep

foreach ($r in $results) {
    $line = "| $($r.Mode) | $($r.missing_order_id) | $($r.missing_on_fill) | $($r.missing_on_cancel) | $($r.missing_on_remove) | $($r.orphan_cancel_mode) | $($r.orphan_cancel_ignored) | $($r.orphan_remove_ignored) | $($r.orphan_fill_events) | $($r.orphan_fill_level_reductions) | $($r.crossed_book_snapshots) | $($r.non_positive_spread_snapshots) | $($r.first_missing_order_record_index) | $($r.first_crossed_book_record_index) | $($r.l2_strategy_ready) |"
    Write-Host $line
}

# --- 7. Print L2 strategy-ready status ---
Write-Host ""
$anyReady = $results | Where-Object { $_.l2_strategy_ready -eq $true }
if ($anyReady) {
    Write-Host "L2 strategy-ready: YES (at least one mode)" -ForegroundColor Green
} else {
    Write-Host "L2 strategy-ready: NO (all modes)" -ForegroundColor Red
}

# --- 8. Missing-cancel-probe (optional) ---
if ($RunMissingCancelProbe) {
    Write-Host ""
    Write-Host "Running missing-cancel-probe..." -ForegroundColor Cyan
    $probeOut = "$ReportDirFull\missing_cancel_probe.csv"
    & $ExePath "missing-cancel-probe" $QshFullPath "--out" $probeOut "--max-probes" "100" 2>&1 | Write-Host
    if (Test-Path $probeOut) {
        Write-Host "Probe output: $probeOut" -ForegroundColor Green
    } else {
        Write-Host "Probe output not generated." -ForegroundColor Yellow
    }
} else {
    Write-Host ""
    Write-Host "Tip: run with -RunMissingCancelProbe to also probe missing_on_cancel orders." -ForegroundColor DarkGray
}

# --- 9. Orphan-cancel-audit (optional) ---
if ($RunOrphanCancelAudit) {
    Write-Host ""
    Write-Host "Running orphan-cancel-audit..." -ForegroundColor Cyan
    $auditOut = "$ReportDirFull\orphan_cancel_audit.csv"
    & $ExePath "orphan-cancel-audit" $QshFullPath "--out" $auditOut "--max-audits" "200" 2>&1 | Write-Host
    if (Test-Path $auditOut) {
        Write-Host "Audit output: $auditOut" -ForegroundColor Green
    } else {
        Write-Host "Audit output not generated." -ForegroundColor Yellow
    }
} else {
    Write-Host ""
    Write-Host "Tip: run with -RunOrphanCancelAudit for detailed orphan cancel/remove audit." -ForegroundColor DarkGray
}

# --- 10. First-crossed-root-cause (optional) ---
if ($RunFirstCrossedProbe) {
    Write-Host ""
    Write-Host "Running first-crossed-root-cause..." -ForegroundColor Cyan
    & $ExePath "first-crossed-root-cause" $QshFullPath "--out-dir" $ReportDirFull "--context" "40" 2>&1 | Write-Host
    $rootCauseFile = "$ReportDirFull\first_crossed_root_cause.csv"
    if (Test-Path $rootCauseFile) {
        Write-Host "Root cause output: $ReportDirFull" -ForegroundColor Green
    } else {
        Write-Host "Root cause output not generated." -ForegroundColor Yellow
    }
} else {
    Write-Host ""
    Write-Host "Tip: run with -RunFirstCrossedProbe for first crossed-book root cause trace." -ForegroundColor DarkGray
}

# --- 11. Snapshot-audit (optional) ---
if ($RunSnapshotAudit) {
    Write-Host ""
    Write-Host "Running snapshot-audit..." -ForegroundColor Cyan
    $snapshotAuditOut = "$ReportDirFull\snapshot_audit_before_crossing.csv"
    & $ExePath "snapshot-audit" $QshFullPath "--out" $snapshotAuditOut "--max-records" "10000" 2>&1 | Write-Host
    if (Test-Path $snapshotAuditOut) {
        Write-Host "Snapshot audit output: $snapshotAuditOut" -ForegroundColor Green
    } else {
        Write-Host "Snapshot audit output not generated." -ForegroundColor Yellow
    }
} else {
    Write-Host ""
    Write-Host "Tip: run with -RunSnapshotAudit for snapshot record audit before first crossing." -ForegroundColor DarkGray
}

# --- 11b. Crossing-window-audit (optional) ---
if ($RunCrossingWindowAudit) {
    Write-Host ""
    Write-Host "Running crossing-window-audit..." -ForegroundColor Cyan
    $crossingAuditOut = "$ReportDirFull\crossing_window_1966_2136_audit.csv"
    & $ExePath "crossing-window-audit" $QshFullPath "--from" "1966" "--to" "2136" "--out" $crossingAuditOut 2>&1 | Write-Host
    if (Test-Path $crossingAuditOut) {
        Write-Host "Crossing window audit output: $crossingAuditOut" -ForegroundColor Green
    } else {
        Write-Host "Crossing window audit output not generated." -ForegroundColor Yellow
    }
} else {
    Write-Host ""
    Write-Host "Tip: run with -RunCrossingWindowAudit for crossing window audit of records 1966..2136." -ForegroundColor DarkGray
}

# --- 11c. Crossed-persistence-audit (optional) ---
if ($RunCrossedPersistenceAudit) {
    Write-Host ""
    Write-Host "Running crossed-persistence-audit..." -ForegroundColor Cyan
    $crossedPersistenceOut = "$ReportDirFull\crossed_persistence_audit.csv"
    & $ExePath "crossed-persistence-audit" $QshFullPath "--from" "2136" "--out" $crossedPersistenceOut 2>&1 | Write-Host
    if (Test-Path $crossedPersistenceOut) {
        Write-Host "Crossed persistence audit output: $crossedPersistenceOut" -ForegroundColor Green
    } else {
        Write-Host "Crossed persistence audit output not generated." -ForegroundColor Yellow
    }
} else {
    Write-Host ""
    Write-Host "Tip: run with -RunCrossedPersistenceAudit for crossed-state persistence analysis from record 2136." -ForegroundColor DarkGray
}

# --- 12. Summary ---
Write-Host ""
Write-Host "=== Validation Complete ===" -ForegroundColor Cyan
Write-Host "Report dir: $ReportDirFull"
Write-Host "Generated CSV/JSON files are under data/reports/ (not committed to Git)."

Set-Location $OriginalDir
