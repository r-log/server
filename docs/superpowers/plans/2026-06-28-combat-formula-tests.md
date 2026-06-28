# Combat Melee Hit-Table Formula Tests — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract the Cata melee hit-table chance arithmetic (glancing, crushing, miss) from `Unit::` into a pure, unit-tested header, with behavior identical to today.

**Architecture:** New header-only `src/game/Object/CombatFormulas.h` with `inline` free functions in `namespace CombatFormulas`. `UnitCombat.cpp` calls them; `tests/test_combat_formulas.cpp` (doctest) asserts verified Cata values against the same functions. Production eligibility gates and state-dependent modifiers stay in `Unit::`.

**Tech Stack:** C++17, doctest 2.4.11 (`tests/doctest/doctest.h`), CMake (`BUILD_TESTS=ON`), MSVC local build.

## Global Constraints

- Branch `feat/test-framework` only (origin = r-log/server). **No PR to upstream mangosthree master.**
- MaNGOS license header (23 lines, `Copyright (C) 2005-2025`) on every new `.h`/`.cpp` — copy verbatim from `src/game/Object/Unit.cpp` lines 1-23.
- Production edits are **verbatim-equivalent** arithmetic moves: zero behavior change.
- `int32` comes from `#include "Platform/Define.h"`.
- Commit convention: `[Tests] Short title`. **No AI attribution** (no Co-Authored-By, no "Generated with").
- Build/run commands (PowerShell, from repo root `C:\Users\Roko\Documents\PYTHON\MANGOS\server`):
  - Re-GLOB after adding a test file: `cmake build -DBUILD_TESTS=ON`
  - Compile production wiring: `cmake --build build --target game --config Release`
  - Build tests: `cmake --build build --target mangos_tests --config Release`
  - Run: `build\bin\Release\mangos_tests.exe` (or `cd build; ctest -C Release --output-on-failure`)

---

## Task 1: Glancing chance + CombatFormulas.h skeleton

**Files:**
- Create: `src/game/Object/CombatFormulas.h`
- Create: `tests/test_combat_formulas.cpp`
- Modify: `src/game/Object/UnitCombat.cpp` (add include; replace glancing block ~lines 324-328)

**Interfaces:**
- Produces: `namespace CombatFormulas { int32 GlancingChanceBasisPoints(int32 attackerMaxSkill, int32 victimMaxSkill); }`

- [ ] **Step 1: Write the failing test** — create `tests/test_combat_formulas.cpp`:

```cpp
<23-line MaNGOS license header copied verbatim from src/game/Object/Unit.cpp:1-23>

/**
 * @file test_combat_formulas.cpp
 * @brief Characterization tests for the pure Cata melee hit-table formulas.
 */

#include "doctest/doctest.h"
#include "CombatFormulas.h"

using namespace CombatFormulas;

TEST_CASE("combat: glancing chance (basis points)")
{
    CHECK(GlancingChanceBasisPoints(100, 100) == 600);   // equal skill -> 6%
    CHECK(GlancingChanceBasisPoints(100, 105) == 1200);  // +1 level (victim +5 skill) -> 12%
    CHECK(GlancingChanceBasisPoints(100, 110) == 1800);  // +2 -> 18%
    CHECK(GlancingChanceBasisPoints(100, 115) == 2400);  // +3 -> 24%
    CHECK(GlancingChanceBasisPoints(100, 130) == 4000);  // +6 NPC ceiling -> capped 40%
    CHECK(GlancingChanceBasisPoints(100, 200) == 4000);  // far above -> still capped
}
```

- [ ] **Step 2: Run to verify it fails** — `cmake build -DBUILD_TESTS=ON` then `cmake --build build --target mangos_tests --config Release`. Expected: FAIL — `Cannot open include file: 'CombatFormulas.h'`.

- [ ] **Step 3: Create the header** — `src/game/Object/CombatFormulas.h`:

```cpp
<23-line MaNGOS license header copied verbatim from src/game/Object/Unit.cpp:1-23>

/**
 * @file CombatFormulas.h
 * @brief Pure Cataclysm melee hit-table chance formulas, extracted from Unit
 *        combat code so they can be unit-tested in isolation. Header-only inline
 *        free functions; no engine dependencies. Eligibility gates (who can
 *        glance/crush) and state-dependent modifiers stay in Unit::.
 */

#ifndef MANGOS_COMBATFORMULAS_H
#define MANGOS_COMBATFORMULAS_H

#include "Platform/Define.h"

namespace CombatFormulas
{
    /// Glancing chance in basis points (10000 = 100%): 6% + 1.2% per skill point
    /// of (victimMaxSkill - attackerMaxSkill), capped at 40% (4000 bp).
    inline int32 GlancingChanceBasisPoints(int32 attackerMaxSkill, int32 victimMaxSkill)
    {
        int32 chance = 600 + (victimMaxSkill - attackerMaxSkill) * 120;
        if (chance > 4000)
            chance = 4000;
        return chance;
    }
}

#endif // MANGOS_COMBATFORMULAS_H
```

- [ ] **Step 4: Run to verify it passes** — `cmake --build build --target mangos_tests --config Release` then `build\bin\Release\mangos_tests.exe`. Expected: PASS (glancing + earlier sanity/linkage cases all green).

- [ ] **Step 5: Wire production** — in `src/game/Object/UnitCombat.cpp` add `#include "CombatFormulas.h"` with the other includes, then replace this block (the glancing arithmetic in `RollMeleeOutcomeAgainst`):

```cpp
        // cap possible value (with bonuses > max skill)
        int32 skill = attackerMaxSkillValueForLevel;

        tmp = 600 + (victimMaxSkillValueForLevel - skill) * 120;
        tmp = tmp > 4000 ? 4000 : tmp;
```

with:

```cpp
        tmp = CombatFormulas::GlancingChanceBasisPoints(attackerMaxSkillValueForLevel, victimMaxSkillValueForLevel);
```

- [ ] **Step 6: Build production** — `cmake --build build --target game --config Release`. Expected: compiles clean (game.lib links).

- [ ] **Step 7: Commit**

```bash
git add src/game/Object/CombatFormulas.h tests/test_combat_formulas.cpp src/game/Object/UnitCombat.cpp
git commit -m "[Tests] Extract glancing-chance formula to CombatFormulas.h + tests"
```

---

## Task 2: Crushing chance

**Files:**
- Modify: `src/game/Object/CombatFormulas.h` (add function)
- Modify: `tests/test_combat_formulas.cpp` (add test case)
- Modify: `src/game/Object/UnitCombat.cpp` (replace crushing block ~lines 376-379)

**Interfaces:**
- Consumes: `CombatFormulas` namespace from Task 1.
- Produces: `int32 CombatFormulas::CrushingChanceBasisPoints(int32 attackerMaxSkill, int32 victimMaxSkill)`

- [ ] **Step 1: Write the failing test** — append to `tests/test_combat_formulas.cpp`:

```cpp
TEST_CASE("combat: crushing chance (basis points)")
{
    CHECK(CrushingChanceBasisPoints(120, 100) == 2500);  // diff 20 -> 25%
    CHECK(CrushingChanceBasisPoints(125, 100) == 3500);  // diff 25 -> 35%
    CHECK(CrushingChanceBasisPoints(105, 100) == 2500);  // diff 5 floored to 20 -> 25%
    CHECK(CrushingChanceBasisPoints(100, 100) == 2500);  // diff 0 floored to 20 -> 25%
}
```

- [ ] **Step 2: Run to verify it fails** — `cmake --build build --target mangos_tests --config Release`. Expected: FAIL — `CrushingChanceBasisPoints` is not a member of `CombatFormulas`.

- [ ] **Step 3: Add the function** — in `src/game/Object/CombatFormulas.h`, inside `namespace CombatFormulas`, after `GlancingChanceBasisPoints`:

```cpp
    /// Crushing chance in basis points: max(attackerMaxSkill - victimMaxSkill, 20)
    /// * 2% - 15%. May be <= 0; the caller gates eligibility and a > 0 check.
    inline int32 CrushingChanceBasisPoints(int32 attackerMaxSkill, int32 victimMaxSkill)
    {
        int32 skillDiff = attackerMaxSkill - victimMaxSkill;
        if (skillDiff < 20)
            skillDiff = 20;
        return skillDiff * 200 - 1500;
    }
```

- [ ] **Step 4: Run to verify it passes** — `cmake --build build --target mangos_tests --config Release` then `build\bin\Release\mangos_tests.exe`. Expected: PASS.

- [ ] **Step 5: Wire production** — in `src/game/Object/UnitCombat.cpp`, replace this block (in `RollMeleeOutcomeAgainst`):

```cpp
        int32 crush_chance = attackerMaxSkillValueForLevel - victimMaxSkillValueForLevel;
        if (crush_chance < 20)
            crush_chance = 20;
        crush_chance = crush_chance * 200 - 1500;  // basis points; 10000 = 100%
```

with:

```cpp
        int32 crush_chance = CombatFormulas::CrushingChanceBasisPoints(attackerMaxSkillValueForLevel, victimMaxSkillValueForLevel);
```

- [ ] **Step 6: Build production** — `cmake --build build --target game --config Release`. Expected: compiles clean.

- [ ] **Step 7: Commit**

```bash
git add src/game/Object/CombatFormulas.h tests/test_combat_formulas.cpp src/game/Object/UnitCombat.cpp
git commit -m "[Tests] Extract crushing-chance formula to CombatFormulas.h + tests"
```

---

## Task 3: Melee miss skill reduction

**Files:**
- Modify: `src/game/Object/CombatFormulas.h` (add function)
- Modify: `tests/test_combat_formulas.cpp` (add test case)
- Modify: `src/game/Object/UnitCombat.cpp` (replace skill-diff if/else in `MeleeMissChanceCalc` ~lines 1028-1039)

**Interfaces:**
- Consumes: `CombatFormulas` namespace from Tasks 1-2.
- Produces: `float CombatFormulas::MeleeMissSkillReduction(int32 skillDiff, bool victimIsPlayer)`

> Note: refined from the spec's `SkillBasedMeleeMissChance` to a drop-in
> *reduction* term (the value subtracted from the 5% base). This keeps the
> production edit a one-line replacement of the existing if/else — the base 5%
> and dual-wield/hit/aura modifiers stay exactly in place (smaller blast radius
> than reordering the function).

- [ ] **Step 1: Write the failing test** — append to `tests/test_combat_formulas.cpp`:

```cpp
TEST_CASE("combat: melee miss skill reduction")
{
    // Base melee miss is 5%; this reduction is SUBTRACTED, so a negative
    // reduction RAISES the miss chance. Comments show the resulting miss%.
    CHECK(MeleeMissSkillReduction(0, false) == doctest::Approx(0.0f));    // miss 5.0%
    CHECK(MeleeMissSkillReduction(0, true)  == doctest::Approx(0.0f));    // miss 5.0%
    CHECK(MeleeMissSkillReduction(-15, false) == doctest::Approx(-3.0f)); // +3 NPC boss: miss 8.0%
    CHECK(MeleeMissSkillReduction(-15, true)  == doctest::Approx(-0.6f)); // vs player: miss 5.6%
    CHECK(MeleeMissSkillReduction(-5, false)  == doctest::Approx(-0.5f)); // NPC, -5 skill: miss 5.5%
}
```

- [ ] **Step 2: Run to verify it fails** — `cmake --build build --target mangos_tests --config Release`. Expected: FAIL — `MeleeMissSkillReduction` is not a member of `CombatFormulas`.

- [ ] **Step 3: Add the function** — in `src/game/Object/CombatFormulas.h`, inside `namespace CombatFormulas`, after `CrushingChanceBasisPoints`:

```cpp
    /// Skill-based reduction (percent) subtracted from the 5% base melee miss
    /// chance. PvP (victim is a player) uses a 0.04/skill slope; PvE uses 0.1,
    /// with a steeper 0.4 slope below the -10 skill-diff threshold. Dual-wield,
    /// hit-rating and aura modifiers stay in Unit::MeleeMissChanceCalc.
    inline float MeleeMissSkillReduction(int32 skillDiff, bool victimIsPlayer)
    {
        if (victimIsPlayer)
            return skillDiff * 0.04f;
        else if (skillDiff < -10)
            return (skillDiff + 10) * 0.4f - 1.0f;
        else
            return skillDiff * 0.1f;
    }
```

- [ ] **Step 4: Run to verify it passes** — `cmake --build build --target mangos_tests --config Release` then `build\bin\Release\mangos_tests.exe`. Expected: PASS.

- [ ] **Step 5: Wire production** — in `src/game/Object/UnitCombat.cpp` `MeleeMissChanceCalc`, replace this block:

```cpp
    if (pVictim->GetTypeId() == TYPEID_PLAYER)
    {
        missChance -= skillDiff * 0.04f;
    }
    else if (skillDiff < -10)
    {
        missChance -= (skillDiff + 10) * 0.4f - 1.0f;
    }
    else
    {
        missChance -=  skillDiff * 0.1f;
    }
```

with:

```cpp
    missChance -= CombatFormulas::MeleeMissSkillReduction(skillDiff, pVictim->GetTypeId() == TYPEID_PLAYER);
```

- [ ] **Step 6: Build production** — `cmake --build build --target game --config Release`. Expected: compiles clean.

- [ ] **Step 7: Run full suite + push**

```bash
cd build && ctest -C Release --output-on-failure && cd ..
git add src/game/Object/CombatFormulas.h tests/test_combat_formulas.cpp src/game/Object/UnitCombat.cpp
git commit -m "[Tests] Extract melee miss skill-reduction formula to CombatFormulas.h + tests"
GIT_TERMINAL_PROMPT=0 GCM_INTERACTIVE=never git push origin feat/test-framework
```

Expected: `ctest` reports all tests passed (sanity + linkage + 3 combat cases).

---

## Self-review

- **Spec coverage:** glancing (Task 1), crushing (Task 2), miss (Task 3) — all three spec formulas + their production wiring + tests. ✓ Damage multipliers / armor / CI deliberately out of scope per spec. ✓
- **Placeholders:** none — every function body, test body, and exact old→new production block is shown. (The license-header line is a copy instruction, not a placeholder.)
- **Type consistency:** `GlancingChanceBasisPoints`/`CrushingChanceBasisPoints` return `int32`; `MeleeMissSkillReduction` returns `float`; all in `namespace CombatFormulas`; names identical across plan, tests, and production wiring. ✓
- **Deviation note:** spec's `SkillBasedMeleeMissChance` (base+skill) refined to `MeleeMissSkillReduction` (reduction term) for a minimal one-line production edit; mathematically equivalent (5.0 base stays in `MeleeMissChanceCalc`).
