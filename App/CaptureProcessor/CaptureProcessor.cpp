// CaptureProcessor.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "CaptureProcessor.h"

CaptureProcessor::CaptureProcessor() :
	m_Running(false),
	m_FirstTime(true),
	m_SingleOutput(-1),
	m_LightColumns(0),
	m_LightRows(0),
	m_UnexpectedErrorEvent(NULL),
	m_ExpectedErrorEvent(NULL),
	m_TerminateThreadsEvent(NULL),
	m_LightProcessor(nullptr),
	m_ThreadManager(nullptr)
{
	m_ColourScale[0] = m_ColourScale[1] = m_ColourScale[2] = 1.0f;
}

CaptureProcessor::~CaptureProcessor()
{
	Stop();
}

bool CaptureProcessor::Start(int singleOutput, int lightColumns, int lightRows)
{
	if (m_Running)
	{
		return false;
	}

	m_FirstTime = true;
	m_SingleOutput = singleOutput;
	m_LightColumns = lightColumns;
	m_LightRows = lightRows;

	// Event used by the threads to signal an unexpected error and we want to quit the app
	m_UnexpectedErrorEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!m_UnexpectedErrorEvent)
	{
		return false;
	}

	// Event for when a thread encounters an expected error
	m_ExpectedErrorEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!m_ExpectedErrorEvent)
	{
		return false;
	}

	// Event to tell spawned threads to quit
	m_TerminateThreadsEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!m_TerminateThreadsEvent)
	{
		return false;
	}

	m_Running = true;

	return true;
}

bool CaptureProcessor::Process()
{
	if (!m_Running)
	{
		return false;
	}

	if (WaitForSingleObjectEx(m_UnexpectedErrorEvent, 0, FALSE) == WAIT_OBJECT_0)
	{
		// Unexpected error occurred so exit the application
		Stop();
		return false;
	}
	else if (m_FirstTime || WaitForSingleObjectEx(m_ExpectedErrorEvent, 0, FALSE) == WAIT_OBJECT_0)
	{
		if (!m_FirstTime)
		{
			// Terminate other threads
			SetEvent(m_TerminateThreadsEvent);
			m_ThreadManager->WaitForThreadTermination();
			ResetEvent(m_TerminateThreadsEvent);
			ResetEvent(m_ExpectedErrorEvent);

			// Clean up
			delete m_ThreadManager;
			m_ThreadManager = nullptr;
			delete m_LightProcessor;
			m_LightProcessor = nullptr;

			// As we have encountered an error due to a system transition we wait before trying again, using this dynamic wait
			// the wait periods will get progressively long to avoid wasting too much system resource if this state lasts a long time
			m_DynamicWait.Wait();
		}
		else
		{
			// First time through the loop so nothing to clean up
			m_FirstTime = false;
		}

		// Re-initialize
		m_LightProcessor = new LightProcessor();
		m_ThreadManager = new ThreadManager();
		if (m_LightProcessor->Initialise(m_SingleOutput, m_LightColumns, m_LightRows, m_UnexpectedErrorEvent, m_ExpectedErrorEvent))
		{
			HANDLE sharedHandle = m_LightProcessor->GetSharedSurfaceHandle();
			if (sharedHandle)
			{
				m_ThreadManager->Initialise(m_SingleOutput, m_LightProcessor->GetOutputCount(), m_UnexpectedErrorEvent, m_ExpectedErrorEvent, m_TerminateThreadsEvent, sharedHandle, m_LightProcessor->GetDesktopBounds());
			}
			else
			{
				return false;
			}
		}
	}
	else
	{
		// Try and acquire sync on common display buffer
		m_LightProcessor->SetColourScale(m_ColourScale[0], m_ColourScale[1], m_ColourScale[2]);
		if (m_LightProcessor->ProcessFrame())
		{
			return true;
		}
	}

	return false;
}

bool CaptureProcessor::IsRunning()
{
	return m_Running;
}

void CaptureProcessor::SetColourScale(float r, float g, float b)
{
	m_ColourScale[0] = r;
	m_ColourScale[1] = g;
	m_ColourScale[2] = b;
}

void CaptureProcessor::GetLightValues(__int32* values, int length)
{
	memcpy(values, &m_LightProcessor->GetLightValues()[0], length * 4);
}

void CaptureProcessor::Stop()
{
	if (!m_Running)
	{
		return;
	}

	m_Running = false;

	// Make sure all other threads have exited
	if (SetEvent(m_TerminateThreadsEvent))
	{
		m_ThreadManager->WaitForThreadTermination();
	}
	
	// Clean up
	delete m_ThreadManager;
	m_ThreadManager = nullptr;
	delete m_LightProcessor;
	m_LightProcessor = nullptr;

	// Clean up
	CloseHandle(m_UnexpectedErrorEvent);
	m_UnexpectedErrorEvent = NULL;
	CloseHandle(m_ExpectedErrorEvent);
	m_ExpectedErrorEvent = NULL;
	CloseHandle(m_TerminateThreadsEvent);
	m_TerminateThreadsEvent = NULL;
}

