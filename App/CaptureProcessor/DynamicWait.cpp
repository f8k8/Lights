#include "stdafx.h"

#include "DynamicWait.h"

// Period in seconds that a new wait call is considered part of the same wait sequence
static const unsigned int WaitSequenceTimeInSeconds = 2;

#define WAIT_BAND_STOP 0

struct WaitBand
{
	unsigned int waitTime;
	unsigned int waitCount;
};

static const WaitBand WaitBands[] = {
	{ 250, 20 },
	{ 2000, 60 },
	{ 5000, WAIT_BAND_STOP }
};
	

DynamicWait::DynamicWait() : m_CurrentWaitBandIdx(0), m_WaitCountInCurrentBand(0)
{
	m_QPCValid = QueryPerformanceFrequency(&m_QPCFrequency);
	m_LastWakeUpTime.QuadPart = 0L;
}

DynamicWait::~DynamicWait()
{
}

void DynamicWait::Wait()
{
	LARGE_INTEGER CurrentQPC = { 0 };

	// Is this wait being called with the period that we consider it to be part of the same wait sequence
	QueryPerformanceCounter(&CurrentQPC);
	if (m_QPCValid && (CurrentQPC.QuadPart <= (m_LastWakeUpTime.QuadPart + (m_QPCFrequency.QuadPart * WaitSequenceTimeInSeconds))))
	{
		// We are still in the same wait sequence, lets check if we should move to the next band
		if ((WaitBands[m_CurrentWaitBandIdx].waitCount != WAIT_BAND_STOP) && (m_WaitCountInCurrentBand > WaitBands[m_CurrentWaitBandIdx].waitCount))
		{
			m_CurrentWaitBandIdx++;
			m_WaitCountInCurrentBand = 0;
		}
	}
	else
	{
		// Either we could not get the current time or we are starting a new wait sequence
		m_WaitCountInCurrentBand = 0;
		m_CurrentWaitBandIdx = 0;
	}

	// Sleep for the required period of time
	Sleep(WaitBands[m_CurrentWaitBandIdx].waitTime);

	// Record the time we woke up so we can detect wait sequences
	QueryPerformanceCounter(&m_LastWakeUpTime);
	m_WaitCountInCurrentBand++;
}