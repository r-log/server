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
}

#endif // MANGOS_COMBATFORMULAS_H
