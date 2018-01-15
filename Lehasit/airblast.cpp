#include "stdafx.h"
#include "cheats.h"

#include "config.h"
#include "sdk/CUserCmd.h"
#include "sdk/CBaseCombatWeapon.h"
#include "sdk/tf.h"
#include "sdk/Recv.h"
#include "Globals.h"
#include "sdk/trace.h"
#include "sdk/network.h"
#include "sdk/mathlib.h"

enum SpikeType
{
	SPIKE_NONE,
	SPIKE_MANUAL,
	SPIKE_UP,
	SPIKE_DOWN,
	SPIKE_LEFT,
	SPIKE_RIGHT
};

bool airblast::Run(CUserCmd* pCmd, std::vector<CBaseEntity*> deflectableProjectiles)
{
	static QAngle lastAirblastAngle = { 0, 0, 0 };
	static bool didAirblastLastRound = false;

	if (!g_config.airblast_enable)
		return false;

	// Store default angles for later movement fixing
	QAngle oldAngles = pCmd->viewangles;

	// Spike if possible, airblast angles will overwrite these ones if needed
	if (didAirblastLastRound)
	{
		switch (g_config.airblast_spike_type)
		{
			case SPIKE_NONE:
				pCmd->viewangles = lastAirblastAngle;
				break;

			case SPIKE_MANUAL:
				break;

			case SPIKE_UP:
				pCmd->viewangles[PITCH] = -89;
				pCmd->viewangles[YAW] = lastAirblastAngle[YAW];
				break;

			case SPIKE_DOWN:
				pCmd->viewangles[PITCH] = 89;
				pCmd->viewangles[YAW] = lastAirblastAngle[YAW];
				break;

			case SPIKE_LEFT:
				pCmd->viewangles[PITCH] = lastAirblastAngle[PITCH];
				pCmd->viewangles[YAW] = NormalizeAngle(lastAirblastAngle[YAW] + 90.f);
				break;

			case SPIKE_RIGHT:
				pCmd->viewangles[PITCH] = lastAirblastAngle[PITCH];
				pCmd->viewangles[YAW] = NormalizeAngle(lastAirblastAngle[YAW] - 90.f);
				break;
		}

		utils::MovementFix(pCmd, oldAngles);

		didAirblastLastRound = false;
		return true;
	}

	CTFPlayer* pLocalPlayer = utils::GetLocalPlayer();

	if (pLocalPlayer->getLifestate() != LIFE_ALIVE)
		return false;

	CBaseCombatWeapon* pWeapon = pLocalPlayer->getActiveWeapon();
	// No weapon
	if (!pWeapon)
		return false;

	// Not a flamethrower
	if (strcmp(pWeapon->get_client_class()->name, "CTFFlameThrower") != 0)
		return false;

	// Can't airblast now
	if (pWeapon->getNextSecondaryAttackTime() > g_pGlobals->curtime)
	{
		pCmd->buttons &= ~IN_ATTACK2;
		return false;
	}

	// Default tf2 doesn't need special lag compensation
	float latency = 0;

	// There is no lag compensation applied to tfdb tracking rockets, meaning we have to do prediction ourselves
	if (g_config.airblast_tfdb_lagfix)
	{
		auto netinfo = g_interfaces.engine->GetNetChannelInfo();
		latency = netinfo->GetLatency(MAX_FLOWS); // Get incoming and outgoing latency
	}

	CBaseEntity* pClosestProjectile{};
	Vector closestPredictedCenter;
	float closestDistance = -1337.f;
	for (CBaseEntity* pProjectile : deflectableProjectiles)
	{
		Vector vel;
		pProjectile->EstimateAbsVelocity(vel);

		Vector predictpos = pProjectile->GetWorldSpaceCenter() + (vel * latency);
		float dist = (utils::GetLocalViewOrigin() - predictpos).length();

		if (dist < closestDistance || !pClosestProjectile)
		{
			pClosestProjectile = pProjectile;
			closestDistance = dist;
			closestPredictedCenter = predictpos;
		}
	}

	if (pClosestProjectile && closestDistance <= 185)
	{
		QAngle angles = utils::CalculateAngle(utils::GetLocalViewOrigin(), closestPredictedCenter);
		pCmd->viewangles = angles;
		utils::MovementFix(pCmd, oldAngles);
		pCmd->buttons |= IN_ATTACK2;

		lastAirblastAngle = pCmd->viewangles;
		didAirblastLastRound = true;

		return true;
	}

	return false;
}