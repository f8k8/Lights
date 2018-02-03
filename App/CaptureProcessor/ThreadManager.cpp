#include "stdafx.h"

#include "ThreadManager.h"

#include "ScreenProcessor.h"
#include "DuplicationManager.h"

#include "VertexShader.h"
#include "PixelShader.h"

using namespace Microsoft::WRL;

class ThreadProc
{
public:
	ThreadProc() :
		m_SharedSurface(nullptr),
		m_KeyMutex(nullptr)
	{
		m_ScreenProcessor = new ScreenProcessor();
		m_DuplicationManager = new DuplicationManager();
	}

	~ThreadProc()
	{
		if (m_SharedSurface)
		{
			m_SharedSurface = nullptr;
		}

		if (m_KeyMutex)
		{
			m_KeyMutex = nullptr;
		}

		delete m_ScreenProcessor;
		delete m_DuplicationManager;
	}

	void Run(ThreadManager::ThreadData* threadData)
	{
		// Get desktop
		HDESK currentDesktop = nullptr;
		currentDesktop = OpenInputDesktop(0, FALSE, GENERIC_ALL);
		if (!currentDesktop)
		{
			// We do not have access to the desktop so request a retry
			SetEvent(threadData->expectedErrorEvent);
			return;
		}

		// Attach desktop to this thread
		bool desktopAttached = SetThreadDesktop(currentDesktop) != 0;
		CloseDesktop(currentDesktop);
		currentDesktop = nullptr;
		if (!desktopAttached)
		{
			// We do not have access to the desktop so request a retry
			SetEvent(threadData->expectedErrorEvent);
			return;
		}

		// New display manager
		if (!m_ScreenProcessor->Initialise())
		{
			SetEvent(threadData->unexpectedErrorEvent);
			return;
		}

		// Obtain handle to sync shared Surface
		HRESULT hr = m_ScreenProcessor->GetDevice()->OpenSharedResource(threadData->texSharedHandle, __uuidof(ID3D11Texture2D), &m_SharedSurface);
		if (FAILED(hr))
		{
			SetEvent(threadData->unexpectedErrorEvent);
			return;
		}

		hr = m_SharedSurface.As(&m_KeyMutex);
		if (FAILED(hr))
		{
			SetEvent(threadData->unexpectedErrorEvent);
			return;
		}

		// Make duplication manager
		if(!m_DuplicationManager->Initialise(m_ScreenProcessor->GetDevice(), threadData->output))
		{
			SetEvent(threadData->unexpectedErrorEvent);
			return;
		}
		
		// Main duplication loop
		bool waitToProcessCurrentFrame = false;
		while ((WaitForSingleObjectEx(threadData->terminateThreadsEvent, 0, FALSE) == WAIT_TIMEOUT))
		{
			if (!waitToProcessCurrentFrame)
			{
				// Get new frame from desktop duplication
				bool timedOut = false;
				if(!m_DuplicationManager->GetFrame(&timedOut))
				{
					// An error occurred getting the next frame drop out of loop which
					// will check if it was expected or not
					break;
				}

				// Check for timeout
				if (timedOut)
				{
					// No new frame at the moment
					continue;
				}

				if (m_DuplicationManager->GetDirtyCount() == 0 && m_DuplicationManager->GetMoveCount() == 0)
				{
					// No need to update
					m_DuplicationManager->ReleaseFrame();
					continue;
				}
			}

			// We have a new frame so try and process it
			// Try to acquire keyed mutex in order to access shared surface
			hr = m_KeyMutex->AcquireSync(0, 1000);
			if (hr == static_cast<HRESULT>(WAIT_TIMEOUT))
			{
				// Can't use shared surface right now, try again later
				waitToProcessCurrentFrame = true;
				continue;
			}
			else if (FAILED(hr))
			{
				// Generic unknown failure
				m_DuplicationManager->ReleaseFrame();
				break;
			}

			// We can now process the current frame
			waitToProcessCurrentFrame = false;

			// Process new frame
			if(!m_ScreenProcessor->ProcessFrame(*m_DuplicationManager, m_SharedSurface, threadData->offsetX, threadData->offsetY))
			{
				m_DuplicationManager->ReleaseFrame();
				m_KeyMutex->ReleaseSync(1);
				break;
			}

			// Release acquired keyed mutex
			hr = m_KeyMutex->ReleaseSync(1);
			if (FAILED(hr))
			{
				m_DuplicationManager->ReleaseFrame();
				break;
			}

			// Release frame back to desktop duplication
			if(!m_DuplicationManager->ReleaseFrame())
			{
				break;
			}
		}

		// Unexpected error so exit the application
		SetEvent(threadData->unexpectedErrorEvent);
	}

private:
	// Shared surface & mutex to access it
	ComPtr<ID3D11Texture2D> m_SharedSurface;
	ComPtr<IDXGIKeyedMutex> m_KeyMutex;

	// Screen processor & duplication manager for this thread
	ScreenProcessor* m_ScreenProcessor;
	DuplicationManager* m_DuplicationManager;
};

// Entry point for new duplication threads
//
DWORD WINAPI DuplicationThreadProc(void* param)
{
	// Data passed in from thread creation
	ThreadManager::ThreadData* threadData = reinterpret_cast<ThreadManager::ThreadData*>(param);
	ThreadProc* threadProc = new ThreadProc();
	threadProc->Run(threadData);
	delete threadProc;

	return 0;
}

ThreadManager::ThreadManager() : m_ThreadCount(0)
{

}

ThreadManager::~ThreadManager()
{
	for (auto &threadHandle : m_ThreadHandles)
	{
		if (threadHandle)
		{
			CloseHandle(threadHandle);
		}
	}
	m_ThreadHandles.resize(0);
	m_ThreadData.resize(0);
	m_ThreadCount = 0;
}

//
// Start up threads for DDA
//
bool ThreadManager::Initialise(int singleOutput, unsigned int outputCount, HANDLE unexpectedErrorEvent, HANDLE expectedErrorEvent, HANDLE terminateThreadsEvent, HANDLE sharedHandle, const RECT& desktopDimensions)
{
	m_ThreadCount = outputCount;
	m_ThreadHandles.resize(m_ThreadCount);
	m_ThreadData.resize(m_ThreadCount);

	// Create appropriate # of threads for duplication
	for (unsigned int threadIndex = 0; threadIndex < m_ThreadCount; ++threadIndex)
	{
		m_ThreadData[threadIndex].unexpectedErrorEvent = unexpectedErrorEvent;
		m_ThreadData[threadIndex].expectedErrorEvent = expectedErrorEvent;
		m_ThreadData[threadIndex].terminateThreadsEvent = terminateThreadsEvent;
		m_ThreadData[threadIndex].output = (singleOutput < 0) ? threadIndex : singleOutput;
		m_ThreadData[threadIndex].texSharedHandle = sharedHandle;
		m_ThreadData[threadIndex].offsetX = desktopDimensions.left;
		m_ThreadData[threadIndex].offsetY = desktopDimensions.top;

		DWORD threadID;
		m_ThreadHandles[threadIndex] = CreateThread(nullptr, 0, DuplicationThreadProc, &m_ThreadData[threadIndex], 0, &threadID);
		if (m_ThreadHandles[threadIndex] == nullptr)
		{
			false;
		}
	}

	return true;
}

//
// Waits infinitely for all spawned threads to terminate
//
void ThreadManager::WaitForThreadTermination()
{
	if (m_ThreadCount != 0)
	{
		WaitForMultipleObjectsEx(m_ThreadCount, &m_ThreadHandles[0], TRUE, INFINITE, FALSE);
	}
}
