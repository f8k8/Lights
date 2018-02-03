#pragma once

#include <vector>

#include "DirectXResources.h"

// For handling threads for each screen
class ThreadManager
{
public:
	ThreadManager();
	~ThreadManager();
	bool Initialise(int singleOutput, unsigned int outputCount, HANDLE unexpectedErrorEvent, HANDLE expectedErrorEvent, HANDLE terminateThreadsEvent, HANDLE sharedHandle, const RECT& desktopDimensions);
	void WaitForThreadTermination();

public:
	struct ThreadData
	{
		// Used to indicate abnormal error condition
		HANDLE unexpectedErrorEvent;

		// Used to indicate a transition event occurred e.g. PnpStop, PnpStart, mode change, TDR, desktop switch and the application needs to recreate the duplication interface
		HANDLE expectedErrorEvent;

		// Used by WinProc to signal to threads to exit
		HANDLE terminateThreadsEvent;

		// Shared handle for textures
		HANDLE texSharedHandle;
		
		// Which output we're processing
		unsigned int output;

		// X / Y offsets of the desktop
		int offsetX;
		int offsetY;
	};

private:
	unsigned int m_ThreadCount;
	std::vector<HANDLE> m_ThreadHandles;
	std::vector<ThreadData> m_ThreadData;
};
