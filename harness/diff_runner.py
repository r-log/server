#!/usr/bin/env python3
# vmap-harness diff runner.
#
# Aligns a baseline CSV against a candidate CSV by sample_id, classifies each
# delta, and exits non-zero if any meaningful change is detected.
#
# Tolerance band on float columns: 0.01 yards (1 cm). Anything below is
# floating-point noise; above is real change. Per STAGE0_PLAN.md §14.
#
# Usage:
#   python diff_runner.py --baseline baseline_curated_<sha>.csv --candidate run.csv
#
# Exit codes:
#   0 = all rows IDENTICAL or WITHIN_TOLERANCE (no meaningful change)
#   1 = at least one MEANINGFUL_CHANGE or ERROR_REGRESSION
#   2 = harness-internal error (file missing, columns mismatch, etc.)

import argparse
import csv
import sys


FLOAT_TOLERANCE = 0.01  # yards; values closer than this are FP noise

# Columns to compare. Order matches the harness output header.
FLOAT_COLS = ["height_vmap", "liquid_level"]
INT_COLS   = ["area_id", "area_flags", "liquid_type"]
BOOL_COLS  = ["los_blocked"]


def load_csv(path):
    with open(path, newline="") as fh:
        rows = list(csv.DictReader(fh))
    return {r["sample_id"]: r for r in rows}


def parse_float(s):
    return float(s) if s not in ("", None) else None


def classify_pair(field, base_val, cand_val):
    """Return ('IDENTICAL'|'WITHIN_TOLERANCE'|'MEANINGFUL_CHANGE', detail)."""
    if base_val == cand_val:
        return ("IDENTICAL", None)
    if field in FLOAT_COLS:
        bf = parse_float(base_val)
        cf = parse_float(cand_val)
        if bf is None or cf is None:
            return ("MEANINGFUL_CHANGE", f"{field}: '{base_val}' -> '{cand_val}' (absent vs present)")
        delta = abs(bf - cf)
        if delta <= FLOAT_TOLERANCE:
            return ("WITHIN_TOLERANCE", f"{field}: |Δ|={delta:.6f} <= {FLOAT_TOLERANCE}")
        return ("MEANINGFUL_CHANGE", f"{field}: {bf:.3f} -> {cf:.3f} (|Δ|={delta:.3f})")
    return ("MEANINGFUL_CHANGE", f"{field}: '{base_val}' -> '{cand_val}'")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--baseline",  required=True)
    p.add_argument("--candidate", required=True)
    args = p.parse_args()

    try:
        base = load_csv(args.baseline)
        cand = load_csv(args.candidate)
    except FileNotFoundError as exc:
        print(f"diff_runner: {exc}", file=sys.stderr)
        return 2

    only_in_base = set(base) - set(cand)
    only_in_cand = set(cand) - set(base)
    if only_in_base:
        print(f"diff_runner: ERROR — sample_ids in baseline only: {sorted(only_in_base)[:5]}")
        return 2
    if only_in_cand:
        print(f"diff_runner: ERROR — sample_ids in candidate only: {sorted(only_in_cand)[:5]}")
        return 2

    summary = {
        "IDENTICAL":         0,
        "WITHIN_TOLERANCE":  0,
        "MEANINGFUL_CHANGE": 0,
        "ERROR_REGRESSION":  0,
    }
    meaningful_details = []

    for sid in sorted(base):
        b = base[sid]
        c = cand[sid]

        # ERR↔OK transitions are always meaningful
        if b["result"] != c["result"]:
            summary["ERROR_REGRESSION"] += 1
            meaningful_details.append(f"{sid}: result {b['result']} -> {c['result']} ({c.get('notes', '')})")
            continue

        row_changes = []
        for col in FLOAT_COLS + INT_COLS + BOOL_COLS:
            cls, detail = classify_pair(col, b.get(col, ""), c.get(col, ""))
            if cls == "IDENTICAL":
                continue
            if cls == "WITHIN_TOLERANCE":
                summary["WITHIN_TOLERANCE"] += 1
                continue
            row_changes.append(detail)

        if row_changes:
            summary["MEANINGFUL_CHANGE"] += 1
            meaningful_details.append(f"{sid}: " + "; ".join(row_changes))
        else:
            summary["IDENTICAL"] += 1

    print(f"diff_runner: {len(base)} rows compared")
    for k, v in summary.items():
        print(f"  {k:18s} {v}")

    if meaningful_details:
        print("\nMeaningful changes:")
        for line in meaningful_details[:50]:
            print(f"  {line}")
        if len(meaningful_details) > 50:
            print(f"  ... and {len(meaningful_details) - 50} more")

    return 0 if (summary["MEANINGFUL_CHANGE"] == 0 and summary["ERROR_REGRESSION"] == 0) else 1


if __name__ == "__main__":
    sys.exit(main())
