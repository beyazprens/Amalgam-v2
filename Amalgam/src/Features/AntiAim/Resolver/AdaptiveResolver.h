#pragma once
#include "../../../SDK/SDK.h"
#include "../../../Utils/Lifecycle/Lifecycle.h"
#include <deque>
#include <unordered_map>
#include <algorithm>
#include <numeric>

// ResolverData - per-player state tracked by AdaptiveResolver.
struct AdaptiveResolverData
{
	std::deque<float> yaw_history;       // Rolling window of last 10 resolved yaw angles
	float  confidence    = 0.f;          // Hit-rate estimate in [0, 1]
	int    resolver_mode = 0;            // Active mode index (0 = none, 1 = invert, 2 = jitter)
	int    shots_fired   = 0;
	int    shots_hit     = 0;
	int    missed_shots  = 0;
	float  last_real_yaw = 0.f;
	float  last_fake_yaw = 0.f;
	int    mode_misses[3]{ 0, 0, 0 };   // Consecutive misses per mode
	int    ticks_since_shot = 0;

	static constexpr size_t kMaxHistory = 10;
	static constexpr float  kMissThreshold = 0.30f;  // <30 % hit rate triggers mode switch
	static constexpr int    kMinShots = 3;            // Minimum shots before adapting
};

// AdaptiveResolver - learns from hit/miss feedback and switches its
// yaw-resolution strategy when the current one performs poorly.
//
// Integration:
//   Call resolve() every CreateMove tick to get an adjusted yaw offset.
//   Call on_player_shot() from the bullet-impact / player-hurt event.
class AdaptiveResolver : public InstTracker<AdaptiveResolver>
{
private:
	std::unordered_map<int, AdaptiveResolverData> m_player_data;  // entindex -> data

	// Predict yaw based on the rolling history and the current mode.
	float predict_yaw(AdaptiveResolverData& data, CTFPlayer* pPlayer)
	{
		const float flBaseYaw = data.last_real_yaw;

		switch (data.resolver_mode)
		{
		case 1:
			// Invert: flip the delta between the sent and the stored yaw
			return flBaseYaw + (flBaseYaw - data.last_fake_yaw);

		case 2:
		{
			// Jitter: average the last few history entries
			if (data.yaw_history.size() >= 2)
			{
				float sum = 0.f;
				for (float y : data.yaw_history)
					sum += y;
				return sum / static_cast<float>(data.yaw_history.size());
			}
			return flBaseYaw;
		}

		default:
			return flBaseYaw;
		}
	}

	// Returns true if the player's velocity magnitude exceeds the walk threshold.
	bool is_player_moving(CTFPlayer* pPlayer, const Vec3& vVelocity) const
	{
		constexpr float kWalkSpeed = 5.f;
		return (vVelocity.x * vVelocity.x + vVelocity.y * vVelocity.y) > kWalkSpeed * kWalkSpeed;
	}

	// Push the player's current angles into the history ring buffer.
	void update_player_history(CTFPlayer* pPlayer, AdaptiveResolverData& data)
	{
		const float flCurrentYaw = pPlayer->m_angEyeAngles().y;
		data.last_real_yaw = flCurrentYaw;

		data.yaw_history.push_front(flCurrentYaw);
		if (data.yaw_history.size() > AdaptiveResolverData::kMaxHistory)
			data.yaw_history.pop_back();

		++data.ticks_since_shot;
	}

public:
	// Resolve yaw for pPlayer this tick.
	// Returns the predicted real yaw angle to aim at.
	float resolve(CTFPlayer* pPlayer)
	{
		if (!pPlayer)
			return 0.f;

		const int iIndex = pPlayer->GetIndex();
		auto& data = m_player_data[iIndex];

		update_player_history(pPlayer, data);
		update_resolver_mode(data);

		return predict_yaw(data, pPlayer);
	}

	// Notify the resolver that we shot at pPlayer and whether we hit.
	void on_player_shot(CTFPlayer* pPlayer, bool bHit)
	{
		if (!pPlayer)
			return;

		const int iIndex = pPlayer->GetIndex();
		auto& data = m_player_data[iIndex];

		++data.shots_fired;
		data.ticks_since_shot = 0;

		if (bHit)
		{
			++data.shots_hit;
			data.missed_shots = 0;
		}
		else
		{
			++data.missed_shots;
			if (data.resolver_mode >= 0 && data.resolver_mode < 3)
				++data.mode_misses[data.resolver_mode];
		}

		if (data.shots_fired > 0)
			data.confidence = static_cast<float>(data.shots_hit) / static_cast<float>(data.shots_fired);
	}

	// Switch resolver mode when the current mode's hit rate drops below the threshold.
	void update_resolver_mode(AdaptiveResolverData& data)
	{
		if (data.shots_fired < AdaptiveResolverData::kMinShots)
			return;

		if (data.confidence >= AdaptiveResolverData::kMissThreshold)
			return;

		// Find the mode with the fewest misses and switch to it.
		const int iBestMode = static_cast<int>(
			std::min_element(std::begin(data.mode_misses), std::end(data.mode_misses))
			- std::begin(data.mode_misses)
		);

		if (iBestMode != data.resolver_mode)
		{
			data.resolver_mode = iBestMode;
			data.missed_shots  = 0;
		}
	}

	// Clear all per-player state (call on level change / disconnect).
	void reset()
	{
		m_player_data.clear();
	}

	// Returns the raw data record for pPlayer, or nullptr if not found.
	AdaptiveResolverData* get_player_data(CTFPlayer* pPlayer)
	{
		if (!pPlayer)
			return nullptr;

		const int iIndex = pPlayer->GetIndex();
		auto it = m_player_data.find(iIndex);
		if (it == m_player_data.end())
			return nullptr;

		return &it->second;
	}
};

ADD_FEATURE(AdaptiveResolver, adaptive_resolver);
