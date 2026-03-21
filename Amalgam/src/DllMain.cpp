#include <Windows.h>
#include "Core/Core.h"
#include "Utils/ExceptionHandler/ExceptionHandler.h"

DWORD WINAPI MainThread(LPVOID lpParam)
{
	U::ExceptionHandler.Initialize(lpParam);
	U::Core.Load();
	U::Core.Loop();
	U::ExceptionHandler.Unload();
	U::Core.Unload();

	FreeLibraryAndExitThread(static_cast<HMODULE>(lpParam), EXIT_SUCCESS);
}

// Register / unregister the .pdata (RUNTIME_FUNCTION) exception-handler table so
// that x64 stack unwinding works correctly when the DLL is loaded via manual map
// injection (which bypasses the Windows loader and therefore never registers the
// exception directory automatically).  Calling these functions is safe even when
// the DLL was loaded normally via LoadLibrary; the dynamic entry is simply unused
// because the loader's static module list is consulted first.
static PRUNTIME_FUNCTION s_pFunctionTable = nullptr;

static void RegisterExceptionTable(HINSTANCE hinstDLL)
{
	auto pBase = reinterpret_cast<BYTE*>(hinstDLL);
	if (!pBase)
		return;

	// Some manual-map injectors erase the DOS/NT headers after mapping for
	// stealth.  Guard every header access so a missing header is a silent
	// no-op rather than an access-violation crash.
	auto pDos = PIMAGE_DOS_HEADER(pBase);
	if (pDos->e_magic != IMAGE_DOS_SIGNATURE)
		return;

	auto pNt = PIMAGE_NT_HEADERS(pBase + pDos->e_lfanew);
	if (pNt->Signature != IMAGE_NT_SIGNATURE)
		return;

	auto& exDir = pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];

	if (exDir.VirtualAddress && exDir.Size)
	{
		s_pFunctionTable = PRUNTIME_FUNCTION(pBase + exDir.VirtualAddress);
		if (RtlAddFunctionTable(s_pFunctionTable,
		                        exDir.Size / sizeof(RUNTIME_FUNCTION),
		                        reinterpret_cast<DWORD64>(pBase)) == FALSE)
		{
			// Registration failed (e.g. already registered); clear the pointer
			// so UnregisterExceptionTable does not attempt to remove it.
			s_pFunctionTable = nullptr;
		}
	}
}

static void UnregisterExceptionTable()
{
	if (s_pFunctionTable)
	{
		RtlDeleteFunctionTable(s_pFunctionTable);
		s_pFunctionTable = nullptr;
	}
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		// Register exception unwind tables BEFORE spawning the worker thread so
		// that any exception raised in the thread can be properly dispatched.
		RegisterExceptionTable(hinstDLL);

		if (const auto hThread = CreateThread(nullptr, 0, MainThread, hinstDLL, 0, nullptr))
			CloseHandle(hThread);
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
		UnregisterExceptionTable();
	}

	return TRUE;
}