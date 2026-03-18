#pragma once
#include "../../Utils/Lifecycle/Lifecycle.h"

// ---------------------------------------------------------------------------
// MAKE_EVENT - generates a pure-virtual base class whose instances are
// automatically tracked by InstTracker<Has##name>.
//
// Usage:
//   MAKE_EVENT(LevelInit);
//   // Generates:
//   //   class HasLevelInit : public InstTracker<HasLevelInit> { ... };
//
//   class CMyFeature : public HasLevelInit {
//   public:
//       bool onLevelInit() override { return true; }
//   };
//
//   // Iterate all registered listeners:
//   for (auto* p : HasLevelInit::GetInstances()) p->onLevelInit();
// ---------------------------------------------------------------------------
#define MAKE_EVENT(name, ...) \
class Has##name : public InstTracker<Has##name> \
{ \
public: \
    Has##name() : InstTracker() {} \
    virtual ~Has##name() = default; \
    virtual bool on##name(__VA_ARGS__) = 0; \
};

// ---------------------------------------------------------------------------
// Standard game lifecycle events (in addition to HasLoad / HasUnload /
// HasGameEvent which are already declared in Lifecycle.h).
// ---------------------------------------------------------------------------

// Called once when a map begins loading.
MAKE_EVENT(LevelInit)

// Called just before entities are spawned on a new map.
MAKE_EVENT(LevelInitPreEntity, const char* pMapName)

// Called when the current map is shutting down.
MAKE_EVENT(LevelShutdown)

// Called when the viewport/screen resolution changes.
MAKE_EVENT(ScreenSizeChange)

// ---------------------------------------------------------------------------
// HasGameEventMacro is intentionally omitted here: IGameEvent-based handling
// is already covered by HasGameEvent in Lifecycle.h.
// ---------------------------------------------------------------------------
