#pragma once

#include "DisplayProcessor.h"
#include "ThreadManager.h"
#include "DynamicWait.h"

class CaptureProcessor
{
public:
	CaptureProcessor();
	~CaptureProcessor();

	bool Start(int singleOutput);
	void Process();
	void Stop();

private:
	bool m_Running;
	bool m_FirstTime;

	int m_SingleOutput;

	HANDLE m_UnexpectedErrorEvent;
	HANDLE m_ExpectedErrorEvent;
	HANDLE m_TerminateThreadsEvent;

	DynamicWait m_DynamicWait;
	DisplayProcessor m_DisplayProcessor;
	ThreadManager m_ThreadManager;
};