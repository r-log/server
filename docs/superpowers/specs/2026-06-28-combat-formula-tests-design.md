# Combat Melee Hit-Table Formula Extraction + Tests — Design

**Date:** 2026-06-28
**Branch:** `feat/test-framework` (origin = r-log/server; branch-only, NO upstream PR for now)
**Status:** Approved

## Goal

Land the **first real characterization test suite** on top of the doctest harness:
the Cata melee hit-table chance formulas (glancing, crushing, miss). Doing so also
**decouples** that arithmetic out of the `Unit::` god-class into pure, reusable
functions — the deliberate bridge from surface decomp toward the deep phase.

## Why this first (ordering)

Combat formulas maximize all four test-priority criteria simultaneously:
testable (pure math, no live World), **known-correct answers** (verified Cata %
in PRs #105–113), highest value (most-churned, most-bug-prone code), and they are
the leaf the deep phase builds on. See `project_test_framework.md` roadmap step B2.

## Approach (chosen)

**Extract pure formula functions.** The formulas are currently inline inside
`Unit::` methods that read live attacker/victim state, so they are untestable as-is.
We move the *arithmetic* into pure free functions; production calls them. This tests
the **real** production code (not a copy) and is a genuine decoupling win.

Rejected alternatives: *spec tests* (re-implement formula in test — tests a copy,
won't catch production drift); *fixture-based* (construct `Unit`s — impractical,
MaNGOS units need a live Map/World/DBC).

## Architecture

A new **header-only** unit `src/game/Object/CombatFormulas.h` with `inline` free
functions in `namespace CombatFormulas`. Header-only `inline` => no link/ODR
issues, and the test includes the *same* header the production code uses, so it
exercises the real functions. The `game` library is already linked into
`mangos_tests`; these functions don't even require it (header-only), but the test
target stays linked to `game` for the existing linkage test and future suites.

## Components — the three pure functions

Mirroring the current inline math **exactly** (basis points where the code uses
them; 10000 bp = 100%):

```cpp
namespace CombatFormulas
{
    // Glancing: 6% + 1.2% per skill point of (victimMaxSkill - attackerMaxSkill),
    // capped at 40%, floored at 0. (UnitCombat.cpp:327-328)
    //   600 + (victimMaxSkill - attackerMaxSkill) * 120, clamped to [0, 4000]
    inline int32 GlancingChanceBasisPoints(int32 attackerMaxSkill, int32 victimMaxSkill);

    // Crushing: max(attackerMaxSkill - victimMaxSkill, 20) * 2% - 15%.
    // May be <= 0; the caller gates on > 0. (UnitCombat.cpp:376-379)
    //   max(skillDiff, 20) * 200 - 1500
    inline int32 CrushingChanceBasisPoints(int32 attackerMaxSkill, int32 victimMaxSkill);

    // Melee miss: deterministic base 5% minus the skill-diff term, with the
    // PvP (victim is player) vs PvE branch. (UnitCombat.cpp:1023-1039)
    // Returns ONLY the base+skill component; production adds dual-wield/
    // hit-rating/aura modifiers on top.
    //   victimIsPlayer : 5.0 - skillDiff*0.04
    //   skillDiff < -10: 5.0 - ((skillDiff+10)*0.4 - 1.0)
    //   else           : 5.0 - skillDiff*0.1
    inline float SkillBasedMeleeMissChance(int32 skillDiff, bool victimIsPlayer);
}
```

## Production wiring (verbatim-equivalent; behavior identical)

- `Unit::RollMeleeOutcomeAgainst` (UnitCombat.cpp): replace the two inline
  glancing/crushing arithmetic blocks with calls. **The eligibility gates stay
  exactly in place** (player-vs-higher-mob, mob 4+ levels, `!IsNonMeleeSpellCasted`,
  `CREATURE_FLAG_EXTRA_NO_CRUSH`, etc.) — only the number-crunching moves.
- `Unit::MeleeMissChanceCalc` (UnitCombat.cpp): replace the base-5%/skill-diff
  block with `SkillBasedMeleeMissChance(...)`; dual-wield/hit/aura mods stay.

Equivalence is by construction (literal arithmetic move) and pinned by the tests.

## Tests — `tests/test_combat_formulas.cpp`

doctest `TEST_CASE`s asserting Cata-correct values at representative inputs:

- **Glancing** (`GlancingChanceBasisPoints`):
  - equal skill (diff 0) -> 600 (6%)
  - +1/+2/+3 levels (victim skill +5/+10/+15) -> 1200 / 1800 / 2400 (12/18/24%)
  - +6 NPC ceiling (victim skill +30) -> capped 4000 (40%)
  - attacker higher skill (negative diff) -> floored 0
- **Crushing** (`CrushingChanceBasisPoints`):
  - skill diff <= 20 (incl. the 4-level floor) -> 20*200-1500 = 2500 (25%)
  - skill diff 25 -> 25*200-1500 = 3500 (35%)
  - tiny diff still floors at 20 -> 2500
- **Miss** (`SkillBasedMeleeMissChance`):
  - skillDiff 0 -> 5.0%
  - vs +3 NPC boss (skillDiff -15) -> 5 - ((-15+10)*0.4 - 1) = 8.0%
  - vs player, skillDiff -15 -> 5 - (-15*0.04) = 5.6%
  - vs NPC, skillDiff -5 (>= -10 branch) -> 5 - (-5*0.1) = 5.5%

Use `doctest::Approx` for the float (miss) assertions.

## Build / verification

- New header + new test file. `tests/CMakeLists.txt` already GLOBs `*.cpp`;
  `game` already linked. Build `mangos_tests`, run `ctest -C Release` -> green.
- Build the full `game` target too, to confirm the production wiring compiles.

## Scope guardrails (YAGNI)

In scope: glancing / crushing / miss **chance** only. Out of scope (explicit
follow-ups): damage multipliers, `CalcArmorReducedDamage`, eligibility-gate
refactoring, and the CI job to run `ctest` (kept out while branch-only).
