#!/usr/bin/env python3
"""Focused checks for absolute z80opt size limits."""

from __future__ import annotations

import tempfile
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from gen_size_report import check_policy_limits  # noqa: E402


def main() -> int:
    report = {
        "code": {"resident_bytes": 100},
        "memory": {"register_sp_gap_bytes": 50},
    }
    with tempfile.TemporaryDirectory(prefix="size_policy_") as temp_name:
        policy = Path(temp_name) / "policy.toml"
        policy.write_text(
            "[targets.zx]\nresident_ceiling=100\nstack_gap_floor=50\n",
            encoding="ascii",
        )
        assert not check_policy_limits(report, policy, "zx")
        report["code"]["resident_bytes"] = 101
        assert "exceeds" in check_policy_limits(report, policy, "zx")[0]
        report["code"]["resident_bytes"] = 100
        report["memory"]["register_sp_gap_bytes"] = 49
        assert "below" in check_policy_limits(report, policy, "zx")[0]
    print("z80opt absolute size policy checks ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
