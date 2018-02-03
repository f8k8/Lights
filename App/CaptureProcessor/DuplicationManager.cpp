#include "stdafx.h"

#include "DuplicationManager.h"

using namespace Microsoft::WRL;

DuplicationManager::DuplicationManager() :
	m_Device(nullptr),
	m_Duplication(nullptr),
	m_AcquiredDesktopImage(nullptr),
	m_DirtyRects(nullptr),
	m_DirtyCount(0),
	m_MoveRects(nullptr),
	m_MoveCount(0)
{
	RtlZeroMemory(&m_OutputDesc, sizeof(m_OutputDesc));
	RtlZeroMemory(&m_FrameInfo, sizeof(m_FrameInfo));
}

DuplicationManager::~DuplicationManager()
{
	if (m_Duplication)
	{
		m_Duplication = nullptr;
	}

	if (m_AcquiredDesktopImage)
	{
		m_AcquiredDesktopImage = nullptr;
	}

	if (m_Device)
	{
		m_Device = nullptr;
	}
}

bool DuplicationManager::Init(ComPtr<ID3D11Device> device, unsigned int output)
{
	// Add a reference to the device
	m_Device = device;

	// Get DXGI device
	ComPtr<IDXGIDevice> dxgiDevice = nullptr;
	HRESULT hr = m_Device.As(&dxgiDevice);
	if (FAILED(hr))
	{
		return false;
	}

	// Get DXGI adapter
	ComPtr<IDXGIAdapter> dxgiAdapter = nullptr;
	hr = dxgiDevice.As(&dxgiAdapter);
	dxgiDevice = nullptr;
	if (FAILED(hr))
	{
		return false;
	}

	// Get the output interface
	ComPtr<IDXGIOutput> dxgiOutput = nullptr;
	hr = dxgiAdapter->EnumOutputs(output, &dxgiOutput);
	dxgiAdapter = nullptr;
	if (FAILED(hr))
	{
		return false;
	}

	dxgiOutput->GetDesc(&m_OutputDesc);

	// QI for the Output 1 interface
	ComPtr<IDXGIOutput1> dxgiOutput1 = nullptr;
	hr = dxgiOutput.As(&dxgiOutput1);
	dxgiOutput = nullptr;
	if (FAILED(hr))
	{
		return false;
	}

	// Create desktop duplication
	hr = dxgiOutput1->DuplicateOutput(m_Device.Get(), &m_Duplication);
	dxgiOutput1 = nullptr;
	if (FAILED(hr))
	{
		return false;
	}

	return true;
}

bool DuplicationManager::GetFrame(bool* timeout)
{
	ComPtr<IDXGIResource> desktopResource = nullptr;
	DXGI_OUTDUPL_FRAME_INFO frameInfo;

	m_MoveCount = 0;
	m_DirtyCount = 0;

	// Get new frame
	HRESULT hr = m_Duplication->AcquireNextFrame(500, &frameInfo, &desktopResource);
	if (hr == DXGI_ERROR_WAIT_TIMEOUT)
	{
		*timeout = true;
		return true;
	}
	*timeout = false;

	if (FAILED(hr))
	{
		return false;
	}

	// If we're still holding old frame, destroy it
	if (m_AcquiredDesktopImage)
	{
		m_AcquiredDesktopImage = nullptr;
	}

	// Get the IDXGIResource interface
	hr = desktopResource.As(&m_AcquiredDesktopImage);
	desktopResource = nullptr;
	if (FAILED(hr))
	{
		return false;
	}

	// Get metadata
	if (frameInfo.TotalMetadataBufferSize)
	{
		// Check the size of the buffer
		if (frameInfo.TotalMetadataBufferSize > m_Metadata.size())
		{
			m_Metadata.resize(frameInfo.TotalMetadataBufferSize);
		}

		UINT bufferSize = frameInfo.TotalMetadataBufferSize;

		// Get move rectangles
		m_MoveRects = reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(&m_Metadata[0]);
		hr = m_Duplication->GetFrameMoveRects(bufferSize, reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(&m_Metadata[0]), &bufferSize);
		if (FAILED(hr))
		{
			return false;
		}
		m_MoveCount = bufferSize / sizeof(DXGI_OUTDUPL_MOVE_RECT);

		// Get dirty rectangles
		m_DirtyRects = reinterpret_cast<RECT*>(&m_Metadata[bufferSize]);
		bufferSize = frameInfo.TotalMetadataBufferSize - bufferSize;
		hr = m_Duplication->GetFrameDirtyRects(bufferSize, reinterpret_cast<RECT*>(m_DirtyRects), &bufferSize);
		if (FAILED(hr))
		{
			return false;
		}
		m_DirtyCount = bufferSize / sizeof(RECT);
	}

	return true;
}

bool DuplicationManager::ReleaseFrame()
{
	HRESULT hr = m_Duplication->ReleaseFrame();
	if (FAILED(hr))
	{
		return false;
	}

	if (m_AcquiredDesktopImage)
	{
		m_AcquiredDesktopImage = nullptr;
	}

	m_DirtyRects = nullptr;
	m_DirtyCount = 0;
	m_MoveRects = nullptr;
	m_MoveCount = 0;

	return true;
}

ComPtr<ID3D11Texture2D> DuplicationManager::GetTexture() const
{
	return m_AcquiredDesktopImage;
}

int DuplicationManager::GetDirtyCount() const
{
	return m_DirtyCount;
}

RECT* DuplicationManager::GetDirtyRects() const
{
	return m_DirtyRects;
}

int DuplicationManager::GetMoveCount() const
{
	return m_MoveCount;
}

DXGI_OUTDUPL_MOVE_RECT* DuplicationManager::GetMoveRects() const
{
	return m_MoveRects;
}

const DXGI_OUTPUT_DESC& DuplicationManager::GetOutputDesc() const
{
	return m_OutputDesc;
}