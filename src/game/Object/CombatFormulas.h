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
}

#endif // MANGOS_COMBATFORMULAS_H
