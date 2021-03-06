// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "CaptureProcessor.h"

__declspec(thread) CaptureProcessor* g_CaptureProcessor = nullptr;

extern "C" {
	bool Start(int singleOutput, int lightColumns, int lightRows);
	bool Process();
	bool IsRunning();
	void SetColourScale(float r, float g, float b);
	void GetLightValues(__int32* values, int length);
	void Stop();
}

bool Start(int singleOutput, int lightColumns, int lightRows)
{
	if (g_CaptureProcessor != nullptr)
	{
		return false;
	}
	g_CaptureProcessor = new CaptureProcessor();
	return g_CaptureProcessor->Start(singleOutput, lightColumns, lightRows);
}

bool Process()
{
	if (g_CaptureProcessor)
	{
		return g_CaptureProcessor->Process();
	}
	return false;
}

bool IsRunning()
{
	if (g_CaptureProcessor)
	{
		return g_CaptureProcessor->IsRunning();
	}
	return false;
}

void SetColourScale(float r, float g, float b)
{
	if (g_CaptureProcessor)
	{
		g_CaptureProcessor->SetColourScale(r, g, b);
	}
}

void GetLightValues(__int32* values, int length)
{
	if (g_CaptureProcessor)
	{
		g_CaptureProcessor->GetLightValues(values, length);
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

