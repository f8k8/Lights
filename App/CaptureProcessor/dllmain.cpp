// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "CaptureProcessor.h"

__declspec(thread) CaptureProcessor* g_CaptureProcessor = nullptr;

extern "C" {
	bool Start();
	void Process();
	void Stop();
}

bool Start()
{
	if (g_CaptureProcessor != nullptr)
	{
		return false;
	}
	g_CaptureProcessor = new CaptureProcessor();
	return g_CaptureProcessor->Start(-1);
}

void Process()
{
	if (g_CaptureProcessor)
	{
		g_CaptureProcessor->Process();
	}
}

void Stop()
{
	if (g_CaptureProcessor)
	{
		g_CaptureProcessor->Stop();
		delete g_CaptureProcessor;
		g_CaptureProcessor = nullptr;
	}
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

