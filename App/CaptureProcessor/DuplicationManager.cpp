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
	m_MoveCount(0),
	m_UnexpectedErrorEvent(nullptr),
	m_ExpectedErrorEvent(nullptr)
{
	RtlZeroMemory(&m_OutputDesc, sizeof(m_OutputDesc));
	RtlZeroMemory(&m_FrameInfo, sizeof(m_FrameInfo));
}

DuplicationManager::~DuplicationManager()
{
	if (m_AcquiredDesktopImage)
	{
		// Release the last frame
		m_Duplication->ReleaseFrame();
	}
}

bool DuplicationManager::Initialise(ComPtr<ID3D11Device> device, unsigned int outputIndex, HANDLE unexpectedErrorEvent, HANDLE expectedErrorEvent)
{
	// Add a reference to the device
	m_Device = device;

	m_UnexpectedErrorEvent = unexpectedErrorEvent;
	m_ExpectedErrorEvent = expectedErrorEvent;

	// Get DXGI device
	ComPtr<IDXGIDevice> dxgiDevice = nullptr;
	HRESULT hr = m_Device.As(&dxgiDevice);
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, nullptr, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

	// Get DXGI adapter
	ComPtr<IDXGIAdapter> dxgiAdapter = nullptr;
	hr = dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf());
	dxgiDevice = nullptr;
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

	// Get the output interface
	ComPtr<IDXGIOutput> dxgiOutput = nullptr;
	hr = dxgiAdapter->EnumOutputs(outputIndex, &dxgiOutput);
	dxgiAdapter = nullptr;
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, EnumOutputsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

	dxgiOutput->GetDesc(&m_OutputDesc);

	// QI for the Output 1 interface
	ComPtr<IDXGIOutput1> dxgiOutput1 = nullptr;
	hr = dxgiOutput.As(&dxgiOutput1);
	dxgiOutput = nullptr;
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, nullptr, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

	// Create desktop duplication
	hr = dxgiOutput1->DuplicateOutput(m_Device.Get(), &m_Duplication);
	dxgiOutput1 = nullptr;
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, CreateDuplicationExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
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

	if (m_AcquiredDesktopImage)
	{
		m_AcquiredDesktopImage = nullptr;

		HRESULT hr = m_Duplication->ReleaseFrame();
		if (FAILED(hr))
		{
			SetAppropriateEvent(hr, FrameInfoExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
			return false;
		}
	}

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
		SetAppropriateEvent(hr, FrameInfoExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

	// Get the IDXGIResource interface
	hr = desktopResource.As(&m_AcquiredDesktopImage);
	desktopResource = nullptr;
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, nullptr, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		m_Duplication->ReleaseFrame();
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
			m_Duplication->ReleaseFrame();
			SetAppropriateEvent(hr, FrameInfoExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
			return false;
		}
		m_MoveCount = bufferSize / sizeof(DXGI_OUTDUPL_MOVE_RECT);

		// Get dirty rectangles
		m_DirtyRects = reinterpret_cast<RECT*>(&m_Metadata[bufferSize]);
		bufferSize = frameInfo.TotalMetadataBufferSize - bufferSize;
		hr = m_Duplication->GetFrameDirtyRects(bufferSize, reinterpret_cast<RECT*>(m_DirtyRects), &bufferSize);
		if (FAILED(hr))
		{
			m_Duplication->ReleaseFrame();
			SetAppropriateEvent(hr, FrameInfoExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
			return false;
		}
		m_DirtyCount = bufferSize / sizeof(RECT);
	}

	return true;
}

bool DuplicationManager::ReleaseFrame()
{
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