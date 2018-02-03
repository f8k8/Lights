#pragma once

// For handling dynamic wait times with a back off
class DynamicWait
{
public:
	DynamicWait();
	~DynamicWait();

	void Wait();

private:
	unsigned int m_CurrentWaitBandIdx;
	unsigned int m_WaitCountInCurrentBand;
	LARGE_INTEGER m_QPCFrequency;
	LARGE_INTEGER m_LastWakeUpTime;
	bool m_QPCValid;
};
