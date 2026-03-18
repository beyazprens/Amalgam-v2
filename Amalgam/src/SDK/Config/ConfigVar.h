#pragma once
#include "../../Utils/Lifecycle/Lifecycle.h"
#include <string>
#include <typeinfo>

// ---------------------------------------------------------------------------
// TrackedConfigVar - auto-registered configuration variable descriptor.
//
// Each instance registers itself with InstTracker<TrackedConfigVar> so that a
// central save/load routine can iterate all variables without maintaining a
// separate registry.  The actual value lives in the variable pointed to by
// m_ptr; this class only holds metadata.
//
// This is intentionally separate from the existing Vars::ConfigVar<T> template
// (which handles ImGui bindings and flags).  TrackedConfigVar is a lightweight
// companion used purely for serialization discovery.
//
// Usage via MAKE_CFGVAR:
//   MAKE_CFGVAR(aimbot_enabled, false);      // bool
//   MAKE_CFGVAR(aimbot_fov,     180.0f);     // float
//
//   // Access the value:
//   cfg::aimbot_enabled = true;
//
//   // Iterate all registered vars (e.g. for JSON save):
//   for (auto* var : TrackedConfigVar::GetInstances()) { ... }
// ---------------------------------------------------------------------------
class TrackedConfigVar : public InstTracker<TrackedConfigVar>
{
private:
	std::string m_name{};
	void*       m_ptr{};
	size_t      m_type_hash{};

public:
	TrackedConfigVar(std::string_view name, void* ptr, size_t type_hash)
		: InstTracker()
		, m_name(name)
		, m_ptr(ptr)
		, m_type_hash(type_hash)
	{
	}

	// Non-copyable (InstTracker holds raw pointers into static storage)
	TrackedConfigVar(const TrackedConfigVar&)            = delete;
	TrackedConfigVar& operator=(const TrackedConfigVar&) = delete;

	const std::string& name()      const { return m_name;      }
	void*              ptr()       const { return m_ptr;       }
	size_t             type_hash() const { return m_type_hash; }

	// Convenience: cast the stored pointer to T*. Returns nullptr on type mismatch.
	template<typename T>
	T* as() const
	{
		if (typeid(T).hash_code() != m_type_hash)
			return nullptr;
		return static_cast<T*>(m_ptr);
	}
};

// ---------------------------------------------------------------------------
// MAKE_CFGVAR(name, val)
//
// Declares two symbols:
//   cfg::name             - the actual variable (lives in namespace cfg).
//   cfgvar_tracker::name  - a TrackedConfigVar descriptor (auto-registered).
//
// Both are inline so the macro may appear in headers included from multiple TUs.
// ---------------------------------------------------------------------------
#define MAKE_CFGVAR(name, val) \
    namespace cfg { inline decltype(val) name{ val }; } \
    namespace cfgvar_tracker { \
        inline TrackedConfigVar name{ #name, &cfg::name, typeid(cfg::name).hash_code() }; \
    }
