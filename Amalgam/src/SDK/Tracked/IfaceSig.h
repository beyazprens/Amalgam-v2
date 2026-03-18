#pragma once
#include "../../Utils/Lifecycle/Lifecycle.h"
#include "../../Utils/Memory/Memory.h"
#include <string>
#include <functional>
#include <cstdint>

// ---------------------------------------------------------------------------
// Iface - auto-registered game-interface descriptor.
//
// Each Iface instance registers with InstTracker<Iface>.  The central
// initialization loop calls init() on every registered instance:
//
//   for (auto* iface : Iface::GetInstances()) {
//       if (!iface->init())
//           SDK::Output(...);
//   }
//
// Usage via MAKE_IFACE:
//   MAKE_IFACE(IVEngineClient, engine,
//       U::Memory.FindInterface("engine.dll", "VEngineClient013"));
//
//   // Access the interface:
//   I::engine->GetViewAngles(...);
// ---------------------------------------------------------------------------
class Iface : public InstTracker<Iface>
{
private:
	void**                        m_ptr{};
	std::string                   m_name{};
	std::function<void*()>        m_addr_fn{};

public:
	Iface(void** ptr, std::string_view name, const std::function<void*()>& addr_fn)
		: InstTracker()
		, m_ptr(ptr)
		, m_name(name)
		, m_addr_fn(addr_fn)
	{
	}

	// Non-copyable
	Iface(const Iface&)            = delete;
	Iface& operator=(const Iface&) = delete;

	// Returns the currently stored interface pointer (may be null before init).
	void* ptr() const { return m_ptr ? *m_ptr : nullptr; }

	const std::string& name() const { return m_name; }

	// Resolve the interface address via m_addr_fn and store it in *m_ptr.
	// Returns true on success.
	bool init()
	{
		if (!m_ptr || !m_addr_fn)
			return false;

		*m_ptr = m_addr_fn();
		return *m_ptr != nullptr;
	}
};

// ---------------------------------------------------------------------------
// Sig - auto-registered pattern-scan / signature descriptor.
//
// Usage via MAKE_SIG:
//   MAKE_SIG(CreateMove,
//       U::Memory.FindSignature("engine.dll", "55 8B EC 83 EC 10"));
//
//   // Access the address:
//   auto addr = S::CreateMove;
// ---------------------------------------------------------------------------
class Sig : public InstTracker<Sig>
{
private:
	uintptr_t*                    m_ptr{};
	std::string                   m_name{};
	std::function<uintptr_t()>    m_addr_fn{};

public:
	Sig(uintptr_t* ptr, std::string_view name, const std::function<uintptr_t()>& addr_fn)
		: InstTracker()
		, m_ptr(ptr)
		, m_name(name)
		, m_addr_fn(addr_fn)
	{
	}

	// Non-copyable
	Sig(const Sig&)            = delete;
	Sig& operator=(const Sig&) = delete;

	// Returns the stored address (0 before init).
	uintptr_t addr() const { return m_ptr ? *m_ptr : 0; }

	const std::string& name() const { return m_name; }

	// Resolve the address via m_addr_fn and store it in *m_ptr.
	// Returns true on success.
	bool init()
	{
		if (!m_ptr || !m_addr_fn)
			return false;

		*m_ptr = m_addr_fn();
		return *m_ptr != 0;
	}
};

// ---------------------------------------------------------------------------
// MAKE_IFACE(type, name, addr_expr)
//
// Declares:
//   I::name  - typed interface pointer (namespace I).
//   make_iface::name - auto-registered Iface descriptor.
// ---------------------------------------------------------------------------
#define MAKE_IFACE(type, name, addr_expr) \
    namespace I { inline type* name{}; } \
    namespace make_iface { \
        inline Iface name{ \
            reinterpret_cast<void**>(&I::name), \
            #name, \
            []() -> void* { return reinterpret_cast<void*>(addr_expr); } \
        }; \
    }

// ---------------------------------------------------------------------------
// MAKE_SIG(name, addr_expr)
//
// Declares:
//   S::name  - uintptr_t holding the scanned address (namespace S).
//   make_sig::name - auto-registered Sig descriptor.
// ---------------------------------------------------------------------------
#define MAKE_SIG(name, addr_expr) \
    namespace S { inline uintptr_t name{}; } \
    namespace make_sig { \
        inline Sig name{ &S::name, #name, []() -> uintptr_t { return (addr_expr); } }; \
    }
