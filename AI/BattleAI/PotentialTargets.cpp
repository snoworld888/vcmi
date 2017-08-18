/*
 * PotentialTargets.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "PotentialTargets.h"

PotentialTargets::PotentialTargets(const CStack * attacker, const HypotheticBattle & state)
{
	auto attackerInfo = getValOr(state.stackStates, attacker->unitId(), std::make_shared<StackWithBonuses>(&attacker->stackState));

	auto dists = getCbc()->battleGetDistances(&attackerInfo->state, attackerInfo->state.position);
	auto avHexes = getCbc()->battleGetAvailableHexes(&attackerInfo->state, attackerInfo->state.position);

	//FIXME: this should part of battleGetAvailableHexes
	bool forcedTarget = false;
	const CStack * forcedStack = nullptr;
	BattleHex forcedHex;

	if(attackerInfo->hasBonusOfType(Bonus::ATTACKS_NEAREST_CREATURE))
	{
		forcedTarget = true;
		auto nearest = getCbc()->getNearestStack(attacker, boost::none);

		if(nearest.first != nullptr)
		{
			forcedStack = nearest.first;
			forcedHex = nearest.second;
		}
	}

	for(const CStack * defender : getCbc()->battleGetStacks())
	{
		auto defenderInfo = getValOr(state.stackStates, defender->unitId(), std::make_shared<StackWithBonuses>(&defender->stackState));

		if(!forcedTarget && !getCbc()->battleMatchOwner(&attackerInfo->state, &defenderInfo->state))
			continue;

		auto GenerateAttackInfo = [&](bool shooting, BattleHex hex) -> AttackPossibility
		{
			auto bai = BattleAttackInfo(attackerInfo->state, defenderInfo->state, shooting);

			if(hex.isValid() && !shooting)
				bai.chargedFields = dists[hex];

			return AttackPossibility::evaluate(bai, hex);
		};

		if(forcedTarget)
		{
			if(forcedStack && defender->ID == forcedStack->ID)
				possibleAttacks.push_back(GenerateAttackInfo(false, forcedHex));
			else
				unreachableEnemies.push_back(defender);
		}
		else if(getCbc()->battleCanShoot(attacker, defenderInfo->state.position))
		{
			possibleAttacks.push_back(GenerateAttackInfo(true, BattleHex::INVALID));
		}
		else
		{
			for(BattleHex hex : avHexes)
				if(CStack::isMeleeAttackPossible(attacker, defender, hex))
					possibleAttacks.push_back(GenerateAttackInfo(false, hex));

			if(!vstd::contains_if(possibleAttacks, [=](const AttackPossibility &pa) { return pa.enemy.unitId() == defender->unitId(); }))
				unreachableEnemies.push_back(defender);
		}
	}
}

int PotentialTargets::bestActionValue() const
{
	if(possibleAttacks.empty())
		return 0;

	return bestAction().attackValue();
}

AttackPossibility PotentialTargets::bestAction() const
{
	if(possibleAttacks.empty())
		throw std::runtime_error("No best action, since we don't have any actions");

	return *vstd::maxElementByFun(possibleAttacks, [](const AttackPossibility &ap) { return ap.attackValue(); } );
}
