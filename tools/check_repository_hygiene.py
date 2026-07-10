#!/usr/bin/env python3
"""Fail when tracked or pending repository files violate project hygiene rules.

The check covers high-confidence secret signatures, forbidden private/raw file
classes, generated artifacts and project file-size limits. It complements, but
does not replace, code review.
"""

from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path, PurePosixPath

REPO_ROOT = Path(__file__).resolve().parent.parent
MAX_TRACKED_FILE_BYTES = 10 * 1024 * 1024
MAX_SECRET_SCAN_BYTES = 2 * 1024 * 1024

ALLOWED_ENV_FILES = {".env.example"}
ALLOWED_PS1_PREFIX = "tools/"
ALLOWED_CMD_NAMES = {"START_DEMO.cmd", "STOP_DEMO.cmd"}
ALLOWED_CMD_PREFIX = "owner_review_packages/"

FORBIDDEN_EXACT_NAMES = {
    ".env",
    "id_rsa",
    "id_ed25519",
    "cookies.txt",
    "session.json",
    # Owner-provided MOEX files must remain outside Git. Synthetic fixtures must
    # use explicit synthetic/sample names instead of these official filenames.
    "configuration.xml",
    "templates.xml",
}

FORBIDDEN_SUFFIXES = {
    ".key",
    ".pem",
    ".pfx",
    ".p12",
    ".ppk",
    ".exe",
    ".dll",
    ".msi",
    ".so",
    ".dylib",
    ".jar",
    ".bin",
    ".dat",
    ".qsh",
    ".pcap",
    ".pcapng",
    ".db",
    ".sqlite",
    ".sqlite3",
    ".duckdb",
    ".parquet",
    ".feather",
    ".h5",
    ".hdf5",
    ".zip",
    ".rar",
    ".7z",
    ".tar",
    ".gz",
    ".tgz",
}

FORBIDDEN_PATH_PARTS = {
    "secrets",
    "private",
    "credentials",
    "__pycache__",
    ".venv",
    "venv",
    "node_modules",
    "cmakefiles",
    "cmake-build-debug",
    "cmake-build-release",
}

FORBIDDEN_PATH_PREFIXES = {
    "data/raw/",
    "data/private/",
    "data/live/",
    "data/broker/",
    "data/reports/",
    "data/ticks/",
    "data/orderbook/",
    "build/",
    "dist/",
    "output/",
    "runtime/",
}

HIGH_CONFIDENCE_SECRET_PATTERNS = {
    "private key block": re.compile(r"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----"),
    "GitHub classic token": re.compile(r"\bgh[pousr]_[A-Za-z0-9]{30,}\b"),
    "GitHub fine-grained token": re.compile(r"\bgithub_pat_[A-Za-z0-9_]{40,}\b"),
    "AWS access key": re.compile(r"\b(?:AKIA|ASIA)[A-Z0-9]{16}\b"),
    "Slack token": re.compile(r"\bxox[baprs]-[A-Za-z0-9-]{20,}\b"),
}

ASSIGNMENT_SECRET_PATTERN = re.compile(
    r"(?im)^\s*(?:API_KEY|ACCESS_TOKEN|REFRESH_TOKEN|SECRET|PASSWORD|PRIVATE_KEY)\s*[:=]\s*"
    r"[\"']?([^\s\"'#;]{12,})"
)

TEXT_SUFFIXES_FOR_ASSIGNMENT_SCAN = {
    ".py",
    ".cpp",
    ".cc",
    ".cxx",
    ".hpp",
    ".h",
    ".yaml",
    ".yml",
    ".toml",
    ".ini",
    ".cfg",
    ".conf",
    ".json",
    ".xml",
    ".cmd",
    ".ps1",
    ".sh",
}

SAFE_EXAMPLE_VALUES = {
    "put_key_here",
    "put_token_here",
    "put_secret_here",
    "change_me",
    "example",
    "placeholder",
    "dummy",
    "fake",
}


def git_candidate_paths() -> list[str]:
    """Return tracked plus non-ignored untracked paths."""
    result = subprocess.run(
        [
            "git",
            "ls-files",
            "--cached",
            "--others",
            "--exclude-standard",
            "-z",
        ],
        cwd=REPO_ROOT,
        check=True,
        stdout=subprocess.PIPE,
    )
    return sorted(
        path.decode("utf-8", errors="surrogateescape").replace("\\", "/")
        for path in result.stdout.split(b"\0")
        if path
    )


def is_allowed_cmd(path: str) -> bool:
    posix = PurePosixPath(path)
    return (
        path.startswith(ALLOWED_CMD_PREFIX)
        and posix.name in ALLOWED_CMD_NAMES
        and len(posix.parts) >= 3
    )


def is_allowed_sample_csv(path: str) -> bool:
    lower_path = path.lower()
    return lower_path.startswith("data/sample/") or lower_path.endswith(".sample.csv")


def filename_violations(path: str) -> list[str]:
    violations: list[str] = []
    posix = PurePosixPath(path)
    lower_path = path.lower()
    lower_name = posix.name.lower()
    lower_suffix = posix.suffix.lower()
    lower_parts = {part.lower() for part in posix.parts}

    if lower_name in FORBIDDEN_EXACT_NAMES:
        violations.append("forbidden exact filename")

    if lower_name.startswith(".env") and posix.name not in ALLOWED_ENV_FILES:
        violations.append("environment file is forbidden")

    if lower_suffix in FORBIDDEN_SUFFIXES:
        violations.append(f"forbidden file type {lower_suffix}")

    if lower_suffix == ".csv" and not is_allowed_sample_csv(path):
        violations.append("CSV is allowed only as explicit synthetic/sample data")

    if lower_suffix == ".ps1" and not lower_path.startswith(ALLOWED_PS1_PREFIX):
        violations.append("PowerShell scripts are allowed only under tools/")

    if lower_suffix in {".cmd", ".bat"} and not is_allowed_cmd(path):
        violations.append(
            "command scripts are allowed only as START_DEMO.cmd/STOP_DEMO.cmd "
            "inside owner_review_packages/issue-*/"
        )

    matched_parts = sorted(lower_parts & FORBIDDEN_PATH_PARTS)
    if matched_parts:
        violations.append(f"forbidden path component(s): {', '.join(matched_parts)}")

    for prefix in FORBIDDEN_PATH_PREFIXES:
        if lower_path.startswith(prefix):
            violations.append(f"forbidden path prefix: {prefix}")
            break

    return violations


def read_text_safely(path: Path, size: int) -> str | None:
    if size > MAX_SECRET_SCAN_BYTES:
        return None
    try:
        raw = path.read_bytes()
    except OSError:
        return None
    if b"\0" in raw:
        return None
    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError:
        return raw.decode("utf-8", errors="replace")


def secret_violations(path: str, text: str) -> list[str]:
    violations: list[str] = []

    for name, pattern in HIGH_CONFIDENCE_SECRET_PATTERNS.items():
        if pattern.search(text):
            violations.append(f"possible {name}")

    posix = PurePosixPath(path)
    if posix.suffix.lower() in TEXT_SUFFIXES_FOR_ASSIGNMENT_SCAN and posix.name != ".env.example":
        for match in ASSIGNMENT_SECRET_PATTERN.finditer(text):
            value = match.group(1).strip().lower()
            if value not in SAFE_EXAMPLE_VALUES and not value.startswith(("${", "<", "put_")):
                violations.append("possible non-placeholder secret assignment")
                break

    return violations


def main() -> int:
    try:
        paths = git_candidate_paths()
    except (OSError, subprocess.CalledProcessError) as exc:
        print(f"ERROR: cannot enumerate repository files: {exc}", file=sys.stderr)
        return 2

    violations: list[tuple[str, str]] = []
    scanned_files = 0

    for relative_path in paths:
        absolute_path = REPO_ROOT / relative_path
        if not absolute_path.exists() or not absolute_path.is_file():
            continue

        scanned_files += 1
        for reason in filename_violations(relative_path):
            violations.append((relative_path, reason))

        try:
            size = os.path.getsize(absolute_path)
        except OSError as exc:
            violations.append((relative_path, f"cannot read file size: {exc}"))
            continue

        if size > MAX_TRACKED_FILE_BYTES:
            violations.append(
                (
                    relative_path,
                    f"file size {size} bytes exceeds {MAX_TRACKED_FILE_BYTES} byte project limit",
                )
            )

        text = read_text_safely(absolute_path, size)
        if text is not None:
            for reason in secret_violations(relative_path, text):
                violations.append((relative_path, reason))

    if violations:
        print("Repository hygiene check: FAIL")
        for path, reason in sorted(set(violations)):
            print(f"  - {path}: {reason}")
        print()
        print("Do not commit or push until every finding is reviewed and removed.")
        return 1

    print("Repository hygiene check: PASS")
    print(f"Checked {scanned_files} tracked or non-ignored pending files.")
    print(f"Maximum allowed file size: {MAX_TRACKED_FILE_BYTES} bytes.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
