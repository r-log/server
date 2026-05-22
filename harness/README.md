# vmap-harness — regression sampler for vmap spatial queries

Reproducible CLI that probes the same `VMapManager2` queries the live server
uses and emits a deterministic CSV. Every later vmap-redesign stage diffs its
output against the captured baseline — non-zero diff demands explanation.

Design: `MANGOS/STAGE0_PLAN.md`.
Query set mirrors TC's `.gps` GM command (`cs_misc.cpp:230-310`) so diffs are
interpretable in TC-vocabulary terms.

## Files

| File | Purpose |
|---|---|
| `samples_curated.csv` | ~50 hand-picked waypoints (capital fountains, bridges, dungeon portals, WMO interiors, Z-fall hotspots, liquid edges). Stable `sample_id` per row. |
| `baseline_curated_<sha>.csv` | Captured harness output tagged with the upstream commit SHA. The reference all future runs diff against. |
| `diff_runner.py` | Aligns baseline vs candidate by `sample_id`, classifies each delta. Exit 0 = no meaningful change. |

## Running

```sh
# 1. Build the tool (needs BUILD_TOOLS=ON; default ON in fresh checkouts).
cmake -S . -B build -DBUILD_TOOLS=ON
cmake --build build --config Release --target vmap-harness

# 2. Capture a fresh run.
build/src/tools/Extractor_projects/Release/vmap-harness.exe \
    --data-dir /path/to/server_install \
    --samples  server/harness/samples_curated.csv \
    --out      /tmp/run.csv

# 3. Diff against baseline.
python server/harness/diff_runner.py \
    --baseline  server/harness/baseline_curated_<sha>.csv \
    --candidate /tmp/run.csv
```

## Tolerance bands

Float fields (`height_vmap`, `liquid_level`) are compared with a 0.01 yard (1 cm)
tolerance. Below the threshold is FP noise. Above is real change.

Integer fields (`area_id`, `area_flags`, `liquid_type`) and the `los_blocked`
boolean are exact-equality compared.

`result` column ERR↔OK transitions are always meaningful.

## What we read

| Column | Source query |
|---|---|
| `height_vmap` | `IVMapManager::getHeight(mapId, x, y, z, 1000.0)` |
| `area_id` | `IVMapManager::getAreaInfo(...).rootId` |
| `area_flags` | `IVMapManager::getAreaInfo(...).flags` |
| `los_blocked` | `!IVMapManager::isInLineOfSight(mapId, x1,y1,z1, x2,y2,z2)` (only when CSV row has `los_x/y/z`) |
| `liquid_level` | `IVMapManager::GetLiquidLevel(...).level` |
| `liquid_type` | `IVMapManager::GetLiquidLevel(...).type` |

A `height_vmap` of `-200000.000` is `VMAP_INVALID_HEIGHT_VALUE` from
`IVMapManager.h:50` — the queried coord has no vmap structure data. This is
expected for samples in open terrain (vmap only knows about WMOs and M2
doodads). Stable `-200000` values are valid baseline data.

## Acceptance criteria (STAGE0_PLAN §11)

- [x] Builds clean with warnings-as-errors
- [x] Runs on `server_install/` without crashing
- [x] Produces `baseline_curated_<sha>.csv`
- [x] Re-run produces zero diff (determinism)
- [ ] Corrupt one byte of a `.vmtile` → diff appears (interactive demo, deferred)
- [x] `diff_runner.py` classifies regressions
- [x] Query set matches TC `.gps` (vmap side; `.map` terrain side a Stage 0b follow-up)

## Future work — Stage 0b

The current harness only probes `vmap` queries. TC's `.gps` also returns
`groundZ` from the `.map` terrain heightmap (via `GridMap`) and `haveMap` /
`haveMMap` file-presence flags. Adding these to the harness widens coverage but
needs the `GridMap` reader pulled into the tool (it currently lives behind
`mangosd`'s build target only). Captured as Stage 0b.
