#include "Commands.h"

#include "../../Core/Core.h"
#include "../ImGui/Menu/Menu.h"
#include "../NavBot/NavEngine/NavEngine.h"
#include "../Configs/Configs.h"
#include "../Players/PlayerUtils.h"
#include "../Misc/AutoItem/AutoItem.h"
#include "../Misc/Misc.h"
#include <utility>
#include <boost/algorithm/string/replace.hpp>

#define AddCommand(sCommand, fCommand) \
{ \
	FNV1A::Hash32Const(sCommand), \
	[](const std::deque<const char*>& vArgs) \
		fCommand \
},

static std::unordered_map<uint32_t, CommandCallback> s_mCommands = {
	AddCommand("cat_setcvar",
	{
		if (vArgs.size() < 2)
		{
			SDK::Output("Usage:\n\tcat_setcvar <cvar> <value>");
			return;
		}

		const char* sCVar = vArgs[0];
		auto pCVar = I::CVar->FindVar(sCVar);
		if (!pCVar)
		{
			SDK::Output(std::format("Could not find {}", sCVar).c_str());
			return;
		}

		std::string sValue = "";
		for (int i = 1; i < vArgs.size(); i++)
			sValue += std::format("{} ", vArgs[i]);
		sValue.pop_back();
		boost::replace_all(sValue, "\"", "");

		pCVar->SetValue(sValue.c_str());
		SDK::Output(std::format("Set {} to {}", sCVar, sValue).c_str());
	})
	AddCommand("cat_getcvar",
	{
		if (vArgs.size() != 1)
		{
			SDK::Output("Usage:\n\tcat_getcvar <cvar>");
			return;
		}

		const char* sCVar = vArgs[0];
		auto pCVar = I::CVar->FindVar(sCVar);
		if (!pCVar)
		{
			SDK::Output(std::format("Could not find {}", sCVar).c_str());
			return;
		}

		SDK::Output(std::format("Value of {} is {}", sCVar, pCVar->GetString()).c_str());
	})
	AddCommand("cat_queue",
	{
		if (!I::TFPartyClient)
			return;
		if (I::TFPartyClient->BInQueueForMatchGroup(k_eTFMatchGroup_Casual_Default))
			return;
		if (I::EngineClient->IsDrawingLoadingImage())
			return;

		static bool bHasLoaded = false;
		if (!bHasLoaded)
		{
			I::TFPartyClient->LoadSavedCasualCriteria();
			bHasLoaded = true;
		}
		I::TFPartyClient->RequestQueueForMatch(k_eTFMatchGroup_Casual_Default);
	})
	AddCommand("cat_criteria",
	{
		if (!I::TFPartyClient)
		{
			SDK::Output("TFPartyClient interface unavailable");
			return;
		}

		I::TFPartyClient->LoadSavedCasualCriteria();
		SDK::Output("Loaded saved casual criteria.");
	})
	AddCommand("cat_abandon",
	{
		if (!I::TFGCClientSystem)
		{
			SDK::Output("TFGCClientSystem interface unavailable");
			return;
		}
		
		I::TFGCClientSystem->AbandonCurrentMatch();
		SDK::Output("Requested match abandon.");
	})
	AddCommand("cat_load", 
	{
		if (vArgs.size() != 1)
		{
			SDK::Output("Usage:\n\tcat_load <config_name>");
			return;
		}
		F::Configs.LoadConfig(vArgs[0], true);
	})
	AddCommand("cat_path_to", 
	{
		// Check if the user provided at least 3 args
		if (vArgs.size() < 3)
		{
			SDK::Output("Usage:\n\tcat_path_to <x> <y> <z>");
			return;
		}

		Vector vDest;
		try
		{	
			// Get the Vec3
			vDest = Vec3(atoi(vArgs[0]), atoi(vArgs[1]), atoi(vArgs[2]));
		}
		catch (...)
		{
			SDK::Output("Usage:\n\tcat_path_to <x> <y> <z>");
			return;
		}

		auto pLocal = H::Entities.GetLocal();
		if (!pLocal || !pLocal->IsAlive())
		{
			SDK::Output("cat_path_to", "Local player unavailable");
			return;
		}

		F::NavEngine.GetLocalNavArea(pLocal->GetAbsOrigin());
		F::NavEngine.NavTo(vDest);
	})
	AddCommand("cat_cancel_path",
	{
		F::NavEngine.CancelPath();
	})
	AddCommand("cat_save_nav_mesh", 
	{
		if (auto pNavFile = F::NavEngine.GetNavFile())
			pNavFile->Write();
	})
	AddCommand("cat_clearchat", 
	{
		I::ClientModeShared->m_pChatElement->SetText("");
	})
	AddCommand("cat_menu", 
	{
		I::MatSystemSurface->SetCursorAlwaysVisible(F::Menu.m_bIsOpen = !F::Menu.m_bIsOpen);
	})
	AddCommand("cat_unload", 
	{
		if (F::Menu.m_bIsOpen)
			I::MatSystemSurface->SetCursorAlwaysVisible(F::Menu.m_bIsOpen = false);
		U::Core.m_bUnload = G::Unload = true;
	})
	AddCommand("cat_ignore", 
	{
		if (vArgs.size() < 2)
		{
			SDK::Output("Usage:\n\tcat_ignore <id32> <tag|rage>");
			return;
		}
		
		uint32_t uFriendsID = 0;
		try
		{
			uFriendsID = std::stoul(vArgs[0]);
		}
		catch (...)
		{
			SDK::Output("Invalid ID32 format");
			return;
		}

		if (!uFriendsID)
		{
			SDK::Output("Invalid ID32");
			return;
		}

		std::string sTag = vArgs[1];
		std::string sTagLower = sTag;
		std::transform(sTagLower.begin(), sTagLower.end(), sTagLower.begin(), ::tolower);

		int iTagID = FNV1A::Hash32(sTagLower.c_str()) == FNV1A::Hash32Const("rage")
			? F::PlayerUtils.TagToIndex(CHEATER_TAG)
			: F::PlayerUtils.GetTag(sTag);
		if (iTagID == -1)
		{
			SDK::Output(std::format("Invalid tag: {}", sTag).c_str());
			return;
		}

		auto pTag = F::PlayerUtils.GetTag(iTagID);
		if (!pTag || !pTag->m_bAssignable)
		{
			SDK::Output(std::format("Tag {} is not assignable", sTag).c_str());
			return;
		}
		const char* sTagName = pTag->m_sName.c_str();

		if (F::PlayerUtils.HasTag(uFriendsID, iTagID))
		{
			SDK::Output(std::format("ID32 {} already has tag {}", uFriendsID, sTagName).c_str());
			return;
		}

		F::PlayerUtils.AddTag(uFriendsID, iTagID, true);
		SDK::Output(std::format("Added tag {} to ID32 {}", sTagName, uFriendsID).c_str());
	})
	AddCommand("cat_dump",
	{
		F::Misc.DumpProfiles();
	})
	AddCommand("cat_crash", 
	{	// if you want to time out of a server and rejoin
		switch (vArgs.empty() ? 0 : FNV1A::Hash32(vArgs.front()))
		{
		case FNV1A::Hash32Const("true"):
		case FNV1A::Hash32Const("t"):
		case FNV1A::Hash32Const("1"):
			break;
		default:
			Vars::Debug::CrashLogging.Value = false; // we are voluntarily crashing, don't give out log if we don't want one
		}
		reinterpret_cast<void(*)()>(0)();
	})
	AddCommand("cat_rent_item", 
	{	
		if (vArgs.size() != 1)
		{
			SDK::Output("Usage:\n\tcat_rent_item <item_def_index>");
			return;
		}

		item_definition_index_t iDefIdx;
		try
		{
			iDefIdx = atoi(vArgs[0]);
		}
		catch (const std::invalid_argument&)
		{
			SDK::Output("Invalid item_def_index");
			return;
		}

		F::AutoItem.Rent(iDefIdx);
	})
	AddCommand("cat_achievement_unlock", 
	{
		if (vArgs.size() > 1)
		{
			SDK::Output("Usage:\n\tcat_achievement_unlock [all|item|weapon]");
			return;
		}

		const uint32_t uMode = vArgs.empty() ? FNV1A::Hash32Const("all") : FNV1A::Hash32(vArgs[0]);
		switch (uMode)
		{
		case FNV1A::Hash32Const("all"):
		case FNV1A::Hash32Const("normal"):
			F::Misc.UnlockAchievements();
			break;
		case FNV1A::Hash32Const("item"):
		case FNV1A::Hash32Const("items"):
		case FNV1A::Hash32Const("weapon"):
		case FNV1A::Hash32Const("weapons"):
			F::Misc.UnlockItemAchievements();
			break;
		default:
			SDK::Output("Usage:\n\tcat_achievement_unlock [all|item|weapon]");
			break;
		}
	})
	AddCommand("cat_achievement_unlock_item",
	{
		F::Misc.UnlockItemAchievements();
	})
	AddCommand("cat_achievement_unlock_weapon",
	{
		F::Misc.UnlockItemAchievements();
	})
	AddCommand("cat_achievement_lock",
	{
		if (vArgs.size() > 1)
		{
			SDK::Output("Usage:\n\tcat_achievement_lock [all|item|weapon]");
			return;
		}

		const uint32_t uMode = vArgs.empty() ? FNV1A::Hash32Const("all") : FNV1A::Hash32(vArgs[0]);
		switch (uMode)
		{
		case FNV1A::Hash32Const("all"):
		case FNV1A::Hash32Const("normal"):
			F::Misc.LockAchievements();
			break;
		case FNV1A::Hash32Const("item"):
		case FNV1A::Hash32Const("items"):
		case FNV1A::Hash32Const("weapon"):
		case FNV1A::Hash32Const("weapons"):
			F::Misc.LockItemAchievements();
			break;
		default:
			SDK::Output("Usage:\n\tcat_achievement_lock [all|item|weapon]");
			break;
		}
	})
	AddCommand("cat_achievement_lock_item",
	{
		F::Misc.LockItemAchievements();
	})
	AddCommand("cat_achievement_lock_weapon",
	{
		F::Misc.LockItemAchievements();
	})
	AddCommand("cat_bhop_wait",
	{
		if (!I::GlobalVars)
		{
			SDK::Output("cat_bhop_wait", "GlobalVars unavailable", { 255, 100, 100, 255 });
			return;
		}

		auto pFpsMax = I::CVar->FindVar("fps_max");
		if (!pFpsMax)
		{
			SDK::Output("cat_bhop_wait", "Could not find fps_max cvar", { 255, 100, 100, 255 });
			return;
		}

		const float flFpsMax        = pFpsMax->GetFloat();
		const float flTickInterval  = I::GlobalVars->interval_per_tick;
		const float flTickRate      = flTickInterval > 0.f ? (1.f / flTickInterval) : 0.f;

		if (flFpsMax <= 0.f || flTickInterval <= 0.f)
		{
			SDK::Output("cat_bhop_wait", "Invalid fps_max or tick interval (connect to a server first)", { 255, 200, 100, 255 });
			return;
		}

		// Perfect wait = ceil(fps_max / tickrate)
		// The Source Engine 'wait' command only accepts integers (it uses Q_atoi internally).
		// We need N frames such that N * frame_time >= tick_time, i.e. N >= fps_max / tickrate.
		// ceil() guarantees the wait is never shorter than one server tick period, which is
		// the minimum needed to avoid sending +jump faster than the server processes ticks.
		// Example: fps_max 101 / 67 tick = 1.508 -> ceil = 2 (matches the empirically correct
		// "wait 2.4" value; both truncate to wait 2 in the engine).
		const float flFramesPerTick = flFpsMax * flTickInterval;
		const int   iEffectiveWait  = static_cast<int>(std::ceil(flFramesPerTick));

		SDK::Output("cat_bhop_wait", std::format(
			"fps_max: {:.0f} | tickrate: {:.0f} | frames per tick: {:.4f} -> perfect wait: {}\n"
			"  alias bhop_jump \"+jump; wait {}; -jump; wait {}; bhop_jump\"",
			flFpsMax, flTickRate, flFramesPerTick, iEffectiveWait, iEffectiveWait, iEffectiveWait
		).c_str(), { 100, 220, 255, 255 });
	})
};

bool CCommands::Run(const char* sCmd, std::deque<const char*>& vArgs)
{
	std::string sLower = sCmd;
	std::transform(sLower.begin(), sLower.end(), sLower.begin(), ::tolower);

	auto uHash = FNV1A::Hash32(sLower.c_str());
	if (!s_mCommands.contains(uHash))
		return false;

	s_mCommands[uHash](vArgs);
	return true;
}
