#pragma once
#include "../../SDK/SDK.h"
#include "../../Utils/Hash/FNV1A.h"
#include "../../Utils/Lifecycle/Lifecycle.h"
#include "../../Utils/Macros/Macros.h"
#include <string>
#include <vector>
#include <unordered_map>

// PlayerTag - a named, coloured label that can be assigned to players.
// The priority field controls aimbot scanning order:
//   priority < 0  : ignored (skip in aimbot)
//   priority == 0 : no effect on scan order
//   priority > 0  : scanned first; higher value = higher urgency
struct PlayerTag
{
	PlayerTag() = default;

	PlayerTag(std::string_view name, const Color_t& color, int priority = 0)
		: name_hash(FNV1A::Hash32(name.data()))
		, name(name)
		, color(color)
		, priority(priority)
	{
	}

	uint32_t    name_hash{};
	std::string name{};
	Color_t     color{};
	int         priority{};

	bool operator==(const PlayerTag& other) const
	{
		return name_hash == other.name_hash;
	}
};

// PlayerList - manages a registry of PlayerTag definitions and
// maps Steam account IDs to tags.  Implements HasLoad so it can
// participate in the centralized initialization pipeline.
class PlayerList : public HasLoad
{
private:
	std::vector<PlayerTag>                  m_tags{};
	std::unordered_map<uint32_t, uint32_t>  m_player_tags{};  // steamid -> tag name_hash

public:
	// --- Tag registry ---

	// Returns a pointer to the tag with the given display name, or nullptr.
	PlayerTag* find_tag_by_name(const std::string& name)
	{
		for (auto& tag : m_tags)
		{
			if (tag.name == name)
				return &tag;
		}
		return nullptr;
	}

	// Returns a pointer to the tag with the given hash, or nullptr.
	PlayerTag* find_tag_by_hash(uint32_t hash)
	{
		for (auto& tag : m_tags)
		{
			if (tag.name_hash == hash)
				return &tag;
		}
		return nullptr;
	}

	void add_tag(const PlayerTag& tag)
	{
		if (!find_tag_by_hash(tag.name_hash))
			m_tags.push_back(tag);
	}

	void remove_tag(const PlayerTag& tag)
	{
		m_tags.erase(
			std::remove_if(m_tags.begin(), m_tags.end(),
				[&](const PlayerTag& t) { return t == tag; }),
			m_tags.end()
		);
	}

	// --- Player <-> tag assignment ---

	// Assign a tag (by hash) to a player (by Steam account ID).
	void set_player_tag(uint32_t steamid, uint32_t tag_hash)
	{
		m_player_tags[steamid] = tag_hash;
	}

	// Returns the tag assigned to steamid, or nullptr if none.
	PlayerTag* get_player_tag(uint32_t steamid)
	{
		auto it = m_player_tags.find(steamid);
		if (it == m_player_tags.end())
			return nullptr;
		return find_tag_by_hash(it->second);
	}

	// --- Lifecycle ---

	// Registers built-in default tags.
	bool Load() override
	{
		m_tags = {
			{ "Default", { 200, 200, 200, 255 }, 0  },
			{ "Ignored", { 200, 200, 200, 255 }, -1 },
			{ "Cheater", { 255, 100, 100, 255 }, 1  },
			{ "Friend",  { 100, 255, 100, 255 }, 0  },
			{ "Party",   { 100, 100, 255, 255 }, 0  },
		};
		return true;
	}

	// --- Aimbot helpers ---

	// Returns all players currently in the server sorted by tag priority
	// (highest first).  Players with no tag or priority == 0 appear after
	// prioritised ones; ignored players (priority < 0) appear last.
	std::vector<CTFPlayer*> get_players_sorted_by_priority()
	{
		std::vector<std::pair<int, CTFPlayer*>> candidates;

		for (int i = 1; i <= I::EngineClient->GetMaxClients(); ++i)
		{
			auto* pEntity = I::ClientEntityList->GetClientEntity(i);
			if (!pEntity)
				continue;

			auto* pPlayer = reinterpret_cast<CTFPlayer*>(pEntity);
			if (!pPlayer->IsAlive())
				continue;

			const auto pInfo = pPlayer->GetPlayerInfo();
			const int priority = [&]() -> int
			{
				auto* pTag = get_player_tag(pInfo.m_nSteamID);
				return pTag ? pTag->priority : 0;
			}();

			candidates.emplace_back(priority, pPlayer);
		}

		std::stable_sort(candidates.begin(), candidates.end(),
			[](const auto& a, const auto& b) { return a.first > b.first; });

		std::vector<CTFPlayer*> result;
		result.reserve(candidates.size());
		for (auto& [prio, pPlayer] : candidates)
			result.push_back(pPlayer);

		return result;
	}
};

MAKE_UNIQUE(PlayerList, player_list);
