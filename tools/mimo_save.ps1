param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$Message
)

$ErrorActionPreference = "Stop"

function Invoke-GitChecked {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    & git @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Arguments -join ' ') failed with exit code $LASTEXITCODE"
    }
}

Write-Host "== MiMo feature-branch save helper =="
Write-Host "Commit message: $Message"

Invoke-GitChecked @("rev-parse", "--show-toplevel")

$branch = (git branch --show-current).Trim()
if ($LASTEXITCODE -ne 0) {
    throw "Cannot determine current branch."
}

if (-not $branch) {
    throw "Detached HEAD is not allowed. Create or switch to a feature branch first."
}

if ($branch -in @("main", "master")) {
    throw "Direct development on '$branch' is forbidden. Create a dedicated feature branch and retry."
}

$allowedPrefixes = @("mimo/", "fix/", "chore/", "docs/")
$prefixAllowed = $false
foreach ($prefix in $allowedPrefixes) {
    if ($branch.StartsWith($prefix)) {
        $prefixAllowed = $true
        break
    }
}
if (-not $prefixAllowed) {
    throw "Branch '$branch' must start with one of: $($allowedPrefixes -join ', ')"
}

$status = git status --short
if ($LASTEXITCODE -ne 0) {
    throw "git status failed."
}
if (-not $status) {
    Write-Host "Nothing to commit. Working tree is clean."
    exit 0
}

Write-Host "Current branch: $branch"
Write-Host "Current changes:"
$status | ForEach-Object { Write-Host $_ }

Write-Host "Running git diff --check..."
Invoke-GitChecked @("diff", "--check")

Write-Host "Running repository hygiene check..."
& python tools/check_repository_hygiene.py
if ($LASTEXITCODE -ne 0) {
    throw "Repository hygiene check failed. Nothing was committed."
}

Write-Host "Staging changes..."
Invoke-GitChecked @("add", "-A")

Write-Host "Checking staged diff..."
Invoke-GitChecked @("diff", "--cached", "--check")

$staged = git diff --cached --name-only
if ($LASTEXITCODE -ne 0) {
    throw "Cannot inspect staged files."
}
if (-not $staged) {
    Write-Host "Nothing staged after hygiene rules."
    exit 0
}

Write-Host "Staged files:"
$staged | ForEach-Object { Write-Host $_ }

Write-Host "Committing on feature branch..."
Invoke-GitChecked @("commit", "-m", $Message)

Write-Host "Fetching current origin/main..."
Invoke-GitChecked @("fetch", "origin", "main")

Write-Host "Rebasing feature branch on origin/main..."
Invoke-GitChecked @("rebase", "origin/main")

Write-Host "Re-running hygiene check after rebase..."
& python tools/check_repository_hygiene.py
if ($LASTEXITCODE -ne 0) {
    throw "Repository hygiene check failed after rebase. Do not push."
}

Write-Host "Pushing feature branch only..."
Invoke-GitChecked @("push", "-u", "origin", "HEAD")

$commit = (git rev-parse HEAD).Trim()
Write-Host "Done."
Write-Host "Branch: $branch"
Write-Host "Commit: $commit"
Write-Host "Next required action: create or update a Pull Request to main, then stop."
