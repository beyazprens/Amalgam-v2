#pragma once
#include "../../../SDK/SDK.h"
#include "../AimbotGlobal/AimbotGlobal.h"

// MeleeTarget - extended target record used during swing prediction.
struct MeleeTarget
{
	CBaseEntity*     ent{};
	Vec3             ang_to{};
	Vec3             pos{};
	float            fov_to{};
	float            dist_to{};
	const TickRecord* lag_record{};
	bool             can_swing_pred     = false;
	bool             was_swing_predicted = false;
};

// SwingPrediction - predicts whether a melee weapon will connect on the
// next swing tick by simulating forward movement of both the local player
// and the target over the weapon's attack interval.
//
// Integration:
//   1. Call update_local_positions() at the start of CreateMove to cache the
//      per-tick local-player origin predictions.
//   2. Call predict_swing() for each candidate MeleeTarget; if it returns true,
//      the swing should fire this tick.
class SwingPrediction
{
private:
	// Per-tick predicted local player positions (filled by update_local_positions).
	std::vector<Vec3> m_local_positions{};

	static constexpr int   kLookAheadTicks = 16;
	static constexpr float kMeleeRange     = 70.f;  // TF2 default melee range

public:
	// Returns the number of ticks until the weapon can swing again.
	// bVar controls whether to include the secondary attack interval.
	float get_swing_time_delta(CTFWeaponBase* pWeapon)
	{
		if (!pWeapon)
			return 0.f;

		const float flNextAttack  = pWeapon->m_flNextPrimaryAttack();
		const float flServerTime  = I::GlobalVars->curtime;
		const float flTickInterval = I::GlobalVars->interval_per_tick;

		if (flNextAttack <= flServerTime)
			return 0.f;

		return std::ceilf((flNextAttack - flServerTime) / flTickInterval) * flTickInterval;
	}

	// Cache forward-simulated positions for the local player over the next
	// kLookAheadTicks ticks so that predict_swing() can reuse them cheaply.
	void update_local_positions(CTFPlayer* pLocal)
	{
		m_local_positions.clear();

		if (!pLocal)
			return;

		m_local_positions.reserve(kLookAheadTicks);

		const float flTickInterval = I::GlobalVars->interval_per_tick;
		Vec3 vPos   = pLocal->GetAbsOrigin();
		Vec3 vVel   = pLocal->m_vecVelocity();

		for (int i = 0; i < kLookAheadTicks; ++i)
		{
			// Simple Euler integration - sufficient for melee range prediction
			vPos = vPos + vVel * flTickInterval;
			m_local_positions.push_back(vPos);
		}
	}

	// Returns true if the melee weapon is predicted to connect against tTarget
	// during the attack window.  Sets tTarget.can_swing_pred and
	// tTarget.was_swing_predicted accordingly.
	bool predict_swing(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, MeleeTarget& tTarget)
	{
		tTarget.can_swing_pred      = false;
		tTarget.was_swing_predicted = false;

		if (!pLocal || !pWeapon || !tTarget.ent)
			return false;

		const float flSwingDelta   = get_swing_time_delta(pWeapon);
		const float flTickInterval = I::GlobalVars->interval_per_tick;
		const int   nSwingTick     = static_cast<int>(flSwingDelta / flTickInterval);

		if (nSwingTick < 0 || nSwingTick >= kLookAheadTicks)
			return false;

		if (nSwingTick >= static_cast<int>(m_local_positions.size()))
			return false;

		// Predict target position at the swing tick using constant velocity.
		auto* pTargetPlayer = reinterpret_cast<CTFPlayer*>(tTarget.ent);
		const Vec3 vTargetVel = pTargetPlayer->m_vecVelocity();
		const Vec3 vTargetPos = tTarget.pos + vTargetVel * (flSwingDelta);

		const Vec3& vLocalPos = m_local_positions[nSwingTick];

		const Vec3  vDiff = vTargetPos - vLocalPos;
		const float flDist = std::sqrtf(vDiff.x * vDiff.x + vDiff.y * vDiff.y + vDiff.z * vDiff.z);

		tTarget.can_swing_pred      = true;
		tTarget.was_swing_predicted = (flDist <= kMeleeRange);

		return tTarget.was_swing_predicted;
	}
};

ADD_FEATURE(SwingPrediction, swing_prediction);
