#!/usr/bin/env python3
"""Tests for tools/ci_route.py — CI-1 job routing by changed files."""

from __future__ import annotations

import importlib.util
import os
import pathlib
import tempfile

import pytest

# ---------------------------------------------------------------------------
# Import the module under test without requiring a package __init__.
# ---------------------------------------------------------------------------
_MODULE_PATH = pathlib.Path(__file__).resolve().parent.parent / "tools" / "ci_route.py"
_spec = importlib.util.spec_from_file_location("ci_route", _MODULE_PATH)
_mod = importlib.util.module_from_spec(_spec)
assert _spec.loader is not None
_spec.loader.exec_module(_mod)

route = _mod.route
main = _mod.main

# All keys returned by ``route()``.
ALL_KEYS = {"full_matrix", "run_python", "run_fast", "run_raw", "run_pipeline"}


# ---------------------------------------------------------------------------
# Helper
# ---------------------------------------------------------------------------

def _assert_flags(actual: dict[str, bool], **expected: bool) -> None:
    """Assert that *actual* flags match *expected*.

    When ``full_matrix=True`` is expected, all other keys are expected True too
    (the routing function returns all-on for full matrix).
    """
    full = expected.get("full_matrix", False)
    for key in ALL_KEYS:
        if key == "full_matrix":
            assert actual["full_matrix"] is full, f"full_matrix mismatch: {actual}"
        else:
            if full:
                assert actual[key] is True, f"{key} should be True when full_matrix: {actual}"
            else:
                want = expected.get(key, False)
                assert actual[key] is want, f"{key} mismatch: {actual}"


# ---------------------------------------------------------------------------
# Individual routing scenarios
# ---------------------------------------------------------------------------

class TestDocsOnly:
    def test_markdown_root(self) -> None:
        _assert_flags(route(["README.md"]), docs=True)

    def test_docs_directory(self) -> None:
        _assert_flags(route(["docs/architecture.md"]), docs=True)

    def test_decisions_directory(self) -> None:
        _assert_flags(route(["decisions/ADR-0001.md"]), docs=True)

    def test_strategy_knowledge_base(self) -> None:
        _assert_flags(route(["strategy_knowledge_base/notes.md"]), docs=True)

    def test_agent_workspaces(self) -> None:
        _assert_flags(route(["agent_workspaces/mimo/report.md"]), docs=True)

    def test_multiple_docs_files(self) -> None:
        paths = ["README.md", "docs/guide.md", "decisions/ADR-0002.md"]
        _assert_flags(route(paths), docs=True)


class TestFastOnly:
    def test_single_fast_file(self) -> None:
        _assert_flags(route(["cpp/moex_fast/src/decoder.cpp"]), run_fast=True, run_pipeline=True)

    def test_fast_cmake(self) -> None:
        _assert_flags(route(["cpp/moex_fast/CMakeLists.txt"]), run_fast=True, run_pipeline=True)

    def test_multiple_fast_files(self) -> None:
        paths = ["cpp/moex_fast/src/a.cpp", "cpp/moex_fast/include/b.hpp"]
        _assert_flags(route(paths), run_fast=True, run_pipeline=True)


class TestRawOnly:
    def test_single_raw_file(self) -> None:
        _assert_flags(route(["cpp/moex_raw/src/reader.cpp"]), run_raw=True, run_pipeline=True)

    def test_multiple_raw_files(self) -> None:
        paths = ["cpp/moex_raw/src/a.cpp", "cpp/moex_raw/include/b.hpp"]
        _assert_flags(route(paths), run_raw=True, run_pipeline=True)


class TestPythonOnly:
    def test_src(self) -> None:
        _assert_flags(route(["src/strategy.py"]), run_python=True)

    def test_tests(self) -> None:
        _assert_flags(route(["tests/test_example.py"]), run_python=True)

    def test_shared(self) -> None:
        _assert_flags(route(["shared/schemas/schema.json"]), run_python=True)

    def test_tools(self) -> None:
        _assert_flags(route(["tools/some_tool.py"]), run_python=True)

    def test_apps(self) -> None:
        _assert_flags(route(["apps/app.py"]), run_python=True)

    def test_multiple_python_paths(self) -> None:
        paths = ["src/a.py", "tests/b.py", "shared/c.py"]
        _assert_flags(route(paths), run_python=True)


class TestMimoSave:
    def test_mimo_save_triggers_python_only(self) -> None:
        _assert_flags(route(["tools/mimo_save.ps1"]), run_python=True)

    def test_mimo_save_combined_with_other(self) -> None:
        paths = ["tools/mimo_save.ps1", "cpp/moex_fast/src/x.cpp"]
        _assert_flags(route(paths), run_python=True, run_fast=True, run_pipeline=True)


class TestFastPlusPython:
    def test_fast_and_python(self) -> None:
        paths = ["cpp/moex_fast/src/decoder.cpp", "src/strategy.py"]
        _assert_flags(route(paths), run_fast=True, run_python=True, run_pipeline=True)

    def test_raw_and_python(self) -> None:
        paths = ["cpp/moex_raw/src/reader.cpp", "tests/test_x.py"]
        _assert_flags(route(paths), run_raw=True, run_python=True, run_pipeline=True)


class TestFullMatrixTriggers:
    def test_workflow_change(self) -> None:
        _assert_flags(route([".github/workflows/ci.yml"]), full_matrix=True)

    def test_routing_script_change(self) -> None:
        _assert_flags(route(["tools/ci_route.py"]), full_matrix=True)

    def test_routing_test_change(self) -> None:
        _assert_flags(route(["tests/test_ci_route.py"]), full_matrix=True)

    def test_actions_directory(self) -> None:
        _assert_flags(route([".github/actions/setup/action.yml"]), full_matrix=True)

    def test_cmake_directory(self) -> None:
        _assert_flags(route(["cmake/Modules/FindZLIB.cmake"]), full_matrix=True)

    def test_root_cmake_lists(self) -> None:
        _assert_flags(route(["CMakeLists.txt"]), full_matrix=True)

    def test_cmake_presets(self) -> None:
        _assert_flags(route(["CMakePresets.json"]), full_matrix=True)

    def test_vcpkg_json(self) -> None:
        _assert_flags(route(["vcpkg.json"]), full_matrix=True)

    def test_vcpkg_configuration(self) -> None:
        _assert_flags(route(["vcpkg-configuration.json"]), full_matrix=True)

    def test_unknown_path(self) -> None:
        _assert_flags(route(["random/unknown_file.xyz"]), full_matrix=True)

    def test_unknown_cpp_path(self) -> None:
        _assert_flags(route(["cpp/new_contour/src/main.cpp"]), full_matrix=True)

    def test_root_unknown_file(self) -> None:
        _assert_flags(route(["Makefile"]), full_matrix=True)


class TestEmptyInput:
    def test_empty_list(self) -> None:
        _assert_flags(route([]), full_matrix=True)


class TestForcedFullMatrix:
    def test_force_full_overrides_docs(self) -> None:
        """--force-full must produce the full matrix even for docs-only changes."""
        paths_file = tempfile.NamedTemporaryFile(
            mode="w", suffix=".txt", delete=False, encoding="utf-8"
        )
        paths_file.write("README.md\n")
        paths_file.close()
        try:
            output_file = tempfile.NamedTemporaryFile(
                mode="w", suffix=".txt", delete=False, encoding="utf-8"
            )
            output_file.close()
            ret = main([
                "--paths-file", paths_file.name,
                "--force-full",
                "--github-output", output_file.name,
            ])
            assert ret == 0
            with open(output_file.name, encoding="utf-8") as fh:
                lines = fh.read().strip().splitlines()
            values = dict(line.split("=", 1) for line in lines)
            assert values["full_matrix"] == "true"
            assert values["run_python"] == "true"
            assert values["run_fast"] == "true"
            assert values["run_raw"] == "true"
            assert values["run_pipeline"] == "true"
        finally:
            os.unlink(paths_file.name)
            os.unlink(output_file.name)


class TestGitHubOutput:
    def test_writes_correct_output(self) -> None:
        """Verify that --github-output writes the expected key=value pairs."""
        paths_file = tempfile.NamedTemporaryFile(
            mode="w", suffix=".txt", delete=False, encoding="utf-8"
        )
        paths_file.write("src/app.py\n")
        paths_file.close()
        output_file = tempfile.NamedTemporaryFile(
            mode="w", suffix=".txt", delete=False, encoding="utf-8"
        )
        output_file.close()
        try:
            ret = main([
                "--paths-file", paths_file.name,
                "--github-output", output_file.name,
            ])
            assert ret == 0
            with open(output_file.name, encoding="utf-8") as fh:
                lines = fh.read().strip().splitlines()
            values = dict(line.split("=", 1) for line in lines)
            assert values["full_matrix"] == "false"
            assert values["run_python"] == "true"
            assert values["run_fast"] == "false"
            assert values["run_raw"] == "false"
            assert values["run_pipeline"] == "false"
        finally:
            os.unlink(paths_file.name)
            os.unlink(output_file.name)


class TestMultiContourUnion:
    def test_fast_raw_python_union(self) -> None:
        """Multiple known contours union to all expensive jobs."""
        paths = [
            "cpp/moex_fast/src/a.cpp",
            "cpp/moex_raw/src/b.cpp",
            "src/d.py",
        ]
        _assert_flags(
            route(paths),
            run_fast=True,
            run_raw=True,
            run_python=True,
            run_pipeline=True,
        )

    def test_full_matrix_overrides_everything(self) -> None:
        """A single full-matrix trigger forces all jobs on even with known paths."""
        paths = [
            "README.md",
            "cpp/moex_fast/src/a.cpp",
            "src/b.py",
            ".github/workflows/ci.yml",  # triggers full
        ]
        _assert_flags(route(paths), full_matrix=True)


class TestEdgeCases:
    def test_deeply_nested_doc_path(self) -> None:
        _assert_flags(route(["docs/a/b/c/d.md"]), docs=True)

    def test_cpp_root_level_file(self) -> None:
        """cpp/ root file (not under a known subdirectory) → full matrix."""
        _assert_flags(route(["cpp/README.md"]), full_matrix=True)

    def test_multiple_full_matrix_paths(self) -> None:
        paths = [
            ".github/workflows/ci.yml",
            "tools/ci_route.py",
            "tests/test_ci_route.py",
        ]
        _assert_flags(route(paths), full_matrix=True)

    def test_deeply_nested_fast_path(self) -> None:
        _assert_flags(route(["cpp/moex_fast/deep/nested/file.cpp"]), run_fast=True, run_pipeline=True)

    def test_docs_and_python_combined(self) -> None:
        """Docs + Python → Python enabled, not docs-only."""
        paths = ["README.md", "src/app.py"]
        _assert_flags(route(paths), run_python=True)


# ---------------------------------------------------------------------------
# Path validation (invalid paths → full matrix)
# ---------------------------------------------------------------------------

class TestPathValidation:
    def test_absolute_path(self) -> None:
        _assert_flags(route(["/etc/passwd"]), full_matrix=True)

    def test_backslash_path(self) -> None:
        _assert_flags(route(["src\\file.py"]), full_matrix=True)

    def test_dotdot_component(self) -> None:
        _assert_flags(route(["../secret/file.txt"]), full_matrix=True)

    def test_internal_dotdot(self) -> None:
        _assert_flags(route(["src/../../../etc/passwd"]), full_matrix=True)

    def test_nul_character(self) -> None:
        _assert_flags(route(["src/file\x00.py"]), full_matrix=True)

    def test_control_character(self) -> None:
        _assert_flags(route(["src/file\x01.py"]), full_matrix=True)

    def test_trailing_slash(self) -> None:
        _assert_flags(route(["src/"]), full_matrix=True)

    def test_empty_component(self) -> None:
        _assert_flags(route(["src//file.py"]), full_matrix=True)

    def test_dot_component(self) -> None:
        _assert_flags(route(["./src/file.py"]), full_matrix=True)

    def test_internal_dot(self) -> None:
        _assert_flags(route(["src/./file.py"]), full_matrix=True)


# ---------------------------------------------------------------------------
# Missing paths-file and --force-full edge cases
# ---------------------------------------------------------------------------

class TestMissingPathsFile:
    def test_missing_paths_file_full_matrix(self) -> None:
        """Missing paths-file without --force-full → full matrix, exit 0."""
        output_file = tempfile.NamedTemporaryFile(
            mode="w", suffix=".txt", delete=False, encoding="utf-8"
        )
        output_file.close()
        try:
            ret = main([
                "--paths-file", "/nonexistent/paths.txt",
                "--github-output", output_file.name,
            ])
            assert ret == 0
            with open(output_file.name, encoding="utf-8") as fh:
                lines = fh.read().strip().splitlines()
            values = dict(line.split("=", 1) for line in lines)
            assert values["full_matrix"] == "true"
            assert values["run_python"] == "true"
            assert values["run_fast"] == "true"
            assert values["run_raw"] == "true"
            assert values["run_pipeline"] == "true"
        finally:
            os.unlink(output_file.name)

    def test_missing_paths_file_with_force_full(self) -> None:
        """--force-full with missing paths-file → full matrix, exit 0."""
        output_file = tempfile.NamedTemporaryFile(
            mode="w", suffix=".txt", delete=False, encoding="utf-8"
        )
        output_file.close()
        try:
            ret = main([
                "--paths-file", "/nonexistent/paths.txt",
                "--force-full",
                "--github-output", output_file.name,
            ])
            assert ret == 0
            with open(output_file.name, encoding="utf-8") as fh:
                lines = fh.read().strip().splitlines()
            values = dict(line.split("=", 1) for line in lines)
            assert values["full_matrix"] == "true"
            assert values["run_python"] == "true"
            assert values["run_fast"] == "true"
            assert values["run_raw"] == "true"
            assert values["run_pipeline"] == "true"
        finally:
            os.unlink(output_file.name)


# ---------------------------------------------------------------------------
# Pipeline routing
# ---------------------------------------------------------------------------

class TestPipelineOnly:
    def test_single_pipeline_file(self) -> None:
        _assert_flags(route(["cpp/moex_spectra_pipeline/src/ordered_decode.cpp"]), run_pipeline=True)

    def test_pipeline_cmake(self) -> None:
        _assert_flags(route(["cpp/moex_spectra_pipeline/CMakeLists.txt"]), run_pipeline=True)

    def test_multiple_pipeline_files(self) -> None:
        paths = ["cpp/moex_spectra_pipeline/src/a.cpp", "cpp/moex_spectra_pipeline/include/b.hpp"]
        _assert_flags(route(paths), run_pipeline=True)

    def test_pipeline_test_file(self) -> None:
        _assert_flags(route(["cpp/moex_spectra_pipeline/tests/test_ordered_decode.cpp"]), run_pipeline=True)


class TestPipelineTriggers:
    def test_fast_triggers_pipeline(self) -> None:
        _assert_flags(route(["cpp/moex_fast/src/decoder.cpp"]), run_fast=True, run_pipeline=True)

    def test_raw_triggers_pipeline(self) -> None:
        _assert_flags(route(["cpp/moex_raw/src/reader.cpp"]), run_raw=True, run_pipeline=True)

    def test_pipeline_and_python(self) -> None:
        paths = ["cpp/moex_spectra_pipeline/src/a.cpp", "src/app.py"]
        _assert_flags(route(paths), run_pipeline=True, run_python=True)

    def test_fast_raw_pipeline_union(self) -> None:
        paths = ["cpp/moex_fast/src/a.cpp", "cpp/moex_raw/src/b.cpp"]
        _assert_flags(route(paths), run_fast=True, run_raw=True, run_pipeline=True)
