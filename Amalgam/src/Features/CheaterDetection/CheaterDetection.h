#pragma once
#include "../../SDK/SDK.h"

struct AngleHistory_t
{
	Vec3 m_vAngle;
	bool m_bAttacking;
};

struct PlayerInfo
{
	uint32_t m_uAccountID = 0;
	const char* m_sName = "";

	int m_iDetections = 0;

	struct PacketChoking_t
	{
		std::deque<int> m_vChokes = {}; // store last 3 choke counts
		bool m_bInfract = false; // infract the user for choking?

		struct LagCompAbuse_t
		{
			std::deque<int> m_vBurstTicks = {}; // tickcounts of recent multi-cmd bursts
			std::deque<int> m_vDeltaCmds = {}; // delta cmd counts for reference
			bool m_bInfract = false;
			bool m_bWasDormant = false; // track dormancy transitions to avoid false positives
		} m_LagComp;
	} m_PacketChoking;

	struct AimFlicking_t
	{
		std::deque<AngleHistory_t> m_vAngles = {}; // store last 3 angles & if damage was dealt
	} m_AimFlicking;
					
	struct DuckSpeed_t
	{
		int m_iStartTick = 0;
	} m_DuckSpeed;

	struct CritTracker_t
	{
		struct WeaponHistory_t
		{
			std::deque<bool> m_vHistory = {};
			int m_iCrits = 0;
		};

		std::unordered_map<int, WeaponHistory_t> m_mWeaponHistory = {};
		bool m_bInfract = false;
	} m_CritTracker;

	struct TriggerBot_t
	{
		std::unordered_map<int, float> m_mFirstAimTime = {}; // entity index -> curtime when attacker first aimed at their head
		bool m_bInfract = false;
	} m_TriggerBot;

	struct HitboxAbuse_t
	{
		struct HitEvent_t
		{
			float m_flTime;
			bool m_bHeadshot;
		};
		std::deque<HitEvent_t> m_vHits = {}; // rolling window of hit events
		bool m_bInfract = false;
	} m_HitboxAbuse;

	struct SpeedHack_t
	{
		Vec3 m_vLastPos = {};
		float m_flLastTime = 0.f;
		bool m_bHasLastPos = false;
		int m_iConsecutiveViolations = 0;
	} m_SpeedHack;

	struct ReactionTime_t
	{
		std::unordered_map<int, float> m_mFirstVisibleTime = {}; // entity index -> curtime when attacker first had LOS to victim
		bool m_bInfract = false;
	} m_ReactionTime;

	struct AntiAim_t
	{
		float m_flLastYaw = 0.f;
		bool m_bHasLastYaw = false;
		int m_iConsecutiveTicks = 0;
	} m_AntiAim;
};

class CCheaterDetection
{
private:
	bool ShouldScan();

	bool InvalidPitch(CTFPlayer* pEntity);
	bool IsChoking(CTFPlayer* pEntity);
	bool IsFlicking(CTFPlayer* pEntity);
	bool IsDuckSpeed(CTFPlayer* pEntity);
	bool IsLagCompAbusing(CTFPlayer* pEntity, int iDeltaTicks);
	bool IsCritManipulating(CTFPlayer* pEntity);
	void TrackCritEvent(CTFPlayer* pEntity, CTFWeaponBase* pWeapon, bool bCrit);

	bool IsTriggerBot(CTFPlayer* pEntity);
	void TrackTriggerBot(CTFPlayer* pEntity);

	bool IsHitboxAbusing(CTFPlayer* pEntity);
	bool IsSpeedHacking(CTFPlayer* pEntity, float flDeltaTime);
	void TrackReactionTime(CTFPlayer* pEntity);
	bool IsReactionTimeAnomaly(CTFPlayer* pEntity);
	bool IsAntiAiming(CTFPlayer* pEntity);

	void Infract(CTFPlayer* pEntity, const char* sReason);

	std::unordered_map<CTFPlayer*, PlayerInfo> mData = {};

public:
	void Run();

	void ReportChoke(CTFPlayer* pEntity, int iChoke);
	void ReportDamage(IGameEvent* pEvent);
	void Reset();
};

ADD_FEATURE(CCheaterDetection, CheaterDetection);