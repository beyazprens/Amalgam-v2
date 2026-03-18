#pragma once
#include "../../../SDK/SDK.h"
#include "../../../Utils/Math/Math.h"
#include <cmath>
#include <algorithm>

// AimSmoothing - advanced smoothing algorithms for aim assistance.
//
// Two modes are provided:
//   apply_curve_smooth  - cubic-curve non-linear smoothing; produces natural-feeling
//                         aim movement that accelerates when far from target and
//                         decelerates as it approaches.
//   apply_remap_smooth  - linear smoothing with better granularity via parameter
//                         remapping from the [1, 100] UI range to [1.5, 30].
namespace AimSmoothing
{
	// Clamp a delta angle into the [-180, 180] range.
	inline float ClampDelta(float flDelta)
	{
		while (flDelta > 180.f)  flDelta -= 360.f;
		while (flDelta < -180.f) flDelta += 360.f;
		return flDelta;
	}

	// Cubic curve smoothing.
	// The smooth factor is scaled by (1 + |delta_norm|^3) so that larger
	// deltas converge faster, giving a natural curve instead of constant lag.
	//
	// Parameters:
	//   pCmd         - current user command whose view angles are modified in place.
	//   vTargetAngle - the desired final view angle.
	//   flSmooth     - smoothing intensity in [1, 100]; higher = more smoothing.
	//   flMaxSmooth  - maximum denominator (controls the slowest movement speed).
	inline void apply_curve_smooth(CUserCmd* pCmd, const Vec3& vTargetAngle,
	                               float flSmooth, float flMaxSmooth = 100.f)
	{
		if (!pCmd)
			return;

		flSmooth = std::clamp(flSmooth, 1.f, flMaxSmooth);

		const float flPitchDelta = ClampDelta(vTargetAngle.x - pCmd->m_vViewAngles.x);
		const float flYawDelta   = ClampDelta(vTargetAngle.y - pCmd->m_vViewAngles.y);

		// Normalise deltas by 180 degrees to get a [0, 1] magnitude
		const float flPitchNorm = std::min(std::abs(flPitchDelta) / 180.f, 1.f);
		const float flYawNorm   = std::min(std::abs(flYawDelta)   / 180.f, 1.f);

		// Cubic power curve: smooth denominator grows with delta magnitude
		const float flPitchSmooth = flSmooth * (1.f + std::powf(flPitchNorm, 3.f));
		const float flYawSmooth   = flSmooth * (1.f + std::powf(flYawNorm,   3.f));

		pCmd->m_vViewAngles.x += flPitchDelta / flPitchSmooth;
		pCmd->m_vViewAngles.y += flYawDelta   / flYawSmooth;
	}

	// Linear smoothing with remapped parameter range.
	// Remaps smooth_factor from [1, 100] to [1.5, 30] so that the full
	// slider range has meaningful, evenly-spaced granularity.
	//
	// Parameters: same as apply_curve_smooth.
	inline void apply_remap_smooth(CUserCmd* pCmd, const Vec3& vTargetAngle,
	                               float flSmooth, float flMaxSmooth = 100.f)
	{
		if (!pCmd)
			return;

		// Remap [1, max] -> [1.5, 30]
		const float flMapped = Math::RemapVal(
			std::clamp(flSmooth, 1.f, flMaxSmooth),
			1.f, flMaxSmooth,
			1.5f, 30.f
		);

		const float flPitchDelta = ClampDelta(vTargetAngle.x - pCmd->m_vViewAngles.x);
		const float flYawDelta   = ClampDelta(vTargetAngle.y - pCmd->m_vViewAngles.y);

		pCmd->m_vViewAngles.x += flPitchDelta / flMapped;
		pCmd->m_vViewAngles.y += flYawDelta   / flMapped;
	}
}
