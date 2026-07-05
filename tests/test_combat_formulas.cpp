/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

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

TEST_CASE("combat: crushing chance (basis points)")
{
    CHECK(CrushingChanceBasisPoints(120, 100) == 2500);  // diff 20 -> 25%
    CHECK(CrushingChanceBasisPoints(125, 100) == 3500);  // diff 25 -> 35%
    CHECK(CrushingChanceBasisPoints(105, 100) == 2500);  // diff 5 floored to 20 -> 25%
    CHECK(CrushingChanceBasisPoints(100, 100) == 2500);  // diff 0 floored to 20 -> 25%
}

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

TEST_CASE("combat: melee miss vs level matches the 4.3.4 client table")
{
    // Ground the PvE melee-miss result against the 15595 client
    // (PaperDollFrame.lua BASE_MISS_CHANCE_PHYSICAL = {5.0, 5.5, 6.0, 8.0}):
    // a player attacking a target `offset` levels higher misses at those
    // percentages for +0/+1/+2/+3. The +3 (raid boss) 8% is why the Cata melee
    // "hit cap" for special attacks is 8% (web-confirmed). Weapon skill is
    // deprecated in 4.0.1, so max skill is level*5 and the skill diff fed to the
    // reduction is -5 per level the target is above. Algebraically identical to
    // TrinityCore 4.3.4 Unit::GetMeleeMissChance.
    auto pveMiss = [](int levelOffset)
    {
        return 5.0f - MeleeMissSkillReduction(-5 * levelOffset, false);
    };
    CHECK(pveMiss(0) == doctest::Approx(5.0f));   // same level
    CHECK(pveMiss(1) == doctest::Approx(5.5f));   // +1
    CHECK(pveMiss(2) == doctest::Approx(6.0f));   // +2
    CHECK(pveMiss(3) == doctest::Approx(8.0f));   // +3 raid boss (8% hit cap)

    // White-hit dual wield adds a flat +19% miss (client DUAL_WIELD_HIT_PENALTY,
    // Blizzard-confirmed; auto-attacks only). Unit::MeleeMissChanceCalc adds this
    // on top of the level-based base, so a dual-wielder vs a +3 boss sees 27%.
    CHECK(pveMiss(3) + 19.0f == doctest::Approx(27.0f));
}

TEST_CASE("combat: spell miss vs level matches the 4.3.4 client table")
{
    // Ground the PvE spell-miss result against the 15595 client
    // (PaperDollFrame.lua BASE_MISS_CHANCE_SPELL = {4.0, 5.0, 6.0, 17.0}):
    // miss = 100 - SpellHitChanceBase for +0/+1/+2/+3. The +3 (raid boss)
    // 17% is the well-known Cata spell "hit cap" (period-confirmed,
    // 2010-2011 sources). Ranged uses BASE_MISS_CHANCE_PHYSICAL and is
    // covered by the melee miss-vs-level case above. Algebraically
    // identical to TrinityCore 4.3.4 for PvE.
    auto pveMiss = [](int levelOffset)
    {
        return 100 - SpellHitChanceBase(levelOffset, false);
    };
    CHECK(pveMiss(0) == 4);    // same level
    CHECK(pveMiss(1) == 5);    // +1
    CHECK(pveMiss(2) == 6);    // +2
    CHECK(pveMiss(3) == 17);   // +3 raid boss (17% spell hit cap)
    CHECK(pveMiss(4) == 28);   // beyond boss: +11%/level vs creatures

    // vs players the slope beyond +2 is 7%/level: +3 player = 13% miss.
    // Characterizes current behaviour; TC 4.3.4 instead uses a separate
    // PvP base (94 - 3*offset -> 6/9/12/15) -- divergence flagged for
    // review, not fixed here.
    CHECK(100 - SpellHitChanceBase(0, true) == 4);
    CHECK(100 - SpellHitChanceBase(3, true) == 13);

    // Lower-level targets: miss shrinks 1%/level below the caster
    // (characterizes current behaviour; the client paperdoll table only
    // shows offsets 0..3).
    CHECK(100 - SpellHitChanceBase(-1, false) == 3);
    CHECK(100 - SpellHitChanceBase(-2, false) == 2);
}

TEST_CASE("combat: glancing damage multiplier")
{
    // Cata 4.0.1 flat model: 10% reduction per level the victim is above the
    // attacker, capped at 3 levels. +3 raid boss = 0.70 (-30%) is web-verified;
    // intermediate per-level steps pin the current 4.0.1 implementation.
    CHECK(GlancingDamageMultiplier(0) == doctest::Approx(1.0f));
    CHECK(GlancingDamageMultiplier(1) == doctest::Approx(0.9f));
    CHECK(GlancingDamageMultiplier(2) == doctest::Approx(0.8f));
    CHECK(GlancingDamageMultiplier(3) == doctest::Approx(0.7f));  // +3 raid boss: 70%
    CHECK(GlancingDamageMultiplier(6) == doctest::Approx(0.7f));  // capped at 3 levels
}

TEST_CASE("combat: crushing blow damage (150%)")
{
    CHECK(ApplyCrushingDamage(100) == 150u);
    CHECK(ApplyCrushingDamage(101) == 151u);  // integer: 101 + 101/2
    CHECK(ApplyCrushingDamage(3)   == 4u);    // 3 + 3/2
    CHECK(ApplyCrushingDamage(0)   == 0u);
}

TEST_CASE("combat: critical hit damage (200%)")
{
    // Physical melee/ranged crit doubles damage in 4.3.4 (flat +100% bonus,
    // unchanged since vanilla; matches TrinityCore's `Damage *= 2`). The MoP
    // 5.0.4 spell-crit change to +100% does not apply. Aura crit-damage
    // modifiers layer on top in Unit:: and are out of scope here.
    CHECK(ApplyCriticalDamage(100) == 200u);
    CHECK(ApplyCriticalDamage(101) == 202u);
    CHECK(ApplyCriticalDamage(1)   == 2u);
    CHECK(ApplyCriticalDamage(0)   == 0u);
}

TEST_CASE("combat: spell critical damage (150%)")
{
    // Magic spell crits deal 150% in 4.3.4 (+50% bonus), integer form
    // base + base / 2 -- matches TrinityCore 4.3.4's "Magic spells will
    // simply deal 50% additional crit damage" (crit_bonus += damage / 2)
    // and the pre-MoP wikis ("spell critical strikes deal 150% normal
    // damage without talents"). The change to +100% was patch 5.0.4/MoP,
    // after 4.3.4. Melee/ranged damage-class spells instead use the
    // physical x2 (ApplyCriticalDamage). Aura/spell-mod layers stay in
    // Unit::SpellCriticalDamageBonus.
    CHECK(ApplySpellCriticalDamage(100) == 150u);
    CHECK(ApplySpellCriticalDamage(101) == 151u);  // integer: 101 + 101/2
    CHECK(ApplySpellCriticalDamage(3)   == 4u);    // 3 + 3/2
    CHECK(ApplySpellCriticalDamage(0)   == 0u);
}

TEST_CASE("combat: spell critical healing (200%)")
{
    // Healing crits double in 4.3.4: raised from 150% to 200% in patch
    // 4.2.0 (2011-06-28) -- Wowpedia patch note quoted verbatim in
    // TrinityCore 4.3.4's Unit::SpellCriticalHealingBonus (damage *= 2).
    // So in 4.3.4 a heal crit is x2 while a spell-damage crit is x1.5.
    // MOD_CRITICAL_HEALING_AMOUNT auras layer on top in Unit::.
    CHECK(ApplySpellCriticalHealing(100) == 200u);
    CHECK(ApplySpellCriticalHealing(101) == 202u);
    CHECK(ApplySpellCriticalHealing(1)   == 2u);
    CHECK(ApplySpellCriticalHealing(0)   == 0u);
}

TEST_CASE("combat: crit chance percent from stat (formula)")
{
    // crit% = (base + statValue * ratioPerStat) * 100
    CHECK(CritChancePercentFromStat(0.0f,  0.0f,    0.0f)    == doctest::Approx(0.0f));
    CHECK(CritChancePercentFromStat(0.05f, 0.0f,    0.0001f) == doctest::Approx(5.0f));   // base only
    CHECK(CritChancePercentFromStat(0.05f, 1000.0f, 0.0001f) == doctest::Approx(15.0f));  // 0.05 + 0.10
}

TEST_CASE("combat: crit-from-stat grounded in our 15595 GameTables")
{
    // Authoritative coefficients read from our own client extract
    // (server_install/dbc). Layout: base = gt*CritBase row (class-1);
    // ratio = gt*Crit row ((class-1)*GT_MAX_LEVEL + level-1), value column = "xf".

    // Mage (class 8) spell crit @ level 85:
    //   gtChanceToSpellCritBase.dbc row7  = 0.00907500
    //   gtChanceToSpellCrit.dbc     row784 = 1.54105e-05  (per intellect)
    const float mageBase  = 0.009075f;
    const float mageRatio = 1.54105e-05f;
    CHECK(CritChancePercentFromStat(mageBase, 0.0f,    mageRatio) == doctest::Approx(0.9075f));
    CHECK(CritChancePercentFromStat(mageBase, 1000.0f, mageRatio) == doctest::Approx(2.44855f));

    // Rogue (class 4) melee crit @ level 85 (note negative base):
    //   gtChanceToMeleeCritBase.dbc row3   = -0.00295000
    //   gtChanceToMeleeCrit.dbc     row384 = 3.07957e-05  (per agility)
    const float rogueBase  = -0.00295f;
    const float rogueRatio = 3.07957e-05f;
    CHECK(CritChancePercentFromStat(rogueBase, 0.0f,    rogueRatio) == doctest::Approx(-0.295f));
    CHECK(CritChancePercentFromStat(rogueBase, 1000.0f, rogueRatio) == doctest::Approx(2.78457f));
}

TEST_CASE("combat: combat-rating to percent multiplier (formula)")
{
    // multiplier (% per 1 rating point) = classScalar / ratingPerPercent
    CHECK(CombatRatingMultiplier(1.0f, 100.0f) == doctest::Approx(0.01f));
    CHECK(CombatRatingMultiplier(0.5f, 100.0f) == doctest::Approx(0.005f));
}

TEST_CASE("combat: rating multiplier grounded in our 15595 GameTables")
{
    // gtCombatRatings.dbc row 884 = CR_CRIT_MELEE(8)*GT_MAX_LEVEL(100) + (85-1):
    //   ratingPerPercent = 179.28004  (our 15595 extract; the canonical "179.28
    //   crit rating = 1% crit at level 85").
    // gtOCTClassCombatRatingScalar.dbc: class scalar for crit = 1.0 (e.g. Warrior
    //   row (0*32 + 8 + 1)=9, Mage row 233).
    const float critRatingPerPct = 179.28004f;
    const float critScalar       = 1.0f;

    CHECK(CombatRatingMultiplier(critScalar, critRatingPerPct) == doctest::Approx(0.0055779f));
    // 179.28 crit rating yields exactly 1% crit at level 85:
    CHECK(critRatingPerPct * CombatRatingMultiplier(critScalar, critRatingPerPct) == doctest::Approx(1.0f));
}

TEST_CASE("combat: armor damage reduction fraction")
{
    // reduction = armor / (armor + K), K = 85*levelModifier + 400, capped 0.75.
    // armor == K therefore yields exactly 50%. All K values below are the 4.3.4
    // client's (15595 PaperDollFrame_GetArmorReduction), three-tier level modifier.
    CHECK(ArmorDamageReductionFraction(0.0f, 85)       == doctest::Approx(0.0f));

    // <60 and 60..80 tiers (K = 467.5*level - 22167.5 in the 60..80 range):
    CHECK(ArmorDamageReductionFraction(2950.0f, 30)    == doctest::Approx(0.5f));   // <60: K=85*30+400
    CHECK(ArmorDamageReductionFraction(10557.5f, 70)   == doctest::Approx(0.5f));   // K(70)
    CHECK(ArmorDamageReductionFraction(15232.5f, 80)   == doctest::Approx(0.5f));   // K(80) - last 2-tier level

    // > 80 tier adds +20*(level-80). Boundary (81) plus boss/cap levels:
    CHECK(ArmorDamageReductionFraction(17400.0f, 81)   == doctest::Approx(0.5f));   // K(81)=17400 (2-tier would be 15700)
    CHECK(ArmorDamageReductionFraction(21735.0f, 83)   == doctest::Approx(0.5f));   // K(83)=21735
    CHECK(ArmorDamageReductionFraction(26070.0f, 85)   == doctest::Approx(0.5f));   // K(85)=26070

    // 75% cap (armor == 3*K -> exactly 0.75, and beyond stays clamped):
    CHECK(ArmorDamageReductionFraction(3.0f*26070.0f, 85) == doctest::Approx(0.75f));
    CHECK(ArmorDamageReductionFraction(1.0e9f, 85)     == doctest::Approx(0.75f));
}

TEST_CASE("combat: enemy dodge/parry level bonus")
{
    // 4.3.4 client (PaperDollFrame.lua): with a 5.0% creature base, total enemy
    // avoidance by levelOffset (victimLevel - attackerLevel, 0..3, 3 = boss):
    //   dodge: 5.0 / 5.5 / 6.0 / 6.5   parry: 5.0 / 5.5 / 6.0 / 14.0
    const float base = 5.0f;
    CHECK(base + EnemyDodgeLevelBonus(0) == doctest::Approx(5.0f));
    CHECK(base + EnemyDodgeLevelBonus(1) == doctest::Approx(5.5f));
    CHECK(base + EnemyDodgeLevelBonus(2) == doctest::Approx(6.0f));
    CHECK(base + EnemyDodgeLevelBonus(3) == doctest::Approx(6.5f));
    CHECK(base + EnemyDodgeLevelBonus(5) == doctest::Approx(6.5f));   // capped at +3

    CHECK(base + EnemyParryLevelBonus(0) == doctest::Approx(5.0f));
    CHECK(base + EnemyParryLevelBonus(1) == doctest::Approx(5.5f));
    CHECK(base + EnemyParryLevelBonus(2) == doctest::Approx(6.0f));
    CHECK(base + EnemyParryLevelBonus(3) == doctest::Approx(14.0f));  // boss jump
    CHECK(base + EnemyParryLevelBonus(7) == doctest::Approx(14.0f));  // capped at +3

    // Negative offsets (attacking lower targets) reduce avoidance by 0.5%/level.
    CHECK(EnemyDodgeLevelBonus(-2) == doctest::Approx(-1.0f));
    CHECK(EnemyParryLevelBonus(-2) == doctest::Approx(-1.0f));
}
