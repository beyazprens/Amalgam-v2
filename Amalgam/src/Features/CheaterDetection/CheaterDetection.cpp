#include "CheaterDetection.h"

#include "../Players/PlayerUtils.h"
#include "../Output/Output.h"

bool CCheaterDetection::ShouldScan()
{
	if (!Vars::CheaterDetection::Methods.Value /*|| I::EngineClient->IsPlayingDemo()*/)
		return false;

	static int iStaticTickcount = I::GlobalVars->tickcount;
	const int iLastTickcount = iStaticTickcount;
	const int iCurrTickcount = iStaticTickcount = I::GlobalVars->tickcount;
	if (iCurrTickcount != iLastTickcount + 1)
		return false;

	auto pNetChan = I::EngineClient->GetNetChannelInfo();
	if (pNetChan && (pNetChan->GetTimeSinceLastReceived() > TICK_INTERVAL * 2 || pNetChan->IsTimingOut()))
		return false;

	return true;
}

bool CCheaterDetection::InvalidPitch(CTFPlayer* pEntity)
{
	return Vars::CheaterDetection::Methods.Value & Vars::CheaterDetection::MethodsEnum::InvalidPitch && fabsf(pEntity->m_angEyeAnglesX()) == 90.f;
}

bool CCheaterDetection::IsChoking(CTFPlayer* pEntity)
{
	bool bReturn = mData[pEntity].m_PacketChoking.m_bInfract;
	mData[pEntity].m_PacketChoking.m_bInfract = false;

	return Vars::CheaterDetection::Methods.Value & Vars::CheaterDetection::MethodsEnum::PacketChoking && bReturn;
}

bool CCheaterDetection::IsFlicking(CTFPlayer* pEntity) // awful
{
	auto& vAngles = mData[pEntity].m_AimFlicking.m_vAngles;
	if (!(Vars::CheaterDetection::Methods.Value & Vars::CheaterDetection::MethodsEnum::AimFlicking))
	{
		vAngles.clear();
		return false;
	}

	vAngles.emplace_front(pEntity->GetEyeAngles(), false);
	if (vAngles.size() > 3)
		vAngles.pop_back();

	if (vAngles.size() != 3 || !vAngles[0].m_bAttacking && !vAngles[1].m_bAttacking && !vAngles[2].m_bAttacking
		|| Math::CalcFov(vAngles[0].m_vAngle, vAngles[1].m_vAngle) < Vars::CheaterDetection::MinimumFlick.Value
		|| Math::CalcFov(vAngles[0].m_vAngle, vAngles[2].m_vAngle) > Vars::CheaterDetection::MaximumNoise.Value * (TICK_INTERVAL / 0.015f))
		return false;

	vAngles.clear();
	return true;
}

bool CCheaterDetection::IsDuckSpeed(CTFPlayer* pEntity)
{
	if (!(Vars::CheaterDetection::Methods.Value & Vars::CheaterDetection::MethodsEnum::DuckSpeed)
		|| !pEntity->IsDucking() || !pEntity->IsOnGround()
		|| pEntity->m_vecVelocity().Length2D() < pEntity->m_flMaxspeed() * 0.5f)
	{
		mData[pEntity].m_DuckSpeed.m_iStartTick = 0;
		return false;
	}

	if (!mData[pEntity].m_DuckSpeed.m_iStartTick)
		mData[pEntity].m_DuckSpeed.m_iStartTick = I::GlobalVars->tickcount;

	if (I::GlobalVars->tickcount - mData[pEntity].m_DuckSpeed.m_iStartTick > TIME_TO_TICKS(1))
	{
		mData[pEntity].m_DuckSpeed.m_iStartTick = 0;
		return true;
	}

	return false;
}

bool CCheaterDetection::IsLagCompAbusing(CTFPlayer* pEntity, int iDeltaTicks)
{
	auto& tLagComp = mData[pEntity].m_PacketChoking.m_LagComp;
	if (!(Vars::CheaterDetection::Methods.Value & Vars::CheaterDetection::MethodsEnum::LagCompAbuse))
	{
		tLagComp = {};
		return false;
	}

	const int iMinDelta = std::max(2, Vars::CheaterDetection::LagCompMinimumDelta.Value);
	const int iWindowTicks = std::max(1, TIME_TO_TICKS(std::max(0.1f, Vars::CheaterDetection::LagCompWindow.Value)));
	const int iRequiredBursts = std::max(1, Vars::CheaterDetection::LagCompBurstCount.Value);

	if (iDeltaTicks <= iMinDelta)
	{
		return false;
	}

	tLagComp.m_vBurstTicks.emplace_back(I::GlobalVars->tickcount);
	tLagComp.m_vDeltaCmds.emplace_back(iDeltaTicks);

	while (!tLagComp.m_vBurstTicks.empty() && I::GlobalVars->tickcount - tLagComp.m_vBurstTicks.front() > iWindowTicks)
	{
		tLagComp.m_vBurstTicks.pop_front();
		tLagComp.m_vDeltaCmds.pop_front();
	}

	if ((int)tLagComp.m_vBurstTicks.size() >= iRequiredBursts)
	{
		tLagComp.m_vBurstTicks.clear();
		tLagComp.m_vDeltaCmds.clear();
		tLagComp.m_bInfract = true;
	}

	bool bReturn = tLagComp.m_bInfract;
	tLagComp.m_bInfract = false;
	return bReturn;
}

bool CCheaterDetection::IsCritManipulating(CTFPlayer* pEntity)
{
	auto& tCritTracker = mData[pEntity].m_CritTracker;
	if (!(Vars::CheaterDetection::Methods.Value & Vars::CheaterDetection::MethodsEnum::CritManipulation))
	{
		tCritTracker = {};
		return false;
	}

	bool bReturn = tCritTracker.m_bInfract;
	tCritTracker.m_bInfract = false;
	return bReturn;
}

bool CCheaterDetection::IsTriggerBot(CTFPlayer* pEntity)
{
	auto& tTriggerBot = mData[pEntity].m_TriggerBot;
	if (!(Vars::CheaterDetection::Methods.Value & Vars::CheaterDetection::MethodsEnum::TriggerBot))
	{
		tTriggerBot = {};
		return false;
	}

	bool bReturn = tTriggerBot.m_bInfract;
	tTriggerBot.m_bInfract = false;
	return bReturn;
}

void CCheaterDetection::TrackTriggerBot(CTFPlayer* pEntity)
{
	auto& tTriggerBot = mData[pEntity].m_TriggerBot;
	if (!(Vars::CheaterDetection::Methods.Value & Vars::CheaterDetection::MethodsEnum::TriggerBot))
	{
		tTriggerBot.m_mFirstAimTime.clear();
		return;
	}

	// Only detect triggerbot for snipers
	if (pEntity->m_iClass() != TF_CLASS_SNIPER)
	{
		tTriggerBot.m_mFirstAimTime.clear();
		return;
	}

	// Verify they have an active sniper rifle
	auto pWeapon = pEntity->m_hActiveWeapon()->As<CTFWeaponBase>();
	if (!pWeapon)
	{
		tTriggerBot.m_mFirstAimTime.clear();
		return;
	}

	const int nWeaponID = pWeapon->GetWeaponID();
	bool bAiming = false;
	if (nWeaponID == TF_WEAPON_SNIPERRIFLE || nWeaponID == TF_WEAPON_SNIPERRIFLE_DECAP)
		bAiming = pEntity->InCond(TF_COND_ZOOMED);
	else if (nWeaponID == TF_WEAPON_SNIPERRIFLE_CLASSIC)
		bAiming = pWeapon->As<CTFSniperRifleClassic>()->m_bCharging();
	else
	{
		tTriggerBot.m_mFirstAimTime.clear();
		return;
	}

	// Only track while the sniper is scoped/charging
	if (!bAiming)
	{
		tTriggerBot.m_mFirstAimTime.clear();
		return;
	}

	const Vec3 vEyePos = pEntity->m_vecOrigin() + pEntity->GetViewOffset();
	const Vec3 vEyeAngles = pEntity->GetEyeAngles();
	const float flMaxFOV = Vars::CheaterDetection::TriggerBotMaxFOV.Value;

	std::unordered_set<int> currentTargets;

	for (auto& pEnemyEntity : H::Entities.GetGroup(EntityEnum::PlayerAll))
	{
		auto pEnemy = pEnemyEntity->As<CTFPlayer>();
		if (!pEnemy || pEnemy == pEntity)
			continue;
		if (pEnemy->IsDormant() || !pEnemy->IsAlive() || pEnemy->IsAGhost())
			continue;
		if (pEnemy->m_iTeamNum() == pEntity->m_iTeamNum())
			continue;

		const int iEnemyIndex = pEnemy->entindex();
		const Vec3 vHeadPos = pEnemy->m_vecOrigin() + pEnemy->GetViewOffset();
		const Vec3 vAngleTo = Math::CalcAngle(vEyePos, vHeadPos);

		if (Math::CalcFov(vEyeAngles, vAngleTo) > flMaxFOV)
			continue;
		if (!SDK::VisPos(pEntity, pEnemy, vEyePos, vHeadPos))
			continue;

		currentTargets.insert(iEnemyIndex);
		if (tTriggerBot.m_mFirstAimTime.find(iEnemyIndex) == tTriggerBot.m_mFirstAimTime.end())
			tTriggerBot.m_mFirstAimTime[iEnemyIndex] = I::GlobalVars->curtime;
	}

	// Remove enemies no longer in the attacker's view
	for (auto it = tTriggerBot.m_mFirstAimTime.begin(); it != tTriggerBot.m_mFirstAimTime.end();)
	{
		if (currentTargets.find(it->first) == currentTargets.end())
			it = tTriggerBot.m_mFirstAimTime.erase(it);
		else
			++it;
	}
}

bool CCheaterDetection::IsHitboxAbusing(CTFPlayer* pEntity)
{
	auto& tHitboxAbuse = mData[pEntity].m_HitboxAbuse;
	if (!(Vars::CheaterDetection::Methods.Value & Vars::CheaterDetection::MethodsEnum::HitboxAbuse))
	{
		tHitboxAbuse = {};
		return false;
	}

	bool bReturn = tHitboxAbuse.m_bInfract;
	tHitboxAbuse.m_bInfract = false;
	return bReturn;
}

bool CCheaterDetection::IsSpeedHacking(CTFPlayer* pEntity, float flDeltaTime)
{
	auto& tSpeedHack = mData[pEntity].m_SpeedHack;
	if (!(Vars::CheaterDetection::Methods.Value & Vars::CheaterDetection::MethodsEnum::SpeedHack))
	{
		tSpeedHack = {};
		return false;
	}

	// Only check grounded players — airborne movement can have large Z-velocity from jumps
	if (!pEntity->IsOnGround())
	{
		tSpeedHack.m_bHasLastPos = false;
		tSpeedHack.m_iConsecutiveViolations = 0;
		return false;
	}

	// Skip players under any speed-altering condition to avoid false positives:
	// TF_COND_SPEED_BOOST        – Disciplinary Action / Soldier War Cry
	// TF_COND_HALLOWEEN_SPEED_BOOST – Halloween gift / boss proximity
	// TF_COND_RUNE_HASTE         – Powerup mode Haste rune (2× speed)
	// TF_COND_RUNE_AGILITY       – Powerup mode Agility rune (1.3× speed)
	// TF_COND_BLASTJUMPING       – residual horizontal velocity on landing tick
	if (pEntity->InCond(TF_COND_SPEED_BOOST) || pEntity->InCond(TF_COND_HALLOWEEN_SPEED_BOOST)
		|| pEntity->InCond(TF_COND_RUNE_HASTE) || pEntity->InCond(TF_COND_RUNE_AGILITY)
		|| pEntity->InCond(TF_COND_BLASTJUMPING))
	{
		tSpeedHack.m_bHasLastPos = false;
		tSpeedHack.m_iConsecutiveViolations = 0;
		return false;
	}

	const Vec3 vCurrPos = pEntity->m_vecOrigin();

	if (!tSpeedHack.m_bHasLastPos || flDeltaTime <= 0.f)
	{
		tSpeedHack.m_vLastPos = vCurrPos;
		tSpeedHack.m_flLastTime = I::GlobalVars->curtime;
		tSpeedHack.m_bHasLastPos = true;
		tSpeedHack.m_iConsecutiveViolations = 0;
		return false;
	}

	// Compute horizontal speed from position delta to avoid relying on the (potentially spoofed) velocity netvar
	const Vec3 vDelta = vCurrPos - tSpeedHack.m_vLastPos;
	const float flHorizSpeed = Vec3(vDelta.x, vDelta.y, 0.f).Length() / flDeltaTime;

	tSpeedHack.m_vLastPos = vCurrPos;
	tSpeedHack.m_flLastTime = I::GlobalVars->curtime;

	const float flThreshold = Vars::CheaterDetection::SpeedHackMaxVelocity.Value;
	if (flHorizSpeed > flThreshold)
	{
		tSpeedHack.m_iConsecutiveViolations++;
		if (tSpeedHack.m_iConsecutiveViolations >= Vars::CheaterDetection::SpeedHackConsecutive.Value)
		{
			tSpeedHack.m_iConsecutiveViolations = 0;
			return true;
		}
	}
	else
	{
		tSpeedHack.m_iConsecutiveViolations = 0;
	}

	return false;
}

void CCheaterDetection::TrackReactionTime(CTFPlayer* pEntity)
{
	auto& tReactionTime = mData[pEntity].m_ReactionTime;
	if (!(Vars::CheaterDetection::Methods.Value & Vars::CheaterDetection::MethodsEnum::ReactionTime))
	{
		tReactionTime.m_mFirstVisibleTime.clear();
		return;
	}

	// Only track players who can deal hitscan damage (not cloaked spies, etc.)
	auto pWeapon = pEntity->m_hActiveWeapon()->As<CTFWeaponBase>();
	if (!pWeapon)
	{
		tReactionTime.m_mFirstVisibleTime.clear();
		return;
	}

	switch (SDK::GetWeaponType(pWeapon))
	{
	case EWeaponType::HITSCAN:
		break; // only track hitscan weapons for reaction time
	default:
		tReactionTime.m_mFirstVisibleTime.clear();
		return;
	}

	const Vec3 vEyePos = pEntity->m_vecOrigin() + pEntity->GetViewOffset();
	const Vec3 vEyeAngles = pEntity->GetEyeAngles();

	std::unordered_set<int> currentVisible;

	for (auto& pEnemyEntity : H::Entities.GetGroup(EntityEnum::PlayerAll))
	{
		auto pEnemy = pEnemyEntity->As<CTFPlayer>();
		if (!pEnemy || pEnemy == pEntity)
			continue;
		if (pEnemy->IsDormant() || !pEnemy->IsAlive() || pEnemy->IsAGhost())
			continue;
		if (pEnemy->m_iTeamNum() == pEntity->m_iTeamNum())
			continue;

		const int iEnemyIndex = pEnemy->entindex();
		const Vec3 vHeadPos = pEnemy->m_vecOrigin() + pEnemy->GetViewOffset();

		// Require both line-of-sight and that the enemy is within roughly the attacker's FOV
		if (!SDK::VisPos(pEntity, pEnemy, vEyePos, vHeadPos))
			continue;

		const Vec3 vAngleTo = Math::CalcAngle(vEyePos, vHeadPos);
		if (Math::CalcFov(vEyeAngles, vAngleTo) > 60.f)
			continue;

		currentVisible.insert(iEnemyIndex);
		if (tReactionTime.m_mFirstVisibleTime.find(iEnemyIndex) == tReactionTime.m_mFirstVisibleTime.end())
			tReactionTime.m_mFirstVisibleTime[iEnemyIndex] = I::GlobalVars->curtime;
	}

	// Remove enemies that are no longer visible
	for (auto it = tReactionTime.m_mFirstVisibleTime.begin(); it != tReactionTime.m_mFirstVisibleTime.end();)
	{
		if (currentVisible.find(it->first) == currentVisible.end())
			it = tReactionTime.m_mFirstVisibleTime.erase(it);
		else
			++it;
	}
}

bool CCheaterDetection::IsReactionTimeAnomaly(CTFPlayer* pEntity)
{
	auto& tReactionTime = mData[pEntity].m_ReactionTime;
	if (!(Vars::CheaterDetection::Methods.Value & Vars::CheaterDetection::MethodsEnum::ReactionTime))
	{
		tReactionTime = {};
		return false;
	}

	bool bReturn = tReactionTime.m_bInfract;
	tReactionTime.m_bInfract = false;
	return bReturn;
}

bool CCheaterDetection::IsAntiAiming(CTFPlayer* pEntity)
{
	auto& tAntiAim = mData[pEntity].m_AntiAim;
	if (!(Vars::CheaterDetection::Methods.Value & Vars::CheaterDetection::MethodsEnum::AntiAim))
	{
		tAntiAim = {};
		return false;
	}

	// Need significant horizontal velocity to compare against — ignore near-stationary players
	const Vec3 vVelocity = pEntity->m_vecVelocity();
	const float flSpeed2D = vVelocity.Length2D();
	if (flSpeed2D < 50.f)
	{
		tAntiAim.m_iConsecutiveTicks = 0;
		return false;
	}

	// Convert velocity direction to a yaw angle
	const float flVelocityYaw = Math::VectorAngles(Vec3(vVelocity.x, vVelocity.y, 0.f)).y;
	const float flViewYaw = pEntity->GetEyeAngles().y;

	// Normalize absolute yaw difference to [0, 180].
	// Eye yaw is in [-180, 180] and VectorAngles yaw is also in [-180, 180],
	// so the raw fabsf difference can be at most 360. Clamp to 360 first, then
	// fold the upper half back into [0, 180].
	float flDiff = fabsf(flViewYaw - flVelocityYaw);
	if (flDiff > 360.f)
		flDiff = 360.f;
	if (flDiff > 180.f)
		flDiff = 360.f - flDiff;

	const float flMin = Vars::CheaterDetection::AntiAimMinDeviation.Value;
	const float flMax = Vars::CheaterDetection::AntiAimMaxDeviation.Value;

	// flDiff is in [0, 180], so cap flMax at 180 for the comparison
	const float flMaxClamped = std::min(flMax, 180.f);

	if (flDiff >= flMin && flDiff <= flMaxClamped)
	{
		tAntiAim.m_iConsecutiveTicks++;
		if (tAntiAim.m_iConsecutiveTicks >= Vars::CheaterDetection::AntiAimConsecutive.Value)
		{
			tAntiAim.m_iConsecutiveTicks = 0;
			return true;
		}
	}
	else
	{
		tAntiAim.m_iConsecutiveTicks = 0;
	}

	return false;
}

void CCheaterDetection::TrackCritEvent(CTFPlayer* pEntity, CTFWeaponBase* pWeapon, bool bCrit)
{
	if (!(Vars::CheaterDetection::Methods.Value & Vars::CheaterDetection::MethodsEnum::CritManipulation) || !pWeapon)
		return;

	auto& tCritTracker = mData[pEntity].m_CritTracker;

	if (pEntity->IsCritBoosted())
	{
		tCritTracker.m_mWeaponHistory.erase(pWeapon->GetWeaponID());
		return;
	}

	auto& tHistory = tCritTracker.m_mWeaponHistory[pWeapon->GetWeaponID()];
	tHistory.m_vHistory.emplace_back(bCrit);
	if (bCrit)
		tHistory.m_iCrits++;

	const int iWindow = std::max(1, Vars::CheaterDetection::CritWindow.Value);
	while ((int)tHistory.m_vHistory.size() > iWindow)
	{
		if (tHistory.m_vHistory.front())
			tHistory.m_iCrits--;
		tHistory.m_vHistory.pop_front();
	}

	if ((int)tHistory.m_vHistory.size() < iWindow)
		return;

	const float flCritRate = (float(tHistory.m_iCrits) / float(tHistory.m_vHistory.size())) * 100.f;
	if (flCritRate >= Vars::CheaterDetection::CritThreshold.Value)
	{
		tHistory.m_vHistory.clear();
		tHistory.m_iCrits = 0;
		tCritTracker.m_bInfract = true;
	}
}

void CCheaterDetection::Infract(CTFPlayer* pEntity, const char* sReason)
{
	bool bMark = false;
	if (Vars::CheaterDetection::DetectionsRequired.Value)
	{
		mData[pEntity].m_iDetections++;
		bMark = mData[pEntity].m_iDetections >= Vars::CheaterDetection::DetectionsRequired.Value;
	}

	F::Output.CheatDetection(mData[pEntity].m_sName, bMark ? "marked" : "infracted", sReason);
	if (bMark)
	{
		const int iDetections = std::max(mData[pEntity].m_iDetections, Vars::CheaterDetection::DetectionsRequired.Value);
		mData[pEntity].m_iDetections = 0;
		F::PlayerUtils.AddTag(
			mData[pEntity].m_uAccountID,
			F::PlayerUtils.TagToIndex(CHEATER_TAG),
			true,
			mData[pEntity].m_sName,
			sReason,
			iDetections,
			true);
	}
}

void CCheaterDetection::Run()
{
	if (!ShouldScan() || !I::EngineClient->IsConnected() || I::EngineClient->IsPlayingDemo())
		return;

	auto pResource = H::Entities.GetResource();
	if (!pResource)
		return;

	for (auto& pEntity : H::Entities.GetGroup(EntityEnum::PlayerAll))
	{
		auto pPlayer = pEntity->As<CTFPlayer>();
		int iIndex = pPlayer->entindex();
		float flDeltaTime = H::Entities.GetDeltaTime(iIndex);

		const int iDeltaTicks = TIME_TO_TICKS(flDeltaTime);

		if (iIndex == I::EngineClient->GetLocalPlayer() || !pPlayer->IsAlive() || pPlayer->IsAGhost()
			|| pResource->IsFakePlayer(iIndex) || F::PlayerUtils.HasTag(iIndex, F::PlayerUtils.TagToIndex(CHEATER_TAG)))
		{
			mData[pPlayer].m_PacketChoking = {};
			mData[pPlayer].m_AimFlicking = {};
			mData[pPlayer].m_DuckSpeed = {};
			mData[pPlayer].m_CritTracker = {};
			mData[pPlayer].m_TriggerBot = {};
			mData[pPlayer].m_HitboxAbuse = {};
			mData[pPlayer].m_SpeedHack = {};
			mData[pPlayer].m_ReactionTime = {};
			mData[pPlayer].m_AntiAim = {};
			continue;
		}

		// Track dormancy state for all alive players (even with zero delta) to detect transitions.
		// When a player exits dormancy their simulation time jumps, which would otherwise be
		// mistaken for lag-comp abuse. Clearing the burst history on that transition prevents it.
		const bool bCurrDormant = pPlayer->IsDormant();
		const bool bWasDormant = mData[pPlayer].m_PacketChoking.m_LagComp.m_bWasDormant;
		mData[pPlayer].m_PacketChoking.m_LagComp.m_bWasDormant = bCurrDormant;

		if (!flDeltaTime)
			continue;

		mData[pPlayer].m_uAccountID = pResource->m_iAccountID(iIndex);
		mData[pPlayer].m_sName = F::PlayerUtils.GetPlayerName(iIndex, pResource->GetName(iIndex));

		if (InvalidPitch(pPlayer))
			Infract(pPlayer, "invalid pitch");
		if (IsChoking(pPlayer))
			Infract(pPlayer, "choking packets");
		if (IsFlicking(pPlayer))
			Infract(pPlayer, "flicking");
		if (IsDuckSpeed(pPlayer))
			Infract(pPlayer, "duck speed");
		if (bWasDormant && !bCurrDormant)
		{
			// Player just became visible after dormancy; clear accumulated bursts so the
			// sim-time catch-up on this frame is not mistakenly counted as lag-comp abuse.
			mData[pPlayer].m_PacketChoking.m_LagComp.m_vBurstTicks.clear();
			mData[pPlayer].m_PacketChoking.m_LagComp.m_vDeltaCmds.clear();
			// Also reset position-based detectors: the player may have teleported while
			// dormant, so the first position delta would produce a spurious speed spike.
			mData[pPlayer].m_SpeedHack.m_bHasLastPos = false;
			mData[pPlayer].m_SpeedHack.m_iConsecutiveViolations = 0;
			mData[pPlayer].m_ReactionTime.m_mFirstVisibleTime.clear();
		}
		else if (IsLagCompAbusing(pPlayer, iDeltaTicks))
			Infract(pPlayer, "lag-comp abuse");
		if (IsCritManipulating(pPlayer))
			Infract(pPlayer, "crit manipulation");
		TrackTriggerBot(pPlayer);
		if (IsTriggerBot(pPlayer))
			Infract(pPlayer, "triggerbot");
		if (IsHitboxAbusing(pPlayer))
			Infract(pPlayer, "hitbox abuse");
		if (IsSpeedHacking(pPlayer, flDeltaTime))
			Infract(pPlayer, "speed hack");
		TrackReactionTime(pPlayer);
		if (IsReactionTimeAnomaly(pPlayer))
			Infract(pPlayer, "reaction time anomaly");
		if (IsAntiAiming(pPlayer))
			Infract(pPlayer, "anti-aim");
	}
}

void CCheaterDetection::Reset()
{
	mData.clear();
}

void CCheaterDetection::ReportChoke(CTFPlayer* pEntity, int iChoke)
{
	if (Vars::CheaterDetection::Methods.Value & Vars::CheaterDetection::MethodsEnum::PacketChoking)
	{
		mData[pEntity].m_PacketChoking.m_vChokes.push_back(iChoke);
		if (mData[pEntity].m_PacketChoking.m_vChokes.size() == 3)
		{
			mData[pEntity].m_PacketChoking.m_bInfract = true; // check for last 3 choke amounts
			for (auto& iChoke : mData[pEntity].m_PacketChoking.m_vChokes)
			{
				if (iChoke < Vars::CheaterDetection::MinimumChoking.Value)
					mData[pEntity].m_PacketChoking.m_bInfract = false;
			}
			mData[pEntity].m_PacketChoking.m_vChokes.clear();
		}
	}
	else
		mData[pEntity].m_PacketChoking.m_vChokes.clear();
}

void CCheaterDetection::ReportDamage(IGameEvent* pEvent)
{
	const bool bAimFlicking = Vars::CheaterDetection::Methods.Value & Vars::CheaterDetection::MethodsEnum::AimFlicking;
	const bool bCritTracking = Vars::CheaterDetection::Methods.Value & Vars::CheaterDetection::MethodsEnum::CritManipulation;
	const bool bTriggerBot = Vars::CheaterDetection::Methods.Value & Vars::CheaterDetection::MethodsEnum::TriggerBot;
	const bool bHitboxAbuse = Vars::CheaterDetection::Methods.Value & Vars::CheaterDetection::MethodsEnum::HitboxAbuse;
	const bool bReactionTime = Vars::CheaterDetection::Methods.Value & Vars::CheaterDetection::MethodsEnum::ReactionTime;
	if (!bAimFlicking && !bCritTracking && !bTriggerBot && !bHitboxAbuse && !bReactionTime)
		return;

	int iIndex = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("attacker"));
	if (iIndex == I::EngineClient->GetLocalPlayer())
		return;

	auto pEntity = I::ClientEntityList->GetClientEntity(iIndex)->As<CTFPlayer>();
	if (!pEntity || !pEntity->IsPlayer() || pEntity->IsDormant())
		return;

	auto pWeapon = pEntity->m_hActiveWeapon()->As<CTFWeaponBase>();
	switch (SDK::GetWeaponType(pWeapon))
	{
	case EWeaponType::UNKNOWN:
	case EWeaponType::PROJECTILE:
		return;
	}

	if (bAimFlicking)
	{
		auto& vAngles = mData[pEntity].m_AimFlicking.m_vAngles;
		if (!vAngles.empty())
			vAngles.back().m_bAttacking = true;
	}

	if (bCritTracking)
		TrackCritEvent(pEntity, pWeapon, pEvent->GetBool("crit"));

	if (bTriggerBot)
	{
		const int nWeaponID = pWeapon->GetWeaponID();
		if (nWeaponID == TF_WEAPON_SNIPERRIFLE || nWeaponID == TF_WEAPON_SNIPERRIFLE_DECAP || nWeaponID == TF_WEAPON_SNIPERRIFLE_CLASSIC)
		{
			const int iVictimIndex = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("userid"));
			auto& tTriggerBot = mData[pEntity].m_TriggerBot;
			auto it = tTriggerBot.m_mFirstAimTime.find(iVictimIndex);
			if (it != tTriggerBot.m_mFirstAimTime.end())
			{
				const float flThreshold = Vars::CheaterDetection::TriggerBotMaxReactionTime.Value / 1000.f;
				const float flReactionTime = I::GlobalVars->curtime - it->second;
				if (flReactionTime >= 0.f && flReactionTime < flThreshold)
					tTriggerBot.m_bInfract = true;
				tTriggerBot.m_mFirstAimTime.erase(it);
			}
		}
	}

	if (bHitboxAbuse)
	{
		// Only track headshot ratio for hitscan weapons — melee hits in TF2 report
		// a hitgroup but headshots carry no special gameplay meaning for melee,
		// so counting them would inflate the ratio and produce false positives.
		if (SDK::GetWeaponType(pWeapon) == EWeaponType::HITSCAN)
		{
			const int iHitGroup = pEvent->GetInt("hitgroup");
			const bool bIsHeadshot = (iHitGroup == HITGROUP_HEAD);
			auto& tHitboxAbuse = mData[pEntity].m_HitboxAbuse;

			const float flNow = I::GlobalVars->curtime;
			const float flWindow = Vars::CheaterDetection::HitboxAbuseWindow.Value;

			// Evict stale events outside the rolling window
			while (!tHitboxAbuse.m_vHits.empty() && flNow - tHitboxAbuse.m_vHits.front().m_flTime > flWindow)
				tHitboxAbuse.m_vHits.pop_front();

			tHitboxAbuse.m_vHits.push_back({ flNow, bIsHeadshot });

			const int iSampleSize = Vars::CheaterDetection::HitboxAbuseSampleSize.Value;
			if (iSampleSize >= 1 && (int)tHitboxAbuse.m_vHits.size() >= iSampleSize)
			{
				int iHeadshots = 0;
				for (auto& tHit : tHitboxAbuse.m_vHits)
				{
					if (tHit.m_bHeadshot)
						iHeadshots++;
				}

				const float flRatio = (float(iHeadshots) / float(tHitboxAbuse.m_vHits.size())) * 100.f;
				if (flRatio >= Vars::CheaterDetection::HitboxAbuseThreshold.Value)
				{
					tHitboxAbuse.m_vHits.clear();
					tHitboxAbuse.m_bInfract = true;
				}
			}
		}
	}

	if (bReactionTime)
	{
		const int iVictimIndex = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("userid"));
		auto& tReactionTime = mData[pEntity].m_ReactionTime;
		auto it = tReactionTime.m_mFirstVisibleTime.find(iVictimIndex);
		if (it != tReactionTime.m_mFirstVisibleTime.end())
		{
			const float flThreshold = Vars::CheaterDetection::ReactionTimeThreshold.Value / 1000.f;
			const float flReactionTime = I::GlobalVars->curtime - it->second;
			if (flReactionTime >= 0.f && flReactionTime < flThreshold)
				tReactionTime.m_bInfract = true;
			tReactionTime.m_mFirstVisibleTime.erase(it);
		}
	}
}