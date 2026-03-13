#include "../SDK/SDK.h"

#include "../Features/EnginePrediction/EnginePrediction.h"
#include "../Features/Spectate/Spectate.h"
#include "../Features/Misc/AutoVote/AutoVote.h"
#include "../Features/Configs/Configs.h"

MAKE_HOOK(CHLClient_LevelShutdown, U::Memory.GetVirtual(I::Client, 7), void,
	void* rcx)
{
	DEBUG_RETURN(CHLClient_LevelShutdown, rcx);

	H::Entities.Clear(true);
	F::EnginePrediction.Unload();
	F::Spectate.Reset();
#ifndef TEXTMODE
	G::TriggerStorage.clear();
#endif
	F::AutoVote.Reset();
	F::Configs.HandleAutoConfig(false);

	CALL_ORIGINAL(rcx);
}