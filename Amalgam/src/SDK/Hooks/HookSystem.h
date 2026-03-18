#pragma once
#include "../../Utils/Lifecycle/Lifecycle.h"
#include <string>
#include <functional>

// HookEntry - auto-registered hook descriptor that stores a name, an
// initialization callback, and the trampoline pointer to the original function.
//
// All HookEntry instances are tracked via InstTracker<HookEntry>, so the hook
// initialization loop simply iterates GetInstances() and calls init() on each.
//
// Prefer the MAKE_HOOK macro in Utils/Hooks/Hooks.h for MinHook-based hooks;
// use this class when you need the InstTracker pattern for custom back-ends.
class HookEntry : public InstTracker<HookEntry>
{
private:
	std::string             m_name{};
	std::function<bool()>   m_init_fn{};
	void*                   m_original_fn = nullptr;

public:
	HookEntry(std::string_view name, const std::function<bool()>& init_fn)
		: InstTracker()
		, m_name(name)
		, m_init_fn(init_fn)
	{
	}

	// Non-copyable, non-movable (the InstTracker list holds raw pointers).
	HookEntry(const HookEntry&)            = delete;
	HookEntry& operator=(const HookEntry&) = delete;

	// Run the user-supplied initialization callback.
	bool init() const { return m_init_fn ? m_init_fn() : false; }

	// Display name for diagnostics.
	const std::string& name() const { return m_name; }

	// Store the trampoline returned by the hooking back-end.
	void set_original(void* ptr) { m_original_fn = ptr; }

	// Retrieve the original function cast to the requested function-pointer type.
	template<typename T>
	T call() const { return reinterpret_cast<T>(m_original_fn); }
};

// ---------------------------------------------------------------------------
// MAKE_HOOK_ENTRY - convenience macro for declaring a named HookEntry alongside
// its hook function in a sub-namespace, following the existing Hooks:: convention.
//
// Example:
//   MAKE_HOOK_ENTRY(MyHook, void, void* pSelf)
//   {
//       // hook body
//       return MyHook_entry.call<FN>()(pSelf);
//   }
// ---------------------------------------------------------------------------
#define MAKE_HOOK_ENTRY(name, ret, ...) \
namespace HookEntries { \
    namespace name { \
        bool _init(); \
        inline HookEntry entry{ #name, _init }; \
        using FN = ret(__fastcall*)(__VA_ARGS__); \
        ret __fastcall func(__VA_ARGS__); \
    } \
} \
ret __fastcall HookEntries::name::func(__VA_ARGS__)
