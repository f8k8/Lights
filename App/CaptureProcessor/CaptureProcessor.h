#pragma once

#include "LightProcessor.h"
#include "ThreadManager.h"
#include "DynamicWait.h"

// Main capture processor to start / stop capturing
class CaptureProcessor
{
public:
	CaptureProcessor();
	~CaptureProcessor();

	bool Start(int singleOutput, int lightColumns, int lightRows);
	bool Process();
	void GetLightValues(__int32* values, int length);
	void Stop();

private:
	bool m_Running;
	bool m_FirstTime;

	int m_SingleOutput;
	int m_LightColumns;
	int m_LightRows;

	HANDLE m_UnexpectedErrorEvent;
	HANDLE m_ExpectedErrorEvent;
	HANDLE m_TerminateThreadsEvent;

	DynamicWait m_DynamicWait;
	LightProcessor* m_LightProcessor;
	ThreadManager* m_ThreadManager;
};