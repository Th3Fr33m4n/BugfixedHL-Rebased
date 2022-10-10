/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"
#include "grapple_tonguetip.h"

LINK_ENTITY_TO_CLASS(grapple_tip, CBarnacleGrappleTip)

const char *const grapple_small[] = {
	"monster_bloater",
	"monster_snark",
	"monster_shockroach",
	"monster_rat",
	"monster_alien_babyvoltigore",
	"monster_babycrab",
	"monster_cockroach",
	"monster_flyer_flock",
	"monster_headcrab",
	"monster_leech",
	"monster_penguin"
};

const char *const grapple_medium[] = {
	"monster_alien_controller",
	"monster_alien_slave",
	"monster_barney",
	"monster_bullchicken",
	"monster_cleansuit_scientist",
	"monster_houndeye",
	"monster_human_assassin",
	"monster_human_grunt",
	"monster_human_grunt_ally",
	"monster_human_medic_ally",
	"monster_human_torch_ally",
	"monster_male_assassin",
	"monster_otis",
	"monster_pitdrone",
	"monster_scientist",
	"monster_zombie",
	"monster_zombie_barney",
	"monster_zombie_soldier"
};

const char *const grapple_large[] = {
	"monster_alien_grunt",
	"monster_alien_voltigore",
	"monster_assassin_repel",
	"monster_grunt_ally_repel",
	"monster_bigmomma",
	"monster_gargantua",
	"monster_geneworm",
	"monster_gonome",
	"monster_grunt_repel",
	"monster_ichthyosaur",
	"monster_nihilanth",
	"monster_pitworm",
	"monster_pitworm_up",
	"monster_shocktrooper"
};

const char *const grapple_fixed[] = {
	"monster_barnacle",
	"monster_sitting_cleansuit_scientist",
	"monster_sitting_scientist",
	"monster_tentacle",
	"ammo_spore"
};

void CBarnacleGrappleTip::Precache()
{
	PRECACHE_MODEL("models/shock_effect.mdl");
}

void CBarnacleGrappleTip::Spawn()
{
	Precache();

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/shock_effect.mdl");

	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));

	UTIL_SetOrigin(pev, pev->origin);

	SetThink(&CBarnacleGrappleTip::FlyThink);
	SetTouch(&CBarnacleGrappleTip::TongueTouch);

	Vector vecAngles = pev->angles;

	vecAngles.x -= 30.0;

	pev->angles = vecAngles;

	UTIL_MakeVectors(pev->angles);

	vecAngles.x = -(30.0 + vecAngles.x);

	pev->velocity = g_vecZero;

	pev->gravity = 1.0;

	pev->nextthink = gpGlobals->time + 0.02;

	m_bIsStuck = FALSE;
	m_bMissed = FALSE;
}

void CBarnacleGrappleTip::FlyThink()
{
	UTIL_MakeAimVectors(pev->angles);

	pev->angles = UTIL_VecToAngles(gpGlobals->v_forward);

	const float flNewVel = ((pev->velocity.Length() * 0.8) + 400.0);

	pev->velocity = pev->velocity * 0.2 + (flNewVel * gpGlobals->v_forward);

	if (!g_pGameRules->IsMultiplayer())
	{
		//Note: the old grapple had a maximum velocity of 1600. - Solokiller
		if (pev->velocity.Length() > 750.0)
		{
			pev->velocity = pev->velocity.Normalized() * 750.0;
		}
	}
	else
	{
		//TODO: should probably clamp at sv_maxvelocity to prevent the tip from going off course. - Solokiller
		if (pev->velocity.Length() > 2000.0)
		{
			pev->velocity = pev->velocity.Normalized() * 2000.0;
		}
	}

	pev->nextthink = gpGlobals->time + 0.02;
}

void CBarnacleGrappleTip::OffsetThink()
{
	//Nothing
}

void CBarnacleGrappleTip::TongueTouch(CBaseEntity *pOther)
{
	if (!pOther)
	{
		targetClass = GRAPPLE_NOT_A_TARGET;
		m_bMissed = TRUE;
	}
	else
	{
		if (pOther->IsPlayer() && pOther->IsAlive())
		{
			targetClass = GRAPPLE_MEDIUM;

			m_hGrappleTarget = pOther;

			m_bIsStuck = TRUE;
		}
		else
		{
			targetClass = CheckTarget(pOther);

			if (targetClass != GRAPPLE_NOT_A_TARGET)
			{
				m_bIsStuck = TRUE;
			}
			else
			{
				m_bMissed = TRUE;
			}
		}
	}

	if (!m_bMissed && targetClass != GRAPPLE_FIXED) {
		pev->movetype = MOVETYPE_FOLLOW;
		pev->aiment = m_hGrappleTarget->edict();
	}

	pev->velocity = g_vecZero;

	m_GrappleType = targetClass;

	SetThink(&CBarnacleGrappleTip::OffsetThink);
	pev->nextthink = gpGlobals->time + 0.02;

	SetTouch(NULL);
}

int CBarnacleGrappleTip::CheckTarget(CBaseEntity *pTarget)
{
	if (!pTarget)
		return GRAPPLE_NOT_A_TARGET;

	if (pTarget->IsPlayer())
	{
		m_hGrappleTarget = pTarget;
		return pTarget->IsAlive() ? GRAPPLE_MEDIUM : GRAPPLE_NOT_A_TARGET;
	}

	Vector vecStart = pev->origin;
	Vector vecEnd = pev->origin + pev->velocity * 1024.0;

	TraceResult tr;

	UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, edict(), &tr);

	CBaseEntity *pHit = Instance(tr.pHit);

	/*	if( !pHit )
		pHit = CWorld::GetInstance();*/

	float rgfl1[3];
	float rgfl2[3];
	const char *pTexture;

	vecStart.CopyToArray(rgfl1);
	vecEnd.CopyToArray(rgfl2);

	if (pHit)
		pTexture = TRACE_TEXTURE(ENT(pHit->pev), rgfl1, rgfl2);
	else
		pTexture = TRACE_TEXTURE(ENT(0), rgfl1, rgfl2);

	bool bIsFixed = false;

	if (pTexture && strnicmp(pTexture, "xeno_grapple", 12) == 0)
	{
		bIsFixed = true;
	}
	else
	{
		for (size_t uiIndex = 0; uiIndex < ARRAYSIZE(grapple_small); ++uiIndex)
		{
			if (FClassnameIs(pTarget->pev, grapple_small[uiIndex]) && pTarget->IsAlive())
			{
				m_hGrappleTarget = pTarget;
				m_vecOriginOffset = pev->origin - pTarget->pev->origin;

				return GRAPPLE_SMALL;
			}
		}

		for (size_t uiIndex = 0; uiIndex < ARRAYSIZE(grapple_medium); ++uiIndex)
		{
			if (FClassnameIs(pTarget->pev, grapple_medium[uiIndex]) && pTarget->IsAlive())
			{
				m_hGrappleTarget = pTarget;
				m_vecOriginOffset = pev->origin - pTarget->pev->origin;

				return GRAPPLE_MEDIUM;
			}
		}

		for (size_t uiIndex = 0; uiIndex < ARRAYSIZE(grapple_large); ++uiIndex)
		{
			if (FClassnameIs(pTarget->pev, grapple_large[uiIndex]) && pTarget->IsAlive())
			{
				m_hGrappleTarget = pTarget;
				m_vecOriginOffset = pev->origin - pTarget->pev->origin;

				return GRAPPLE_LARGE;
			}
		}

		for (size_t uiIndex = 0; uiIndex < ARRAYSIZE(grapple_fixed); ++uiIndex)
		{
			if (FClassnameIs(pTarget->pev, grapple_fixed[uiIndex]))
			{
				bIsFixed = true;
				break;
			}
		}
	}

	if (bIsFixed)
	{
		m_hGrappleTarget = pTarget;
		m_vecOriginOffset = g_vecZero;

		return GRAPPLE_FIXED;
	}

	return GRAPPLE_NOT_A_TARGET;
}

void CBarnacleGrappleTip::SetPosition(Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner)
{
	UTIL_SetOrigin(pev, vecOrigin);
	pev->angles = vecAngles;
	pev->owner = pOwner->edict();
}
