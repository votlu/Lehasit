#include "stdafx.h"

#include "sdk/recv.h"
#include "utils/memory.h"
#include "utils/recv.h"
#include "utils/draw.h"
#include "Globals.h"
#include "hooks.h"
#include "Interfaces.h"
#include "Offsets.h"
#include "menu.h"
#include "config.h"

DWORD WINAPI SetupThread(LPVOID params);

BOOL WINAPI DllMain(HINSTANCE dll_inst, DWORD reason, void* reserved)
{
	// CreateThread is fully safe in DllMain since it's kernel32, which is guaranteed to be loaded in by now
	if (reason == DLL_PROCESS_ATTACH)
		CreateThread(NULL, 0, SetupThread, NULL, 0, NULL);

	return TRUE;
}

DWORD WINAPI SetupThread(LPVOID params)
{
	//AllocConsole();
	//freopen("CONOUT$", "w", stdout);
	//printf("Keter v0.0.2\n");

	// MSDN suggests we use TerminateProcess if we don't know the state of other threads in a process
	// otherwise risk a deadlock.

	// TerminateProcess the current process (hl2.exe) if any of our initialization fails
	// don't worry about GetCurrentProcess opening handles, it just returns a placeholder
	// ((HANDLE)-1) which just tells TerminateProcess to kill the current process.

	if (!g_interfaces.load())
	{
		MessageBoxA(NULL, "Failed to load interfaces", "Initialization failure", MB_OK);
		TerminateProcess(GetCurrentProcess(), -1);
		return -1;
	}

	if (!g_offsets.load())
	{
		MessageBoxA(NULL, "Failed to load offsets", "Initialization failure", MB_OK);
		TerminateProcess(GetCurrentProcess(), -2);
		return -2;
	}

	// TODO: Find better way of finding globals, hopefully not by hooking CHLClient::Init.
	void* globalsScan = utils::ScanModule("client.dll", "A3 ? ? ? ? 8D 45 08 6A 01 50 E8");//("engine.dll", "A1 ? ? ? ? 8B 11 68 ? ? ? ? 50 50 FF 12");
	if (globalsScan != NULL)
	{
		g_pGlobals = **reinterpret_cast<CGlobalVarsBase***>((size_t)globalsScan + 1);
	}
	else
	{
		MessageBoxA(NULL, "Couldn't find CGlobalVarsBase ptr!", "Failure", MB_OK);
		TerminateProcess(GetCurrentProcess(), -3);
		return -3;
	}

	// Straight from F1Public (im lazy)
	DWORD dwFn = (DWORD)utils::ScanModule("client.dll", "E8 ? ? ? ? F3 0F 10 4D ? 8D 85 ? ? ? ? F3 0F 10 45 ? F3 0F 59 C9 56 F3 0F 59 C0 F3 0F 58 C8 0F 2F 0D ? ? ? ? 76 07") + 0x1;
	//void* estimateAbsVelocityScan = utils::ScanModule("client.dll", "55 8B ? ? ? ? 56 8b f1 e8 72 ff 00 00 3b f0");
	if (dwFn != 0x1)
	{
		DWORD dwEstimate = ((*(PDWORD)(dwFn)) + dwFn + 4);
		g_CBaseEntity_EstimateAbsVelocity = reinterpret_cast<CBaseEntity_EstimateAbsVelocity>(dwEstimate);
		//g_CBaseEntity_EstimateAbsVelocity = **reinterpret_cast<CBaseEntity_EstimateAbsVelocity>((size_t)estimateAbsVelocityScan + 2);
	}
	else
	{
		MessageBoxA(NULL, "Couldn't find CBaseEntity::EstimateAbsVelocity ptr!", "Failure", MB_OK);
		TerminateProcess(GetCurrentProcess(), -4);
		return -4;
	}

	g_config.load();
	utils::InitializeDrawing();
	ApplyHooks(); // Apply hooks for hack things and for getting the movehelper

	typedef void(__cdecl* MsgFn)(char const* message, ...);
	MsgFn msg_fn = (MsgFn)GetProcAddress(GetModuleHandleA("tier0.dll"), "Msg");

	msg_fn("[Lehasit]: Loaded successfully\n");

	return 0;
}