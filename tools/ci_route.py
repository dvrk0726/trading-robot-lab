#!/usr/bin/env python3
"""CI-1 job routing by changed file paths.

Fail-closed: empty input, errors, unknown paths, and any path forcing a
sensitive/full-matrix change all route to the full seven-job matrix.

Designed to run inside GitHub Actions ``repository-hygiene`` with no
external dependencies.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import PurePosixPath


# ---------------------------------------------------------------------------
# Classification tables
# ---------------------------------------------------------------------------

# Extensions / directories that are documentation-only.
_DOCS_EXACT_EXTENSIONS: set[str] = {".md"}

_DOCS_DIR_PREFIXES: tuple[str, ...] = (
    "docs/",
    "decisions/",
    "strategy_knowledge_base/",
    "agent_workspaces/",
)

# C++ contour directories → which C++ jobs to enable.
_CPP_FAST_PREFIXES: tuple[str, ...] = ("cpp/moex_fast/",)
_CPP_RAW_PREFIXES: tuple[str, ...] = ("cpp/moex_raw/",)
_CPP_QSH_PREFIXES: tuple[str, ...] = ("cpp/qsh_ingest/",)

# Python contour directories.
_PYTHON_PREFIXES: tuple[str, ...] = (
    "src/",
    "tests/",
    "shared/",
    "tools/",
    "apps/",
)

# Single file that triggers both Python and QSH.
_MIMO_SAVE_PS1 = "tools/mimo_save.ps1"

# Paths that force the full matrix (any change = run everything).
_FULL_MATRIX_EXACT_PATHS: set[str] = {
    "tools/ci_route.py",
    "tests/test_ci_route.py",
}

_FULL_MATRIX_PREFIXES: tuple[str, ...] = (
    ".github/workflows/",
    ".github/actions/",
    "cmake/",
)

_FULL_MATRIX_ROOT_FILES: set[str] = {
    "CMakeLists.txt",
    "CMakePresets.json",
    "vcpkg.json",
    "vcpkg-configuration.json",
}


# ---------------------------------------------------------------------------
# Path validation
# ---------------------------------------------------------------------------

_CONTROL_RE = re.compile(r"[\x00-\x1f\x7f]")


def _is_valid_path(path: str) -> bool:
    """Return ``True`` if *path* is a valid relative POSIX path.

    Invalid (→ full matrix): absolute path, backslash, empty component,
    ``.`` or ``..`` component, NUL/control characters, trailing slash.
    """
    if not path or path.endswith("/"):
        return False
    if path.startswith("/"):
        return False
    if "\\" in path:
        return False
    if _CONTROL_RE.search(path):
        return False
    for part in path.split("/"):
        if part in ("", ".", ".."):
            return False
    return True


# ---------------------------------------------------------------------------
# Single-path classification
# ---------------------------------------------------------------------------

def _classify_single(path: str) -> set[str]:
    """Return a set of contour tags for one changed path.

    Tags: ``"full"``, ``"fast"``, ``"raw"``, ``"qsh"``, ``"python"``, ``"docs"``.

    Unknown paths → ``{"full"}`` (fail-closed).
    """
    if not _is_valid_path(path):
        return {"full"}

    posix = PurePosixPath(path)
    suffix = posix.suffix.lower()
    parts = posix.parts  # e.g. ("cpp", "moex_fast", "foo.cpp")

    # --- Full-matrix triggers ------------------------------------------------
    if path in _FULL_MATRIX_EXACT_PATHS:
        return {"full"}

    for prefix in _FULL_MATRIX_PREFIXES:
        if path.startswith(prefix):
            return {"full"}

    if len(parts) == 1 and path in _FULL_MATRIX_ROOT_FILES:
        return {"full"}

    # --- C++ contours ---------------------------------------------------------
    for prefix in _CPP_FAST_PREFIXES:
        if path.startswith(prefix):
            return {"fast"}

    for prefix in _CPP_RAW_PREFIXES:
        if path.startswith(prefix):
            return {"raw"}

    for prefix in _CPP_QSH_PREFIXES:
        if path.startswith(prefix):
            return {"qsh"}

    # Any other cpp/** is uncategorised → fail-closed.
    if parts and parts[0] == "cpp":
        return {"full"}

    # --- Documentation-only ---------------------------------------------------
    if suffix in _DOCS_EXACT_EXTENSIONS:
        return {"docs"}

    for prefix in _DOCS_DIR_PREFIXES:
        if path.startswith(prefix):
            return {"docs"}

    # --- Python contour -------------------------------------------------------
    if path == _MIMO_SAVE_PS1:
        return {"python", "qsh"}

    for prefix in _PYTHON_PREFIXES:
        if path.startswith(prefix):
            return {"python"}

    # --- Everything else is unknown → fail-closed -----------------------------
    return {"full"}


# ---------------------------------------------------------------------------
# Multi-path routing
# ---------------------------------------------------------------------------

def route(paths: list[str]) -> dict[str, bool]:
    """Compute routing flags from a list of changed file paths.

    Returns a dict with keys ``run_python``, ``run_qsh``, ``run_fast``,
    ``run_raw``, and ``full_matrix`` (all ``bool``).
    """
    # Empty or absent input → full matrix (fail-closed).
    if not paths:
        return {
            "full_matrix": True,
            "run_python": True,
            "run_qsh": True,
            "run_fast": True,
            "run_raw": True,
        }

    tags: set[str] = set()
    for path in paths:
        tags |= _classify_single(path)

    # If any path triggered full, run everything.
    if "full" in tags:
        return {
            "full_matrix": True,
            "run_python": True,
            "run_qsh": True,
            "run_fast": True,
            "run_raw": True,
        }

    # If only documentation paths were changed, skip all expensive jobs.
    if tags == {"docs"}:
        return {
            "full_matrix": False,
            "run_python": False,
            "run_qsh": False,
            "run_fast": False,
            "run_raw": False,
        }

    return {
        "full_matrix": False,
        "run_python": "python" in tags,
        "run_qsh": "qsh" in tags,
        "run_fast": "fast" in tags,
        "run_raw": "raw" in tags,
    }


# ---------------------------------------------------------------------------
# GitHub Actions output writer
# ---------------------------------------------------------------------------

_TRUE_FALSE = {True: "true", False: "false"}


def write_github_output(path: str, flags: dict[str, bool]) -> None:
    """Append routing flags as ``key=value`` lines to *path*."""
    with open(path, "a", encoding="utf-8") as fh:
        for key in ("run_python", "run_qsh", "run_fast", "run_raw", "full_matrix"):
            fh.write(f"{key}={_TRUE_FALSE[flags[key]]}\n")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="CI job router")
    parser.add_argument(
        "--paths-file",
        required=True,
        help="File containing one changed path per line.",
    )
    parser.add_argument(
        "--force-full",
        action="store_true",
        help="Force full matrix regardless of paths.",
    )
    parser.add_argument(
        "--github-output",
        default=None,
        help="Path to $GITHUB_OUTPUT file for writing routing flags.",
    )
    args = parser.parse_args(argv)

    # --force-full takes priority — apply before reading paths-file.
    if args.force_full:
        flags: dict[str, bool] = {
            "full_matrix": True,
            "run_python": True,
            "run_qsh": True,
            "run_fast": True,
            "run_raw": True,
        }
    else:
        # Read paths from file.  Errors → full matrix (fail-closed, exit 0).
        try:
            with open(args.paths_file, encoding="utf-8") as fh:
                paths = [line.strip() for line in fh if line.strip()]
        except OSError as exc:
            print(
                f"WARNING: cannot read paths file: {exc}; "
                "routing to full matrix",
                file=sys.stderr,
            )
            flags = {
                "full_matrix": True,
                "run_python": True,
                "run_qsh": True,
                "run_fast": True,
                "run_raw": True,
            }
            if args.github_output:
                write_github_output(args.github_output, flags)
            for key in ("full_matrix", "run_python", "run_qsh", "run_fast", "run_raw"):
                print(f"{key}={_TRUE_FALSE[flags[key]]}")
            return 0

        flags = route(paths)

    if args.github_output:
        write_github_output(args.github_output, flags)

    # Always print to stdout for local debugging.
    for key in ("full_matrix", "run_python", "run_qsh", "run_fast", "run_raw"):
        print(f"{key}={_TRUE_FALSE[flags[key]]}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
