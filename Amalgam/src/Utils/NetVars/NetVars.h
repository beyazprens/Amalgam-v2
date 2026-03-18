#pragma once
#include "../Macros/Macros.h"
#include "../Hash/FNV1A.h"
#include "../../SDK/Definitions/Misc/dt_recv.h"

class CNetVars
{
public:
	int GetOffset(RecvTable* pTable, const char* szNetVar);
	int GetNetVar(const char* szClass, const char* szNetVar);

	// Hash-based lookup: accepts compile-time FNV1A hashes for class and property names.
	// Used by NETVAR_H macros to avoid runtime string hashing.
	int GetOffsetByHash(RecvTable* pTable, uint32_t uVarHash);
	int GetNetVarByHash(uint32_t uClassHash, uint32_t uVarHash);

	RecvProp* GetProp(RecvTable* pTable, const char* szNetVar);
	RecvProp* GetNetProp(const char* szClass, const char* szNetVar);
};

ADD_FEATURE_CUSTOM(CNetVars, NetVars, U);

// ---------------------------------------------------------------------------
// Legacy macros (4-param style, string-based lookup with lazy static caching)
// ---------------------------------------------------------------------------

#define NETVAR(_name, type, table, name) inline type& _name() \
{ \
	static int nOffset = U::NetVars.GetNetVar(table, name); \
	return *reinterpret_cast<type*>(uintptr_t(this) + nOffset); \
}

#define NETVAR_OFF(_name, type, table, name, offset) inline type& _name() \
{ \
	static int nOffset = U::NetVars.GetNetVar(table, name) + offset; \
	return *reinterpret_cast<type*>(uintptr_t(this) + nOffset); \
}

#define NETVAR_EMBED(_name, type, table, name) inline type _name() \
{ \
	static int nOffset = U::NetVars.GetNetVar(table, name); \
	return reinterpret_cast<type>(uintptr_t(this) + nOffset); \
}

#define NETVAR_OFF_EMBED(_name, type, table, name, offset) inline type _name() \
{ \
	static int nOffset = U::NetVars.GetNetVar(table, name) + offset; \
	return reinterpret_cast<type>(uintptr_t(this) + nOffset); \
}

#define NETVAR_ARRAY(_name, type, table, name) inline type& _name(int iIndex) \
{ \
	static int nOffset = U::NetVars.GetNetVar(table, name); \
	return *reinterpret_cast<type*>(uintptr_t(this) + nOffset + iIndex * sizeof(type)); \
}

#define NETVAR_ARRAY_OFF(_name, type, table, name, offset) inline type& _name(int iIndex) \
{ \
	static int nOffset = U::NetVars.GetNetVar(table, name) + offset; \
	return *reinterpret_cast<type*>(uintptr_t(this) + nOffset + iIndex * sizeof(type)); \
}

// ---------------------------------------------------------------------------
// Hash-based macros: use HASH_CT() to compute FNV1A hashes at compile time.
// These eliminate runtime string hashing while keeping lazy static caching.
// Prefer these for new code; existing code can be gradually migrated.
// ---------------------------------------------------------------------------

// NETVAR_H(func, type, table, name) - compile-time hash, explicit names
#define NETVAR_H(_name, type, table, name) inline type& _name() \
{ \
	static int nOffset = U::NetVars.GetNetVarByHash(HASH_CT(table), HASH_CT(name)); \
	return *reinterpret_cast<type*>(uintptr_t(this) + nOffset); \
}

// NETVAR_OFF_H - compile-time hash with additional byte offset
#define NETVAR_OFF_H(_name, type, table, name, offset) inline type& _name() \
{ \
	static int nOffset = U::NetVars.GetNetVarByHash(HASH_CT(table), HASH_CT(name)) + offset; \
	return *reinterpret_cast<type*>(uintptr_t(this) + nOffset); \
}

// NETVAR_EMBED_H - returns pointer (embedded struct) variant
#define NETVAR_EMBED_H(_name, type, table, name) inline type _name() \
{ \
	static int nOffset = U::NetVars.GetNetVarByHash(HASH_CT(table), HASH_CT(name)); \
	return reinterpret_cast<type>(uintptr_t(this) + nOffset); \
}

// NETVAR_ARRAY_H - array element accessor
#define NETVAR_ARRAY_H(_name, type, table, name) inline type& _name(int iIndex) \
{ \
	static int nOffset = U::NetVars.GetNetVarByHash(HASH_CT(table), HASH_CT(name)); \
	return *reinterpret_cast<type*>(uintptr_t(this) + nOffset + iIndex * sizeof(type)); \
}

// NETVAR_PTR - returns a typed pointer to the netvar field
#define NETVAR_PTR(_name, type, table, name) inline type* _name() \
{ \
	static int nOffset = U::NetVars.GetNetVarByHash(HASH_CT(table), HASH_CT(name)); \
	return reinterpret_cast<type*>(uintptr_t(this) + nOffset); \
}

// NETVAR_OFFSET - returns only the offset value (useful for manual pointer math)
#define NETVAR_OFFSET(table, name) ([]() -> int { \
	static int nOffset = U::NetVars.GetNetVarByHash(HASH_CT(table), HASH_CT(name)); \
	return nOffset; \
}())

// CLASSVAR - access a class member at a fixed compile-time offset (non-netvar members)
#define CLASSVAR(_name, type, offset) inline type& _name() \
{ \
	return *reinterpret_cast<type*>(uintptr_t(this) + offset); \
}