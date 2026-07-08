param(
    [Parameter(Mandatory=$true)]
    [string]$Message
)

$ErrorActionPreference = "Stop"

Write-Host "== MiMo save helper =="
Write-Host "Commit message: $Message"

# Make sure we are in a git repository.
git rev-parse --show-toplevel | Out-Null

$status = git status --short
if (-not $status) {
    Write-Host "Nothing to commit. Working tree is clean."
    exit 0
}

Write-Host "Current changes:"
$status | ForEach-Object { Write-Host $_ }

$forbiddenPatterns = @(
    '^\?\? \.env$',
    '^\?\? \.env\.',
    '^ M \.env$',
    '^\?\? secrets/',
    '^\?\? private/',
    '^\?\? credentials/',
    '^\?\? \.mimocode/',
    '^\?\? .*\.key$',
    '^\?\? .*\.pem$',
    '^\?\? .*\.pfx$',
    '^\?\? .*\.p12$'
)

foreach ($line in $status) {
    foreach ($pattern in $forbiddenPatterns) {
        if ($line -match $pattern) {
            Write-Error "Forbidden file detected in git status: $line"
            exit 1
        }
    }
}

Write-Host "Staging changes..."
git add -A

Write-Host "Committing..."
git commit -m $Message

Write-Host "Pushing..."
git push

Write-Host "Done."
