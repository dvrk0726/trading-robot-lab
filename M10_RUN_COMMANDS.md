# M10_RUN_COMMANDS

Use Developer PowerShell for VS 2022.

## Pull latest files

```powershell
git pull
```

## Run M10

```powershell
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10_L3_L2_RECONSTRUCTION_DIAGNOSTICS.md"
```

## Save M10

```powershell
.\tools\mimo_save.ps1 "Add L3 L2 reconstruction diagnostics"
```

## Build and test

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
```

## Open current dashboard

```powershell
python apps/lab/backend/lab_dashboard.py
```

Then open browser:

```text
http://127.0.0.1:8000
```

Note: the current dashboard is safe and historical only. Real order book preview is planned after M10 diagnostics and M11 UI work.
