#pragma once
#include "../../SDK/SDK.h"
#include "../../Features/Backtrack/Backtrack.h"
#include "../../Utils/Math/Math.h"
#include <memory>
#include <deque>

// InterpHelper - a lightweight snapshot of a TickRecord's positional fields.
// Separating the fields into a value type makes interpolation arithmetic cleaner
// and avoids accidentally mutating the source record.
struct InterpHelper
{
	Vec3  origin{};
	Vec3  mins{};
	Vec3  maxs{};
	Vec3  velocity{};
	float sim_time{};

	InterpHelper() = default;

	explicit InterpHelper(const TickRecord* pRecord)
	{
		if (!pRecord)
			return;

		origin   = pRecord->m_vOrigin;
		mins     = pRecord->m_vMins;
		maxs     = pRecord->m_vMaxs;
		sim_time = pRecord->m_flSimTime;

		// Velocity is not stored directly in TickRecord; initialise to zero.
		// Callers may compute it as (origin[n] - origin[n-1]) / tick_interval.
		velocity = {};
	}
};

// add_interpolated_records - fills in missing intermediate ticks between the two
// most recent records in 'records', based on the simulation-time delta.
//
// Call this after adding a new TickRecord so that the deque has tick-perfect
// coverage even when the server skips ticks (e.g. during choke / lag).
//
// Parameters:
//   records - a deque ordered newest-first (records[0] is the latest tick).
//   pPlayer - the owning player; used to create new TickRecord shells.
inline void add_interpolated_records(std::deque<TickRecord>& records, CTFPlayer* pPlayer)
{
	if (records.size() < 2 || !pPlayer)
		return;

	const TickRecord* pFirst  = &records[0];
	const TickRecord* pSecond = &records[1];

	const float flTimeDelta = pFirst->m_flSimTime - pSecond->m_flSimTime;
	const int   nTickDelta  = static_cast<int>(flTimeDelta / I::GlobalVars->interval_per_tick + 0.5f);

	if (nTickDelta < 2)
		return;  // Records are already consecutive - nothing to interpolate

	const InterpHelper hFirst{ pFirst };
	const InterpHelper hSecond{ pSecond };

	// Insert (nTickDelta - 1) interpolated records between index 0 and 1.
	for (int n = 1; n < nTickDelta; ++n)
	{
		const float frac = static_cast<float>(n) / static_cast<float>(nTickDelta);

		TickRecord rec;
		rec.m_flSimTime = Math::Lerp(hSecond.sim_time,  hFirst.sim_time,  frac);
		rec.m_vOrigin   = Vec3(
			Math::Lerp(hSecond.origin.x, hFirst.origin.x, frac),
			Math::Lerp(hSecond.origin.y, hFirst.origin.y, frac),
			Math::Lerp(hSecond.origin.z, hFirst.origin.z, frac)
		);
		rec.m_vMins  = Vec3(
			Math::Lerp(hSecond.mins.x, hFirst.mins.x, frac),
			Math::Lerp(hSecond.mins.y, hFirst.mins.y, frac),
			Math::Lerp(hSecond.mins.z, hFirst.mins.z, frac)
		);
		rec.m_vMaxs  = Vec3(
			Math::Lerp(hSecond.maxs.x, hFirst.maxs.x, frac),
			Math::Lerp(hSecond.maxs.y, hFirst.maxs.y, frac),
			Math::Lerp(hSecond.maxs.z, hFirst.maxs.z, frac)
		);
		rec.m_bInvalid = false;

		// Bone data: carry forward from the newer record as an approximation.
		std::memcpy(rec.m_aBones, pFirst->m_aBones, sizeof(rec.m_aBones));

		records.insert(records.begin() + n, std::move(rec));
	}
}
