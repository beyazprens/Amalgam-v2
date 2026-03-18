#include "NetVars.h"

#include "../../SDK/Definitions/Interfaces/CHLClient.h"
#include "../Hash/FNV1A.h"
#include <unordered_map>

#ifdef GetProp
	#undef GetProp
#endif

int CNetVars::GetOffset(RecvTable* pTable, const char* szNetVar)
{
	auto uHash = FNV1A::Hash32(szNetVar);
	for (int i = 0; i < pTable->GetNumProps(); i++)
	{
		RecvProp* pProp = pTable->GetProp(i);
		if (uHash == FNV1A::Hash32(pProp->m_pVarName))
			return pProp->GetOffset();

		if (auto pDataTable = pProp->GetDataTable())
		{
			if (auto nOffset = GetOffset(pDataTable, szNetVar))
				return nOffset + pProp->GetOffset();
		}
	}

	return 0;
}

int CNetVars::GetNetVar(const char* szClass, const char* szNetVar)
{
	auto uHash = FNV1A::Hash32(szClass);
	for (auto pCurrNode = I::Client->GetAllClasses(); pCurrNode; pCurrNode = pCurrNode->m_pNext)
	{
		if (uHash == FNV1A::Hash32(pCurrNode->m_pNetworkName))
			return GetOffset(pCurrNode->m_pRecvTable, szNetVar);
	}

	return 0;
}

// Hash-based offset lookup: accepts a precomputed FNV1A hash for the property name.
// Avoids repeated runtime hashing when called from NETVAR_H macros.
int CNetVars::GetOffsetByHash(RecvTable* pTable, uint32_t uVarHash)
{
	for (int i = 0; i < pTable->GetNumProps(); i++)
	{
		RecvProp* pProp = pTable->GetProp(i);
		if (uVarHash == FNV1A::Hash32(pProp->m_pVarName))
			return pProp->GetOffset();

		if (auto pDataTable = pProp->GetDataTable())
		{
			if (auto nOffset = GetOffsetByHash(pDataTable, uVarHash))
				return nOffset + pProp->GetOffset();
		}
	}

	return 0;
}

// Hash-based netvar lookup: accepts precomputed FNV1A hashes for both the class
// and property names. The hash values are computed at compile time via HASH_CT()
// in the NETVAR_H macro family, eliminating runtime string hashing on every
// first-call initialization.
//
// A static cache maps class-name hashes to their RecvTable so that repeated
// calls for the same class (e.g., 100 netvars on DT_TFPlayer) only search the
// full class list once.
int CNetVars::GetNetVarByHash(uint32_t uClassHash, uint32_t uVarHash)
{
	// Per-call-site cache for the RecvTable pointer.  Each NETVAR_H expansion
	// creates its own static nOffset, so GetNetVarByHash is called exactly once
	// per netvar site.  The class-level cache below further avoids re-scanning
	// the class list when multiple netvars share the same class.
	static std::unordered_map<uint32_t, RecvTable*> s_ClassCache;

	RecvTable* pTable = nullptr;
	auto it = s_ClassCache.find(uClassHash);
	if (it != s_ClassCache.end())
	{
		pTable = it->second;
	}
	else
	{
		for (auto pCurrNode = I::Client->GetAllClasses(); pCurrNode; pCurrNode = pCurrNode->m_pNext)
		{
			if (uClassHash == FNV1A::Hash32(pCurrNode->m_pNetworkName))
			{
				pTable = pCurrNode->m_pRecvTable;
				s_ClassCache.emplace(uClassHash, pTable);
				break;
			}
		}
	}

	return pTable ? GetOffsetByHash(pTable, uVarHash) : 0;
}

RecvProp* CNetVars::GetProp(RecvTable* pTable, const char* szNetVar)
{
	auto uHash = FNV1A::Hash32(szNetVar);
	for (int i = 0; i < pTable->GetNumProps(); i++)
	{
		RecvProp* pProp = pTable->GetProp(i);
		if (uHash == FNV1A::Hash32(pProp->m_pVarName))
			return pProp;

		if (auto pDataTable = pProp->GetDataTable())
		{
			if (pProp = GetProp(pDataTable, szNetVar))
				return pProp;
		}
	}

	return nullptr;
}

RecvProp* CNetVars::GetNetProp(const char* szClass, const char* szNetVar)
{
	auto uHash = FNV1A::Hash32(szClass);
	for (auto pCurrNode = I::Client->GetAllClasses(); pCurrNode; pCurrNode = pCurrNode->m_pNext)
	{
		if (uHash == FNV1A::Hash32(pCurrNode->m_pNetworkName))
			return GetProp(pCurrNode->m_pRecvTable, szNetVar);
	}

	return nullptr;
}