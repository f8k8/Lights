#include "stdafx.h"

#include "ThreadManager.h"

#include "DisplayManager.h"
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
		m_DisplayManager.InitD3D(threadData->directXResources);

		// Obtain handle to sync shared Surface
		HRESULT hr = threadData->directXResources.device->OpenSharedResource(threadData->texSharedHandle, __uuidof(ID3D11Texture2D), &m_SharedSurface);
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
		if(!m_DuplicationManager.Init(threadData->directXResources.device, threadData->output))
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
				if(!m_DuplicationManager.GetFrame(&timedOut))
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

				if (m_DuplicationManager.GetDirtyCount() == 0 && m_DuplicationManager.GetMoveCount() == 0)
				{
					// No need to update
					m_DuplicationManager.ReleaseFrame();
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
				m_DuplicationManager.ReleaseFrame();
				break;
			}

			// We can now process the current frame
			waitToProcessCurrentFrame = false;

			// Process new frame
			if(!m_DisplayManager.ProcessFrame(m_DuplicationManager, m_SharedSurface, threadData->offsetX, threadData->offsetY))
			{
				m_DuplicationManager.ReleaseFrame();
				m_KeyMutex->ReleaseSync(1);
				break;
			}

			// Release acquired keyed mutex
			hr = m_KeyMutex->ReleaseSync(1);
			if (FAILED(hr))
			{
				m_DuplicationManager.ReleaseFrame();
				break;
			}

			// Release frame back to desktop duplication
			if(!m_DuplicationManager.ReleaseFrame())
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

	// Display manager & duplication manager for this thread
	DisplayManager m_DisplayManager;
	DuplicationManager m_DuplicationManager;
};

// Entry point for new duplication threads
//
DWORD WINAPI DuplicationThreadProc(void* param)
{
	// Data passed in from thread creation
	ThreadManager::ThreadData* threadData = reinterpret_cast<ThreadManager::ThreadData*>(param);
	ThreadProc threadProc;
	threadProc.Run(threadData);

	return 0;
}

ThreadManager::ThreadManager() : m_ThreadCount(0)
{

}

ThreadManager::~ThreadManager()
{
	Clean();
}

//
// Clean up resources
//
void ThreadManager::Clean()
{
	for (auto &threadHandle : m_ThreadHandles)
	{
		if (threadHandle)
		{
			CloseHandle(threadHandle);
		}
	}
	m_ThreadHandles.resize(0);

	for(auto &threadData : m_ThreadData)
	{
		CleanDx(&threadData.directXResources);
	}
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

		RtlZeroMemory(&m_ThreadData[threadIndex].directXResources, sizeof(DirectXResources));
		if(!InitialiseDx(&m_ThreadData[threadIndex].directXResources))
		{
			return false;
		}

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

//
// Get DX_RESOURCES
//
bool ThreadManager::InitialiseDx(DirectXResources* resources)
{
	HRESULT hr = S_OK;

	// Driver types supported
	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	// Feature levels supported
	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_1
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	D3D_FEATURE_LEVEL featureLevel;

	// Create device
	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; ++driverTypeIndex)
	{
		hr = D3D11CreateDevice(nullptr, driverTypes[driverTypeIndex], nullptr, 
#if defined(_DEBUG) 
			D3D11_CREATE_DEVICE_DEBUG,
#else 
			0,
#endif
			featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &resources->device, &featureLevel, &resources->context);
		if (SUCCEEDED(hr))
		{
			// Device creation success, no need to loop anymore
			break;
		}
	}
	if (FAILED(hr))
	{
		return false;
	}

	// VERTEX shader
	UINT size = ARRAYSIZE(g_VS);
	hr = resources->device->CreateVertexShader(g_VS, size, nullptr, &resources->vertexShader);
	if (FAILED(hr))
	{
		return false;
	}

	// Input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	UINT numElements = ARRAYSIZE(layout);
	hr = resources->device->CreateInputLayout(layout, numElements, g_VS, size, &resources->inputLayout);
	if (FAILED(hr))
	{
		return false;
	}
	resources->context->IASetInputLayout(resources->inputLayout.Get());

	// Pixel shader
	size = ARRAYSIZE(g_PS);
	hr = resources->device->CreatePixelShader(g_PS, size, nullptr, &resources->pixelShader);
	if (FAILED(hr))
	{
		return false;
	}

	// Set up sampler
	D3D11_SAMPLER_DESC SampDesc;
	RtlZeroMemory(&SampDesc, sizeof(SampDesc));
	SampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	SampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	SampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	SampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SampDesc.MinLOD = 0;
	SampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = resources->device->CreateSamplerState(&SampDesc, &resources->samplerLinear);
	if (FAILED(hr))
	{
		return false;
	}

	return true;
}

//
// Clean up DX_RESOURCES
//
void ThreadManager::CleanDx(DirectXResources* resources)
{
	if (resources->context)
	{
		resources->context = nullptr;
	}

	if (resources->vertexShader)
	{
		resources->vertexShader = nullptr;
	}

	if (resources->pixelShader)
	{
		resources->pixelShader = nullptr;
	}

	if (resources->inputLayout)
	{
		resources->inputLayout = nullptr;
	}

	if (resources->samplerLinear)
	{
		resources->samplerLinear = nullptr;
	}
	
	if (resources->device)
	{
#if defined(_DEBUG) 
		ComPtr<ID3D11Debug> debugDevice = nullptr;
		resources->device.As(&debugDevice);
		debugDevice->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
		debugDevice = nullptr;
#endif

		resources->device = nullptr;
	}
}
