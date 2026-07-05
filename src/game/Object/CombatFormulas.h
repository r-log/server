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

    /// Crushing chance in basis points: max(attackerMaxSkill - victimMaxSkill, 20)
    /// * 2% - 15%. May be <= 0; the caller gates eligibility and a > 0 check.
    inline int32 CrushingChanceBasisPoints(int32 attackerMaxSkill, int32 victimMaxSkill)
    {
        int32 skillDiff = attackerMaxSkill - victimMaxSkill;
        if (skillDiff < 20)
            skillDiff = 20;
        return skillDiff * 200 - 1500;
    }

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

    /// Base spell hit chance (whole percent) from `leveldif` =
    /// victimLevel - casterLevel. Spell sibling of the melee hit-table
    /// formulas: miss = 100 - hit reproduces the 4.3.4 client's
    /// BASE_MISS_CHANCE_SPELL = {4.0, 5.0, 6.0, 17.0} for PvE offsets
    /// +0..+3 (hit 96/95/94/83). Beyond +2 each extra level adds 11% miss
    /// vs creatures and 7% vs players. Spell mods, victim auras, mechanic
    /// resistance and hit rating stay in Unit::MagicSpellHitResult.
    inline int32 SpellHitChanceBase(int32 leveldif, bool victimIsPlayer)
    {
        if (leveldif < 3)
            return 96 - leveldif;

        int32 lchance = victimIsPlayer ? 7 : 11;
        return 94 - (leveldif - 2) * lchance;
    }

    /// Glancing-blow damage multiplier (Cata 4.0.1): flat 10% reduction per level
    /// the victim is above the attacker, capped at 3 levels (raid boss = 0.70,
    /// i.e. -30%). The pre-4.0.1 weapon-skill random window is gone. Only used
    /// for white melee from a player/pet against a higher-level mob (caller-gated).
    inline float GlancingDamageMultiplier(int32 levelDiff)
    {
        if (levelDiff > 3)
            levelDiff = 3;
        return 1.0f - levelDiff * 0.1f;
    }

    /// Crushing-blow damage: 150% of normal, matching the integer form used in
    /// combat (base + base / 2). Version-invariant across vanilla -> Cata.
    inline uint32 ApplyCrushingDamage(uint32 baseDamage)
    {
        return baseDamage + baseDamage / 2;
    }

    /// Critical-hit damage: 200% of normal (double), matching the integer form
    /// used in melee combat (base + base). Physical melee/ranged crit is a flat
    /// +100% bonus in 4.3.4, unchanged since vanilla (the spell-crit base-bonus
    /// change to 100% was patch 5.0.4/MoP, which does not apply here). Aura
    /// crit-damage modifiers (MOD_CRIT_DAMAGE_BONUS,
    /// MOD_ATTACKER_MELEE_CRIT_DAMAGE) are state-dependent and stay in Unit::.
    inline uint32 ApplyCriticalDamage(uint32 baseDamage)
    {
        return baseDamage + baseDamage;
    }

    /// Spell critical damage: 150% of normal, matching the integer form used
    /// in spell combat (base + base / 2). Magic spell crits are a +50% bonus
    /// in 4.3.4; the change to +100% was patch 5.0.4/MoP, which does not
    /// apply here. Melee/ranged damage-class spells double instead (see
    /// ApplyCriticalDamage). Aura crit-damage modifiers
    /// (MOD_CRIT_DAMAGE_BONUS, MOD_ATTACKER_SPELL_CRIT_DAMAGE) and spell
    /// mods are state-dependent and stay in Unit::SpellCriticalDamageBonus.
    inline uint32 ApplySpellCriticalDamage(uint32 baseDamage)
    {
        return baseDamage + baseDamage / 2;
    }

    /// Spell critical healing: 200% of normal (double), matching the integer
    /// form used in Unit::SpellCriticalHealingBonus (base + base). Healing
    /// crits were raised from 150% to 200% in patch 4.2.0 (2011-06-28), so
    /// 4.3.4 doubles. MOD_CRITICAL_HEALING_AMOUNT auras layer on top in
    /// Unit:: and are out of scope here.
    inline uint32 ApplySpellCriticalHealing(uint32 baseHeal)
    {
        return baseHeal + baseHeal;
    }

    /// Crit chance (percent) derived from a primary stat: (base + statValue *
    /// ratioPerStat) * 100. `base` and `ratioPerStat` are the class/level
    /// coefficients from the Cata GameTables (gtChanceTo{Spell,Melee}Crit{Base}),
    /// authoritative in the 15595 client extract. Used for melee-crit-from-agility
    /// and spell-crit-from-intellect.
    inline float CritChancePercentFromStat(float base, float statValue, float ratioPerStat)
    {
        return (base + statValue * ratioPerStat) * 100.0f;
    }

    /// Combat-rating-to-percent multiplier (percent gained per 1 point of a
    /// combat rating): classScalar / ratingPerPercent. `classScalar` comes from
    /// gtOCTClassCombatRatingScalar and `ratingPerPercent` (rating points needed
    /// for 1% at the unit's level) from gtCombatRatings -- both authoritative in
    /// the 15595 client extract. Caller guards null GameTable entries.
    inline float CombatRatingMultiplier(float classScalar, float ratingPerPercent)
    {
        return classScalar / ratingPerPercent;
    }

    /// Physical-damage reduction fraction [0, 0.75] from armor vs an attacker of
    /// `attackerLevel`. reduction = armor / (armor + K), K = 85*levelModifier + 400.
    /// Three-tier level modifier, matching the 4.3.4 client verbatim
    /// (15595 PaperDollFrame.lua, PaperDollFrame_GetArmorReduction):
    ///   level > 80: level + 4.5*(level-59) + 20*(level-80)
    ///   level > 59: level + 4.5*(level-59)
    ///   else:       level
    /// The +20*(level-80) tier is the Cata change WotLK lacked; without it the
    /// server over-mitigates vs level 81+ attackers (K(85) = 26070, not 17570).
    /// The `armor` passed in is already post-armor-penetration.
    inline float ArmorDamageReductionFraction(float armor, int32 attackerLevel)
    {
        float levelModifier = float(attackerLevel);
        if (levelModifier > 80.0f)
            levelModifier = levelModifier + 4.5f * (levelModifier - 59.0f) + 20.0f * (levelModifier - 80.0f);
        else if (levelModifier > 59.0f)
            levelModifier = levelModifier + 4.5f * (levelModifier - 59.0f);

        float tmpvalue = 0.1f * armor / (8.5f * levelModifier + 40.0f);
        tmpvalue = tmpvalue / (1.0f + tmpvalue);

        if (tmpvalue < 0.0f)
            tmpvalue = 0.0f;
        if (tmpvalue > 0.75f)
            tmpvalue = 0.75f;
        return tmpvalue;
    }

    /// Level-based bonus (in percent) added to an enemy's DODGE chance, where
    /// `levelOffset` = victimLevel - attackerLevel. Matches the 4.3.4 client
    /// (PaperDollFrame.lua BASE_ENEMY_DODGE_CHANCE minus the 5% base): +0.5%
    /// per level, capped at the +3 (boss) tier -> +1.5% (5.0% -> 6.5%). Negative
    /// offsets (attacking lower targets) reduce avoidance by 0.5%/level.
    inline float EnemyDodgeLevelBonus(int32 levelOffset)
    {
        if (levelOffset > 3)
            levelOffset = 3;
        return levelOffset * 0.5f;
    }

    /// Level-based bonus (in percent) added to an enemy's PARRY chance, where
    /// `levelOffset` = victimLevel - attackerLevel. Matches the 4.3.4 client
    /// (PaperDollFrame.lua BASE_ENEMY_PARRY_CHANCE minus the 5% base): +0.5%
    /// per level for +1/+2, then a jump to +9.0% at the +3 (boss) tier
    /// (5.0% -> 14.0%). Negative offsets reduce avoidance by 0.5%/level.
    inline float EnemyParryLevelBonus(int32 levelOffset)
    {
        if (levelOffset >= 3)
            return 9.0f;
        return levelOffset * 0.5f;
    }

    /// Bonus weapon damage per swing from attack power:
    /// attackPower / 14 * attackSpeedSeconds. 14 AP = 1 DPS, version-invariant
    /// vanilla -> Cata (15595 client PaperDollFrame.lua
    /// ATTACK_POWER_MAGIC_NUMBER = 14). The caller selects the speed (real
    /// weapon speed for white swings, normalized speed for some abilities --
    /// both via GetAPMultiplier) and keeps the weapon min/max roll and
    /// school/aura bonuses in Unit::/Player::.
    inline float MeleeAttackPowerDamageBonus(float attackPower, float attackSpeedSeconds)
    {
        return attackPower / 14.0f * attackSpeedSeconds;
    }

    /// Per-source haste time factor: 100 / (100 + hastePct). Exact float form
    /// of the ApplyPercentModFloatVar remove-branch (Util.h) that
    /// Unit::ApplyAttackTimePercentMod / ApplyCastTimePercentMod call for
    /// positive haste, so each independent haste PERCENT source multiplies
    /// the running attack/cast time factor by this -- percent sources stack
    /// multiplicatively in 4.3.4, while haste RATING is summed into one pool
    /// and converted once (Player::ApplyRatingMod re-bases; TC 4.3.4 and
    /// Cata-era references agree).
    /// A negative hastePct (a slow) yields a factor > 1.
    inline float HasteTimeFactor(float hastePct)
    {
        return 100.0f / (100.0f + hastePct);
    }

    /// Effective (hastened) time from a base time and a single haste percent:
    /// baseTime * HasteTimeFactor(hastePct), algebraically baseTime /
    /// (1 + hastePct/100). Units follow the caller -- m3 stores attack time
    /// in milliseconds. Stacking a second source is
    /// HastenedTime(HastenedTime(t, h1), h2); rating->percent conversion is
    /// CombatRatingMultiplier above. Production keeps the stateful
    /// apply/remove bookkeeping in Unit::ApplyAttackTimePercentMod /
    /// ApplyCastTimePercentMod via the shared Util.h helper -- this is the
    /// extracted pure kernel, not a rewire target.
    inline float HastenedTime(float baseTime, float hastePct)
    {
        return baseTime * HasteTimeFactor(hastePct);
    }

    /// Flat spell-power contribution to a spell's damage or healing:
    /// spellPower * coefficient, the exact float multiply from
    /// Unit::SpellBonusWithCoeffs (the caller then applies the level
    /// penalty and truncates to int32). NOTE (characterized, not fixed):
    /// m3 derives `coefficient` from the spell_bonus_data DB table with a
    /// WotLK-era cast-time/3.5s computed fallback (CalculateDefaultCoefficient),
    /// while 4.3.4 is data-driven -- the 15595 SpellEffect.dbc carries an
    /// explicit per-effect coefficient (m_effectBonus, loaded into
    /// SpellEffectEntry::EffectBonusMultiplier but currently unused; TC 4.3.4
    /// multiplies it by the spell scaling multiplier instead of a level
    /// penalty). For periodic effects the DBC value is already per-tick.
    inline float SpellPowerDamageBonus(float spellPower, float coefficient)
    {
        return spellPower * coefficient;
    }
}

#endif // MANGOS_COMBATFORMULAS_H
