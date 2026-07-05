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
#include "SpellMgr.h"
#include "DBCStructure.h"

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

TEST_CASE("combat: ranged periodic magic crits use the spell multiplier (150%)")
{
    // 4.3.4: ranged-damage-class spells whose periodic damage is a magic
    // school (Serpent Sting = Nature, Black Arrow = Shadow) crit for +50%
    // like spells, not +100% like physical ranged attacks.
    // Unit::SpellCriticalDamageBonus selects ApplySpellCriticalDamage for
    // periodic ticks of magic-school ranged spells.
    CHECK(ApplySpellCriticalDamage(1000u) == 1500u);
    CHECK(ApplyCriticalDamage(1000u) == 2000u);
    // A Serpent Sting tick that crits must take the 1500 path, not 2000.
    CHECK(ApplySpellCriticalDamage(1000u) != ApplyCriticalDamage(1000u));
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

TEST_CASE("combat: attack power bonus to weapon damage per swing")
{
    // Weapon Damage = (Weapon DPS + AP/14) * Weapon Speed; the AP term is
    // attackPower / 14 * speedSeconds (14 AP = 1 DPS; client 15595
    // PaperDollFrame.lua ATTACK_POWER_MAGIC_NUMBER = 14).
    CHECK(MeleeAttackPowerDamageBonus(1400.0f, 3.6f) == doctest::Approx(360.0f));
    CHECK(MeleeAttackPowerDamageBonus(28.0f, 1.0f)   == doctest::Approx(2.0f)); // 28 AP = 2 DPS
    CHECK(MeleeAttackPowerDamageBonus(0.0f, 2.4f)    == doctest::Approx(0.0f));

    // m3 feeds speed in seconds derived from GetAttackTime() milliseconds
    // (GetAPMultiplier, non-normalized: GetAttackTime(attType) / 1000.0f):
    // a 2600 ms weapon at 966 AP gains 69 DPS * 2.6 s = +179.4 per swing.
    CHECK(MeleeAttackPowerDamageBonus(966.0f, 2600.0f / 1000.0f) == doctest::Approx(179.4f));

    // Normalized speed (ability AP scaling; speed selection stays in the
    // caller): dagger 1.7.
    CHECK(MeleeAttackPowerDamageBonus(1000.0f, 1.7f) == doctest::Approx(121.428571f));
}

TEST_CASE("combat: haste time factor / hastened time")
{
    // factor = 100 / (100 + hastePct); zero haste is a no-op.
    CHECK(HasteTimeFactor(0.0f) == doctest::Approx(1.0f));
    CHECK(HastenedTime(3600.0f, 0.0f) == doctest::Approx(3600.0f));

    CHECK(HasteTimeFactor(30.0f) == doctest::Approx(100.0f / 130.0f));

    // A 3.6 s weapon under 30% haste swings every ~2.769 s.
    CHECK(HastenedTime(3600.0f, 30.0f) == doctest::Approx(2769.2307f));

    // Equivalence to the canonical published form baseTime / (1 + h/100).
    CHECK(HastenedTime(2500.0f, 17.5f) == doctest::Approx(2500.0f / (1.0f + 17.5f / 100.0f)));

    // Negative haste is a slow: half speed doubles the time.
    CHECK(HastenedTime(2000.0f, -50.0f) == doctest::Approx(4000.0f));

    // Production consumption sites both reduce to this per-source form:
    // SpellMgr.cpp castTime * UNIT_MOD_CAST_SPEED, and Unit.cpp attack
    // timer GetAttackTime(type) * m_modAttackSpeedPct[type].
}

TEST_CASE("combat: haste sources stack multiplicatively (Cata)")
{
    // Two independent 10% sources compose as factor*factor, not as a
    // single 20% source -- 2975.2 ms vs 3000 ms.
    CHECK(HastenedTime(HastenedTime(3600.0f, 10.0f), 10.0f)
          == doctest::Approx(3600.0f * 100.0f / 110.0f * 100.0f / 110.0f));
    CHECK(HastenedTime(HastenedTime(3600.0f, 10.0f), 10.0f)
          != doctest::Approx(HastenedTime(3600.0f, 20.0f)));

    // Realistic stack: a 30% Bloodlust-style buff on top of 12.5% from
    // rating composes as the product of the two per-source factors.
    float composed = HasteTimeFactor(30.0f) * HasteTimeFactor(12.5f);
    CHECK(HastenedTime(HastenedTime(3600.0f, 30.0f), 12.5f) == doctest::Approx(3600.0f * composed));
}

TEST_CASE("combat: haste RATING stacks additively with itself (Cata)")
{
    // Player::ApplyRatingMod re-bases the whole haste-rating pool on every
    // change (remove old total percent, apply new total percent), mirroring
    // TC 4.3.4, so rating composes additively before the single conversion.
    // Emulate two item equips each worth +10% haste from rating: the pool
    // goes 0% -> 10% -> 20% and the timer ends at base / 1.20, NOT the
    // per-delta multiplicative base / 1.21.
    float t = 3600.0f;
    t = t / HasteTimeFactor(0.0f) * HasteTimeFactor(10.0f);   // equip 1: 0 -> 10
    t = t / HasteTimeFactor(10.0f) * HasteTimeFactor(20.0f);  // equip 2: 10 -> 20
    CHECK(t == doctest::Approx(3600.0f / 1.20f));
    CHECK(t != doctest::Approx(3600.0f / 1.21f));

    // Single source: re-base from an empty pool is just the plain factor.
    CHECK(3600.0f / HasteTimeFactor(0.0f) * HasteTimeFactor(10.0f)
          == doctest::Approx(HastenedTime(3600.0f, 10.0f)));

    // Zero rating: re-base is a no-op.
    CHECK(3600.0f / HasteTimeFactor(0.0f) * HasteTimeFactor(0.0f)
          == doctest::Approx(3600.0f));

    // Unequip symmetry: removing one item re-bases 20 -> 10.
    t = t / HasteTimeFactor(20.0f) * HasteTimeFactor(10.0f);
    CHECK(t == doctest::Approx(HastenedTime(3600.0f, 10.0f)));
}

TEST_CASE("combat: creature AP-delta damage bonus (operand-order normalization)")
{
    // Creature::UpdateDamagePhysical converts only the AP DELTA vs the DB
    // MeleeAttackPower base to bonus damage (the DB damage range already
    // includes the template's base AP); rewired to the shared helper --
    // the legacy (delta * speed) / 14 order agrees with (delta / 14) * speed
    // within float rounding.
    CHECK(MeleeAttackPowerDamageBonus(280.0f, 2.0f) == doctest::Approx((280.0f * 2.0f) / 14.0f));   // exact: 40 bonus damage per swing
    CHECK(MeleeAttackPowerDamageBonus(-140.0f, 2.0f) == doctest::Approx(-20.0f));   // negative delta (AP debuffed below DB base) keeps its sign
    CHECK(MeleeAttackPowerDamageBonus(966.3f, 2.6f) == doctest::Approx((966.3f * 2.6f) / 14.0f));   // non-exact operands: both orders agree within epsilon
}

TEST_CASE("combat: victim crit-damage-taken mod on a ranged periodic magic crit (characterization)")
{
    // FLAG (characterized, not changed): Unit::SpellCriticalDamageBonus keys
    // the victim-side crit-damage-taken aura on DmgClass -- a ranged periodic
    // MAGIC crit consults SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_DAMAGE (204),
    // not the spell variant (205), while the attacker-side multiplier treats
    // the same tick as a spell (+50%). No authoritative reference exists: TC
    // 4.3.4 has no victim-side block in this function and leaves aura 205
    // unimplemented. Full 15595 SpellEffect.dbc scan: aura 204 appears only
    // on internal test spell 61860 (Arena 1 Resil), which carries 203+204+205
    // all at -24% / school mask 127 -- either keying yields the same value --
    // and aura 205 otherwise only on unused TBC debuffs 38714-38717. The
    // selection is therefore value-neutral in authentic 4.3.4 data.

    // Pipeline shape pinned via the pure kernels: a 1000 ranged periodic
    // magic tick crits to 1500 (+50%), then the victim mod scales only the
    // 500 bonus: production computes int32(bonus * (100 + mod) / 100).
    const uint32 tick = 1000;
    const uint32 crit = ApplySpellCriticalDamage(tick);
    CHECK(crit == 1500);
    const int32 bonus = int32(crit - tick);
    // 61860's -24% taken-mod: identical result whether keyed ranged or spell.
    CHECK(int32(bonus * (100.0f + -24) / 100.0f) == 380);   // 1000 + 380 = 1380 either way
}

TEST_CASE("combat: spell power damage bonus (pure multiply kernel)")
{
    // Exact float multiply from Unit::SpellBonusWithCoeffs; level penalty
    // and int32 truncation stay with the caller.
    CHECK(SpellPowerDamageBonus(2000.0f, 0.5f) == doctest::Approx(1000.0f));
    CHECK(SpellPowerDamageBonus(0.0f, 1.0f) == doctest::Approx(0.0f));
    CHECK(SpellPowerDamageBonus(2000.0f, 0.0f) == doctest::Approx(0.0f));

    // Grounded in the real 15595 SpellEffect.dbc per-effect coefficients
    // (m_effectBonus): Fireball 1.236, Shadow Bolt 0.754, Smite 0.856.
    CHECK(SpellPowerDamageBonus(2000.0f, 1.236f) == doctest::Approx(2472.0f));
    CHECK(SpellPowerDamageBonus(2000.0f, 0.754f) == doctest::Approx(1508.0f));
    CHECK(SpellPowerDamageBonus(2000.0f, 0.856f) == doctest::Approx(1712.0f));

    // DoT per-tick: the coefficient fed in is per-tick (Corruption 0.176
    // per tick in the 15595 DBC; m3's computed fallback divides the full
    // coefficient by the tick count to get the same shape).
    CHECK(SpellPowerDamageBonus(2000.0f, 0.176f) == doctest::Approx(352.0f));
}

TEST_CASE("combat: spell power bonus coefficient derivation (characterization)")
{
    // FIXED: SpellBonusWithCoeffs now derives the default coefficient from
    // the 15595 SpellEffect.dbc per-effect coefficient (m_effectBonus) times
    // the SpellScaling.dbc multiplier (TC 4.3.4 parity); spell_bonus_data DB
    // rows still override, and CalculateDefaultCoefficient (the retired
    // WotLK-era cast-time/3500 fallback) survives only for debug/DB-check
    // tooling. This case now pins the retired fallback's pure shape for
    // historical reference: what an instant (GCD-floored 1500ms) cast used
    // to produce under the old rule.
    float instantCoeff = 1500.0f / 3500.0f;
    CHECK(SpellPowerDamageBonus(1000.0f, instantCoeff) == doctest::Approx(428.5714f).epsilon(0.0001));

    // Production form truncates: int32(benefit * coeff * LvlPenalty).
    CHECK(int32(SpellPowerDamageBonus(1000.0f, instantCoeff) * 1.0f) == 428);
}

TEST_CASE("combat: legacy vs 4.3.4 DBC coefficients for representative spells (characterization)")
{
    // The coefficient-source switch (spell_bonus_data / CalculateDefaultCoefficient
    // -> SpellEffect.dbc m_effectBonus) has landed in SpellBonusWithCoeffs.
    // This case now documents two things side by side for a representative
    // spell set at level 85: (1) what a spell_bonus_data DB override row
    // still produces when one is shipped (database/World/Setup/FullDB/
    // spell_bonus_data.sql, 132 rows, WotLK-era values; direct: castTime/3500,
    // DoT: DotFactor = duration/15000/maxTicks via the retired
    // CalculateDefaultCoefficient fallback, both formerly followed by
    // CalculateLevelPenalty == 1.0 at 85 for these spells), and (2) the real
    // 15595 SpellEffect.dbc per-effect coefficients that are now the default
    // when no override row exists.
    const float sp = 2000.0f;

    // --- legacy path: what a spell_bonus_data DB override row (or the
    // retired fallback) used to feed the multiply kernel ---

    // Fireball (133): DB row direct_bonus = 1.0.
    CHECK(SpellPowerDamageBonus(sp, 1.0f) == doctest::Approx(2000.0f));

    // Smite (585): DB row direct_bonus = 0.714.
    CHECK(SpellPowerDamageBonus(sp, 0.714f) == doctest::Approx(1428.0f));

    // Corruption (172): DB row dot_bonus = 0.2 per tick. Same shape as the
    // computed fallback: duration 18000ms / 15000 / 6 ticks = 0.2.
    CHECK(SpellPowerDamageBonus(sp, 0.2f) == doctest::Approx(400.0f));

    // Immolate (348): DB row direct 0.2 / dot 0.2 per tick.
    CHECK(SpellPowerDamageBonus(sp, 0.2f) == doctest::Approx(400.0f));  // direct
    CHECK(SpellPowerDamageBonus(sp, 0.2f) == doctest::Approx(400.0f));  // per tick

    // Shadow Bolt (686): NO row in the shipped spell_bonus_data dump ->
    // computed fallback castTimeForBonus/3500 with its 3.0s base cast.
    CHECK(SpellPowerDamageBonus(sp, 3000.0f / 3500.0f) == doctest::Approx(1714.2857f));

    // Renew (139): no row in the shipped dump either; fallback path with the
    // 1.88 SCALE_SPELLPOWER_HEALING factor applies. Renew is an instant
    // (1500ms) HoT, duration 12000ms, 4 ticks -> DotFactor = 12000/15000/4 =
    // 0.2, so per-tick coeff = 1500/3500 * 0.2 * 1.88.
    const float renewCoeff = (1500.0f / 3500.0f) * 0.2f * 1.88f;
    CHECK(SpellPowerDamageBonus(sp, renewCoeff)
          == doctest::Approx(2000.0f * (1500.0f / 3500.0f) * 0.2f * 1.88f));

    // --- 4.3.4 DBC path: what the 15595 SpellEffect.dbc actually ships ---
    // (server_install/dbc/SpellEffect.dbc, WDBC, field 6 = m_effectBonus,
    // 13,937 nonzero records in our extract). This is the target of the
    // upcoming coefficient-source switch.

    // Fireball eff0 SCHOOL_DAMAGE 1.236.
    CHECK(SpellPowerDamageBonus(sp, 1.236f) == doctest::Approx(2472.0f));

    // Smite eff0 SCHOOL_DAMAGE 0.856.
    CHECK(SpellPowerDamageBonus(sp, 0.856f) == doctest::Approx(1712.0f));

    // Shadow Bolt eff0 SCHOOL_DAMAGE 0.754.
    CHECK(SpellPowerDamageBonus(sp, 0.754f) == doctest::Approx(1508.0f));

    // Corruption eff0 APPLY_AURA/PERIODIC_DAMAGE 0.176 per tick.
    CHECK(SpellPowerDamageBonus(sp, 0.176f) == doctest::Approx(352.0f));

    // Immolate eff1 SCHOOL_DAMAGE 0.220 direct AND eff2 APPLY_AURA/
    // PERIODIC_DAMAGE 0.176 per tick.
    CHECK(SpellPowerDamageBonus(sp, 0.220f) == doctest::Approx(440.0f));  // direct
    CHECK(SpellPowerDamageBonus(sp, 0.176f) == doctest::Approx(352.0f)); // per tick

    // Renew eff0 APPLY_AURA/PERIODIC_HEAL 0.131 per tick.
    CHECK(SpellPowerDamageBonus(sp, 0.131f) == doctest::Approx(262.0f));

    // Flash Heal (2061) eff0 HEAL 0.725.
    CHECK(SpellPowerDamageBonus(sp, 0.725f) == doctest::Approx(1450.0f));

    // SpellScaling.dbc rows for all of these evaluate to multiplier 1.0 at
    // level 85 (Fireball row 18, Smite 211, Shadow Bolt 306, Corruption 271,
    // Immolate 291, Renew 207, Flash Heal 189; parsed from our
    // server_install/dbc/SpellScaling.dbc), so it is a no-op here -- but
    // below level 80 the rows attenuate (e.g. Fireball coefBase 0.88 to
    // level 80), replacing the vanilla CalculateLevelPenalty entirely.

    // --- divergence: this is the delta a spell_bonus_data DB override row
    // still makes versus the DBC-driven default ---
    CHECK(SpellPowerDamageBonus(sp, 1.0f) != doctest::Approx(SpellPowerDamageBonus(sp, 1.236f)));   // Fireball: DB row shadows the DBC value
    CHECK(SpellPowerDamageBonus(sp, 0.714f) != doctest::Approx(SpellPowerDamageBonus(sp, 0.856f)));  // Smite
    CHECK(SpellPowerDamageBonus(sp, 3000.0f / 3500.0f) != doctest::Approx(SpellPowerDamageBonus(sp, 0.754f)));  // Shadow Bolt: fallback shape vs DBC
    CHECK(SpellPowerDamageBonus(sp, 0.2f) != doctest::Approx(SpellPowerDamageBonus(sp, 0.176f)));    // Corruption per-tick
    CHECK(SpellPowerDamageBonus(sp, 0.2f) != doctest::Approx(SpellPowerDamageBonus(sp, 0.220f)));    // Immolate direct
    CHECK(SpellPowerDamageBonus(sp, renewCoeff) != doctest::Approx(SpellPowerDamageBonus(sp, 0.131f)));  // Renew: 1.88 fallback vs DBC heal coeff
}

TEST_CASE("combat: spell scaling multiplier mirrors TC 4.3.4 (15595 SpellScaling rows)")
{
    // No scaling row (e.g. spells outside the scaling system) is a no-op.
    CHECK(CalculateSpellScalingMultiplier(NULL, 85) == doctest::Approx(1.0f));

    // Fireball (row 18): parsed from server_install/dbc/SpellScaling.dbc.
    SpellScalingEntry row18 = {};
    row18.castTimeMin = 1500;
    row18.castTimeMax = 2500;
    row18.castScalingMaxLevel = 20;
    row18.playerClass = 8;
    row18.coefBase = 0.88f;
    row18.coefLevelBase = 80;

    CHECK(CalculateSpellScalingMultiplier(&row18, 85) == doctest::Approx(1.0f));
    CHECK(CalculateSpellScalingMultiplier(&row18, 80) == doctest::Approx(1.0f));
    CHECK(CalculateSpellScalingMultiplier(&row18, 10) == doctest::Approx(0.705285f));
    CHECK(CalculateSpellScalingMultiplier(&row18, 5) == doctest::Approx(0.606076f));

    // Immolate (row 291).
    SpellScalingEntry row291 = {};
    row291.castTimeMin = 2000;
    row291.castTimeMax = 2000;
    row291.castScalingMaxLevel = 20;
    row291.playerClass = 9;
    row291.coefBase = 0.55f;
    row291.coefLevelBase = 80;

    CHECK(CalculateSpellScalingMultiplier(&row291, 85) == doctest::Approx(1.0f));
    CHECK(CalculateSpellScalingMultiplier(&row291, 40) == doctest::Approx(0.772152f));

    // Renew (row 207): flat cast time, coefLevelBase 0 -> never nerfed.
    SpellScalingEntry row207 = {};
    row207.castTimeMin = 0;
    row207.castTimeMax = 0;
    row207.castScalingMaxLevel = 20;
    row207.playerClass = 5;
    row207.coefBase = 1.0f;
    row207.coefLevelBase = 0;

    CHECK(CalculateSpellScalingMultiplier(&row207, 85) == doctest::Approx(1.0f));
    CHECK(CalculateSpellScalingMultiplier(&row207, 1) == doctest::Approx(1.0f));

    // At max level the multiplier is 1.0, so the DBC coefficient applies
    // unattenuated; below a row's nerf level it replaces the vanilla
    // CalculateLevelPenalty.
}

TEST_CASE("combat: DBC bonus-coefficient effect selection (15595 effect shapes)")
{
    // Fireball (133): single direct-damage effect.
    SpellEffectEntry fireballE0 = {};
    fireballE0.Effect = SPELL_EFFECT_SCHOOL_DAMAGE;
    fireballE0.EffectBonusMultiplier = 1.236f;
    SpellEffectEntry const* fireballEffects[MAX_EFFECT_INDEX] = { &fireballE0, NULL, NULL };

    CHECK(SelectSpellBonusCoefficient(fireballEffects, SPELL_DIRECT_DAMAGE, false) == doctest::Approx(1.236f));

    // Corruption (172): periodic damage aura plus an inert dummy effect.
    SpellEffectEntry corruptionE0 = {};
    corruptionE0.Effect = SPELL_EFFECT_APPLY_AURA;
    corruptionE0.EffectApplyAuraName = SPELL_AURA_PERIODIC_DAMAGE;
    corruptionE0.EffectBonusMultiplier = 0.176f;
    SpellEffectEntry corruptionE1 = {};
    corruptionE1.Effect = SPELL_EFFECT_DUMMY;
    corruptionE1.EffectBonusMultiplier = 0.0f;
    SpellEffectEntry const* corruptionEffects[MAX_EFFECT_INDEX] = { &corruptionE0, &corruptionE1, NULL };

    CHECK(SelectSpellBonusCoefficient(corruptionEffects, DOT, false) == doctest::Approx(0.176f));

    // Immolate (348): hybrid direct + DoT; a scripted dummy effect precedes
    // both. The same effect array resolves differently per damage type.
    SpellEffectEntry immolateE0 = {};
    immolateE0.Effect = SPELL_EFFECT_SCRIPT_EFFECT;
    immolateE0.EffectBonusMultiplier = 0.0f;
    SpellEffectEntry immolateE1 = {};
    immolateE1.Effect = SPELL_EFFECT_SCHOOL_DAMAGE;
    immolateE1.EffectBonusMultiplier = 0.220f;
    SpellEffectEntry immolateE2 = {};
    immolateE2.Effect = SPELL_EFFECT_APPLY_AURA;
    immolateE2.EffectApplyAuraName = SPELL_AURA_PERIODIC_DAMAGE;
    immolateE2.EffectBonusMultiplier = 0.176f;
    SpellEffectEntry const* immolateEffects[MAX_EFFECT_INDEX] = { &immolateE0, &immolateE1, &immolateE2 };

    CHECK(SelectSpellBonusCoefficient(immolateEffects, SPELL_DIRECT_DAMAGE, false) == doctest::Approx(0.220f));
    CHECK(SelectSpellBonusCoefficient(immolateEffects, DOT, false) == doctest::Approx(0.176f));

    // Renew (139): periodic heal aura.
    SpellEffectEntry renewE0 = {};
    renewE0.Effect = SPELL_EFFECT_APPLY_AURA;
    renewE0.EffectApplyAuraName = SPELL_AURA_PERIODIC_HEAL;
    renewE0.EffectBonusMultiplier = 0.131f;
    SpellEffectEntry const* renewEffects[MAX_EFFECT_INDEX] = { &renewE0, NULL, NULL };

    CHECK(SelectSpellBonusCoefficient(renewEffects, DOT, true) == doctest::Approx(0.131f));

    // Flash Heal (2061): direct heal effect.
    SpellEffectEntry flashHealE0 = {};
    flashHealE0.Effect = SPELL_EFFECT_HEAL;
    flashHealE0.EffectBonusMultiplier = 0.725f;
    SpellEffectEntry const* flashHealEffects[MAX_EFFECT_INDEX] = { &flashHealE0, NULL, NULL };

    CHECK(SelectSpellBonusCoefficient(flashHealEffects, SPELL_DIRECT_DAMAGE, true) == doctest::Approx(0.725f));

    // No matching effect/aura and no nonzero fallback -> 0.0f.
    SpellEffectEntry zeroEffect = {};
    SpellEffectEntry const* zeroEffects[MAX_EFFECT_INDEX] = { &zeroEffect, NULL, NULL };

    CHECK(SelectSpellBonusCoefficient(zeroEffects, SPELL_DIRECT_DAMAGE, false) == doctest::Approx(0.0f));

    // All-NULL effect array -> 0.0f.
    SpellEffectEntry const* nullEffects[MAX_EFFECT_INDEX] = { NULL, NULL, NULL };

    CHECK(SelectSpellBonusCoefficient(nullEffects, SPELL_DIRECT_DAMAGE, false) == doctest::Approx(0.0f));

    // End-to-end proof: DBC effect coefficient x spell scaling multiplier
    // feeding the multiply kernel, 2000 spell power at level 85.
    SpellScalingEntry row18 = {};
    row18.castTimeMin = 1500;
    row18.castTimeMax = 2500;
    row18.castScalingMaxLevel = 20;
    row18.coefBase = 0.88f;
    row18.coefLevelBase = 80;

    CHECK(SpellPowerDamageBonus(2000.0f, SelectSpellBonusCoefficient(fireballEffects, SPELL_DIRECT_DAMAGE, false) * CalculateSpellScalingMultiplier(&row18, 85))
          == doctest::Approx(2472.0f));
    CHECK(SpellPowerDamageBonus(2000.0f, SelectSpellBonusCoefficient(corruptionEffects, DOT, false) * 1.0f)
          == doctest::Approx(352.0f));
}
