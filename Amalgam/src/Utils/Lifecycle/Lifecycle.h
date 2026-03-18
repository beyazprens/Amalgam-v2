#pragma once
#include <vector>
#include <algorithm>

// ---------------------------------------------------------------------------
// Lifecycle Interfaces
// ---------------------------------------------------------------------------
// These lightweight base structs define a standardized lifecycle contract.
// Feature classes can inherit from one or more of these to signal that they
// participate in a particular stage of the initialization/event pipeline.
//
// Usage example:
//   class CMyFeature : public HasLoad, public HasUnload
//   {
//   public:
//       bool Load() override { /* setup */ return true; }
//       void Unload() override { /* teardown */ }
//   };
// ---------------------------------------------------------------------------

// HasLoad - implemented by components that need explicit initialization.
struct HasLoad
{
	virtual bool Load() = 0;
	virtual ~HasLoad() = default;
};

// HasUnload - implemented by components that need explicit cleanup/deregistration.
struct HasUnload
{
	virtual void Unload() = 0;
	virtual ~HasUnload() = default;
};

// HasGameEvent - implemented by components that handle IGameEvent callbacks.
// Forward-declare IGameEvent to keep this header dependency-free; callers
// must include the full interface header themselves.
struct IGameEvent;
struct HasGameEvent
{
	virtual void FireGameEvent(IGameEvent* pEvent) = 0;
	virtual ~HasGameEvent() = default;
};

// ---------------------------------------------------------------------------
// InstTracker<T>
// ---------------------------------------------------------------------------
// CRTP base that automatically registers and deregisters instances of T in a
// global list.  Combine with lifecycle interfaces to enable centralized
// iteration over all live instances.
//
// Usage:
//   class CMyFeature : public InstTracker<CMyFeature>, public HasLoad { ... };
//
//   // Iterate all live instances:
//   for (auto* p : InstTracker<CMyFeature>::GetInstances()) { ... }
// ---------------------------------------------------------------------------

template <typename T>
class InstTracker
{
private:
	static std::vector<T*>& Instances()
	{
		static std::vector<T*> s_vInstances;
		return s_vInstances;
	}

public:
	InstTracker()
	{
		Instances().push_back(static_cast<T*>(this));
	}

	// Non-copyable: copying would duplicate the registration.
	InstTracker(const InstTracker&) = delete;
	InstTracker& operator=(const InstTracker&) = delete;

	// Movable: transfer registration from the old object to the new one.
	InstTracker(InstTracker&& other) noexcept
	{
		auto& v = Instances();
		auto it = std::find(v.begin(), v.end(), static_cast<T*>(&other));
		if (it != v.end())
			*it = static_cast<T*>(this);
		else
			v.push_back(static_cast<T*>(this));
	}
	InstTracker& operator=(InstTracker&&) = delete;

	~InstTracker()
	{
		auto& v = Instances();
		v.erase(std::remove(v.begin(), v.end(), static_cast<T*>(this)), v.end());
	}

	// Returns the global list of all live instances of T.
	static const std::vector<T*>& GetInstances()
	{
		return Instances();
	}
};
