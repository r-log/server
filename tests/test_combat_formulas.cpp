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
