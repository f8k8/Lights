#include "stdafx.h"

#include "ScreenProcessor.h"

#include "VertexShader.h"
#include "PixelShader.h"

using namespace Microsoft::WRL;

//
// Constructor NULLs out vars
//
ScreenProcessor::ScreenProcessor() : m_Device(nullptr),
m_DeviceContext(nullptr),
m_MoveSurface(nullptr),
m_VertexShader(nullptr),
m_PixelShader(nullptr),
m_InputLayout(nullptr),
m_RTV(nullptr),
m_SamplerLinear(nullptr),
m_UnexpectedErrorEvent(nullptr),
m_ExpectedErrorEvent(nullptr)
{
}

//
// Destructor calls CleanRefs to destroy everything
//
ScreenProcessor::~ScreenProcessor()
{
	
}

//
// Initialize D3D variables
//
bool ScreenProcessor::Initialise(HANDLE unexpectedErrorEvent, HANDLE expectedErrorEvent)
{
	HRESULT hr = S_OK;

	m_UnexpectedErrorEvent = unexpectedErrorEvent;
	m_ExpectedErrorEvent = expectedErrorEvent;

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
			D3D11_SDK_VERSION, &m_Device, &featureLevel, &m_DeviceContext);
		if (SUCCEEDED(hr))
		{
			// Device creation success, no need to loop anymore
			break;
		}
	}
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, nullptr, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

	// VERTEX shader
	UINT size = ARRAYSIZE(g_VS);
	hr = m_Device->CreateVertexShader(g_VS, size, nullptr, &m_VertexShader);
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

	// Input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	UINT numElements = ARRAYSIZE(layout);
	hr = m_Device->CreateInputLayout(layout, numElements, g_VS, size, &m_InputLayout);
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}
	m_DeviceContext->IASetInputLayout(m_InputLayout.Get());

	// Pixel shader
	size = ARRAYSIZE(g_PS);
	hr = m_Device->CreatePixelShader(g_PS, size, nullptr, &m_PixelShader);
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
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
	hr = m_Device->CreateSamplerState(&SampDesc, &m_SamplerLinear);
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

	return true;
}

Microsoft::WRL::ComPtr<ID3D11Device> ScreenProcessor::GetDevice() const
{
	return m_Device;
}

//
// Process a given frame and its metadata
//
bool ScreenProcessor::ProcessFrame(const DuplicationManager& duplicationManager, ComPtr<ID3D11Texture2D> sharedSurface, int offsetX, int offsetY)
{
	// Process dirties and moves
	D3D11_TEXTURE2D_DESC textureDescription;
	duplicationManager.GetTexture()->GetDesc(&textureDescription);

	// Process the moves first
	if (duplicationManager.GetMoveCount())
	{
		if (!ProcessMoves(sharedSurface, duplicationManager.GetMoveRects(), duplicationManager.GetMoveCount(), offsetX, offsetY, duplicationManager.GetOutputDesc(), textureDescription.Width, textureDescription.Height))
		{
			return false;
		}
	}

	// Now process the dirties
	if (duplicationManager.GetDirtyCount())
	{
		if (!ProcessDirty(duplicationManager.GetTexture(), sharedSurface, duplicationManager.GetDirtyRects(), duplicationManager.GetDirtyCount(), offsetX, offsetY, duplicationManager.GetOutputDesc()))
		{
			return false;
		}
	}

	return true;
}

//
// Copy move rectangles
//
bool ScreenProcessor::ProcessMoves(ComPtr<ID3D11Texture2D> sharedSurface, DXGI_OUTDUPL_MOVE_RECT* moveRects, unsigned int moveCount, int offsetX, int offsetY, const DXGI_OUTPUT_DESC& desktopDescription, int texWidth, int texHeight)
{
	D3D11_TEXTURE2D_DESC sharedDescription;
	sharedSurface->GetDesc(&sharedDescription);

	// Make sure we have temporary surface for processing moves
	// As CopySubresourceRegion requires source & destination resources to be different
	if (!m_MoveSurface)
	{
		D3D11_TEXTURE2D_DESC moveDescription;
		moveDescription = sharedDescription;
		moveDescription.Width = desktopDescription.DesktopCoordinates.right - desktopDescription.DesktopCoordinates.left;
		moveDescription.Height = desktopDescription.DesktopCoordinates.bottom - desktopDescription.DesktopCoordinates.top;
		moveDescription.BindFlags = D3D11_BIND_RENDER_TARGET;
		moveDescription.MiscFlags = 0;
		HRESULT hr = m_Device->CreateTexture2D(&moveDescription, nullptr, &m_MoveSurface);
		if (FAILED(hr))
		{
			SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
			return false;
		}
	}

	for (unsigned int moveRectIndex = 0; moveRectIndex < moveCount; ++moveRectIndex)
	{
		RECT srcRect;
		RECT destRect;

		ConvertMoveRect(&srcRect, &destRect, desktopDescription, moveRects[moveRectIndex], texWidth, texHeight);

		// Copy rect out of shared surface to our temporary one
		D3D11_BOX box;
		box.left = srcRect.left + desktopDescription.DesktopCoordinates.left - offsetX;
		box.top = srcRect.top + desktopDescription.DesktopCoordinates.top - offsetY;
		box.front = 0;
		box.right = srcRect.right + desktopDescription.DesktopCoordinates.left - offsetX;
		box.bottom = srcRect.bottom + desktopDescription.DesktopCoordinates.top - offsetY;
		box.back = 1;
		m_DeviceContext->CopySubresourceRegion(m_MoveSurface.Get(), 0, srcRect.left, srcRect.top, 0, sharedSurface.Get(), 0, &box);

		// Copy back to shared surface
		box.left = srcRect.left;
		box.top = srcRect.top;
		box.front = 0;
		box.right = srcRect.right;
		box.bottom = srcRect.bottom;
		box.back = 1;
		m_DeviceContext->CopySubresourceRegion(sharedSurface.Get(), 0, destRect.left + desktopDescription.DesktopCoordinates.left - offsetX, destRect.top + desktopDescription.DesktopCoordinates.top - offsetY, 0, m_MoveSurface.Get(), 0, &box);
	}

	return true;
}


// Converts a DX Move Rect into a source & destination RECT
void ScreenProcessor::ConvertMoveRect(RECT* sourceRect, RECT* destRect, const DXGI_OUTPUT_DESC& desktopDescription, const DXGI_OUTDUPL_MOVE_RECT& moveRect, int texWidth, int texHeight)
{
	switch (desktopDescription.Rotation)
	{
	case DXGI_MODE_ROTATION_UNSPECIFIED:
	case DXGI_MODE_ROTATION_IDENTITY:
	{
		sourceRect->left = moveRect.SourcePoint.x;
		sourceRect->top = moveRect.SourcePoint.y;
		sourceRect->right = moveRect.SourcePoint.x + moveRect.DestinationRect.right - moveRect.DestinationRect.left;
		sourceRect->bottom = moveRect.SourcePoint.y + moveRect.DestinationRect.bottom - moveRect.DestinationRect.top;

		*destRect = moveRect.DestinationRect;
		
	}
	break;

	case DXGI_MODE_ROTATION_ROTATE90:
	{
		sourceRect->left = texHeight - (moveRect.SourcePoint.y + moveRect.DestinationRect.bottom - moveRect.DestinationRect.top);
		sourceRect->top = moveRect.SourcePoint.x;
		sourceRect->right = texHeight - moveRect.SourcePoint.y;
		sourceRect->bottom = moveRect.SourcePoint.x + moveRect.DestinationRect.right - moveRect.DestinationRect.left;

		destRect->left = texHeight - moveRect.DestinationRect.bottom;
		destRect->top = moveRect.DestinationRect.left;
		destRect->right = texHeight - moveRect.DestinationRect.top;
		destRect->bottom = moveRect.DestinationRect.right;
		
	}
	break;

	case DXGI_MODE_ROTATION_ROTATE180:
	{
		sourceRect->left = texWidth - (moveRect.SourcePoint.x + moveRect.DestinationRect.right - moveRect.DestinationRect.left);
		sourceRect->top = texHeight - (moveRect.SourcePoint.y + moveRect.DestinationRect.bottom - moveRect.DestinationRect.top);
		sourceRect->right = texWidth - moveRect.SourcePoint.x;
		sourceRect->bottom = texHeight - moveRect.SourcePoint.y;

		destRect->left = texWidth - moveRect.DestinationRect.right;
		destRect->top = texHeight - moveRect.DestinationRect.bottom;
		destRect->right = texWidth - moveRect.DestinationRect.left;
		destRect->bottom = texHeight - moveRect.DestinationRect.top;
	}
	break;

	case DXGI_MODE_ROTATION_ROTATE270:
		{
			sourceRect->left = moveRect.SourcePoint.x;
			sourceRect->top = texWidth - (moveRect.SourcePoint.x + moveRect.DestinationRect.right - moveRect.DestinationRect.left);
			sourceRect->right = moveRect.SourcePoint.y + moveRect.DestinationRect.bottom - moveRect.DestinationRect.top;
			sourceRect->bottom = texWidth - moveRect.SourcePoint.x;

			destRect->left = moveRect.DestinationRect.top;
			destRect->top = texWidth - moveRect.DestinationRect.right;
			destRect->right = moveRect.DestinationRect.bottom;
			destRect->bottom = texWidth - moveRect.DestinationRect.left;
		}
		break;

	default:
		{
			RtlZeroMemory(destRect, sizeof(RECT));
			RtlZeroMemory(sourceRect, sizeof(RECT));
		}
		break;
	}
}

//
// Copies dirty rectangles
//
bool ScreenProcessor::ProcessDirty(ComPtr<ID3D11Texture2D> sourceSurface, ComPtr<ID3D11Texture2D> sharedSurface, RECT* dirtyRects, unsigned int dirtyCount, int offsetX, int offsetY, const DXGI_OUTPUT_DESC& desktopDescription)
{
	HRESULT hr;

	D3D11_TEXTURE2D_DESC sharedDescription;
	sharedSurface->GetDesc(&sharedDescription);

	D3D11_TEXTURE2D_DESC sourceDescription;
	sourceSurface->GetDesc(&sourceDescription);

	// Make sure we have a render target view
	if (!m_RTV)
	{
		hr = m_Device->CreateRenderTargetView(sharedSurface.Get(), nullptr, &m_RTV);
		if (FAILED(hr))
		{
			SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
			return false;
		}
	}
	
	// Create a shader resource view for the source
	D3D11_SHADER_RESOURCE_VIEW_DESC shaderDesc;
	shaderDesc.Format = sourceDescription.Format;
	shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shaderDesc.Texture2D.MostDetailedMip = sourceDescription.MipLevels - 1;
	shaderDesc.Texture2D.MipLevels = sourceDescription.MipLevels;

	ComPtr<ID3D11ShaderResourceView> shaderResource = nullptr;
	hr = m_Device->CreateShaderResourceView(sourceSurface.Get(), &shaderDesc, &shaderResource);
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

	// Set up shader / blending states
	FLOAT blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
	m_DeviceContext->OMSetBlendState(nullptr, blendFactor, 0xFFFFFFFF);
	m_DeviceContext->OMSetRenderTargets(1, m_RTV.GetAddressOf(), nullptr);
	m_DeviceContext->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	m_DeviceContext->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	m_DeviceContext->PSSetShaderResources(0, 1, shaderResource.GetAddressOf());
	m_DeviceContext->PSSetSamplers(0, 1, m_SamplerLinear.GetAddressOf());
	m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Create space for vertices for the dirty rects if the current space isn't large enough
	// 2 triangles per rect = 6 verts
	unsigned int numVerticesPerRect = 6;
	unsigned int numVertices = numVerticesPerRect * dirtyCount;
	if (numVertices == 0)
	{
		return true;
	}
	if (m_DirtyRectVertices.size() < numVertices)
	{
		m_DirtyRectVertices.resize(numVertices);
	}

	// Fill them in
	Vertex* vertex = &m_DirtyRectVertices[0];
	for (unsigned int rectIndex = 0; rectIndex < dirtyCount; ++rectIndex, vertex += numVerticesPerRect)
	{
		BuildDirtyVerts(vertex, dirtyRects[rectIndex], offsetX, offsetY, desktopDescription, sharedDescription, sourceDescription);
	}

	// Create vertex buffer
	D3D11_BUFFER_DESC bufferDesc;
	RtlZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = numVertices * sizeof(Vertex);
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA initData;
	RtlZeroMemory(&initData, sizeof(initData));
	initData.pSysMem = &m_DirtyRectVertices[0];

	ComPtr<ID3D11Buffer> vertexBuffer = nullptr;
	hr = m_Device->CreateBuffer(&bufferDesc, &initData, &vertexBuffer);
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}
	unsigned int stride = sizeof(Vertex);
	unsigned int offset = 0;
	m_DeviceContext->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);

	// Setup the viewport
	D3D11_VIEWPORT viewport;
	viewport.Width = static_cast<FLOAT>(sharedDescription.Width);
	viewport.Height = static_cast<FLOAT>(sharedDescription.Height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	m_DeviceContext->RSSetViewports(1, &viewport);

	// Draw the vertices
	m_DeviceContext->Draw(numVertices, 0);

	// Cleanup
	vertexBuffer = nullptr;
	shaderResource = nullptr;

	return true;
}

//
// Sets up vertices for dirty rects for rotated desktops
//
void ScreenProcessor::BuildDirtyVerts(Vertex* vertices, const RECT& dirtyRect, int offsetX, int offsetY, const DXGI_OUTPUT_DESC& desktopDescription, const D3D11_TEXTURE2D_DESC& sharedDescription, const D3D11_TEXTURE2D_DESC& sourceDescription)
{
	int centerX = sharedDescription.Width / 2;
	int centerY = sharedDescription.Height / 2;

	int width = desktopDescription.DesktopCoordinates.right - desktopDescription.DesktopCoordinates.left;
	int height = desktopDescription.DesktopCoordinates.bottom - desktopDescription.DesktopCoordinates.top;

	// Rotation compensated destination rect
	RECT destDirty = dirtyRect;

	// Set appropriate coordinates compensated for rotation
	switch (desktopDescription.Rotation)
	{
	case DXGI_MODE_ROTATION_ROTATE90:
		{
			destDirty.left = width - dirtyRect.bottom;
			destDirty.top = dirtyRect.left;
			destDirty.right = width - dirtyRect.top;
			destDirty.bottom = dirtyRect.right;

			vertices[0].TexCoord = DirectX::XMFLOAT2(dirtyRect.right / static_cast<FLOAT>(sourceDescription.Width), dirtyRect.bottom / static_cast<FLOAT>(sourceDescription.Height));
			vertices[1].TexCoord = DirectX::XMFLOAT2(dirtyRect.left / static_cast<FLOAT>(sourceDescription.Width), dirtyRect.bottom / static_cast<FLOAT>(sourceDescription.Height));
			vertices[2].TexCoord = DirectX::XMFLOAT2(dirtyRect.right / static_cast<FLOAT>(sourceDescription.Width), dirtyRect.top / static_cast<FLOAT>(sourceDescription.Height));
			vertices[5].TexCoord = DirectX::XMFLOAT2(dirtyRect.left / static_cast<FLOAT>(sourceDescription.Width), dirtyRect.top / static_cast<FLOAT>(sourceDescription.Height));
		}
		break;

	case DXGI_MODE_ROTATION_ROTATE180:
		{
			destDirty.left = width - dirtyRect.right;
			destDirty.top = height - dirtyRect.bottom;
			destDirty.right = width - dirtyRect.left;
			destDirty.bottom = height - dirtyRect.top;

			vertices[0].TexCoord = DirectX::XMFLOAT2(dirtyRect.right / static_cast<FLOAT>(sourceDescription.Width), dirtyRect.top / static_cast<FLOAT>(sourceDescription.Height));
			vertices[1].TexCoord = DirectX::XMFLOAT2(dirtyRect.right / static_cast<FLOAT>(sourceDescription.Width), dirtyRect.bottom / static_cast<FLOAT>(sourceDescription.Height));
			vertices[2].TexCoord = DirectX::XMFLOAT2(dirtyRect.left / static_cast<FLOAT>(sourceDescription.Width), dirtyRect.top / static_cast<FLOAT>(sourceDescription.Height));
			vertices[5].TexCoord = DirectX::XMFLOAT2(dirtyRect.left / static_cast<FLOAT>(sourceDescription.Width), dirtyRect.bottom / static_cast<FLOAT>(sourceDescription.Height));
		
		}
		break;

		case DXGI_MODE_ROTATION_ROTATE270:
		{
			destDirty.left = dirtyRect.top;
			destDirty.top = height - dirtyRect.right;
			destDirty.right = dirtyRect.bottom;
			destDirty.bottom = height - dirtyRect.left;

			vertices[0].TexCoord = DirectX::XMFLOAT2(dirtyRect.left / static_cast<FLOAT>(sourceDescription.Width), dirtyRect.top / static_cast<FLOAT>(sourceDescription.Height));
			vertices[1].TexCoord = DirectX::XMFLOAT2(dirtyRect.right / static_cast<FLOAT>(sourceDescription.Width), dirtyRect.top / static_cast<FLOAT>(sourceDescription.Height));
			vertices[2].TexCoord = DirectX::XMFLOAT2(dirtyRect.left / static_cast<FLOAT>(sourceDescription.Width), dirtyRect.bottom / static_cast<FLOAT>(sourceDescription.Height));
			vertices[5].TexCoord = DirectX::XMFLOAT2(dirtyRect.right / static_cast<FLOAT>(sourceDescription.Width), dirtyRect.bottom / static_cast<FLOAT>(sourceDescription.Height));
		}
		break;

	default:
		assert(false); // drop through
	case DXGI_MODE_ROTATION_UNSPECIFIED:
	case DXGI_MODE_ROTATION_IDENTITY:
		{
			vertices[0].TexCoord = DirectX::XMFLOAT2(dirtyRect.left / static_cast<FLOAT>(sourceDescription.Width), dirtyRect.bottom / static_cast<FLOAT>(sourceDescription.Height));
			vertices[1].TexCoord = DirectX::XMFLOAT2(dirtyRect.left / static_cast<FLOAT>(sourceDescription.Width), dirtyRect.top / static_cast<FLOAT>(sourceDescription.Height));
			vertices[2].TexCoord = DirectX::XMFLOAT2(dirtyRect.right / static_cast<FLOAT>(sourceDescription.Width), dirtyRect.bottom / static_cast<FLOAT>(sourceDescription.Height));
			vertices[5].TexCoord = DirectX::XMFLOAT2(dirtyRect.right / static_cast<FLOAT>(sourceDescription.Width), dirtyRect.top / static_cast<FLOAT>(sourceDescription.Height));
			
		}
		break;
	}

	// Set positions
	// First triangle
	vertices[0].Pos = DirectX::XMFLOAT3((destDirty.left + desktopDescription.DesktopCoordinates.left - offsetX - centerX) / static_cast<FLOAT>(centerX),
										-1 * (destDirty.bottom + desktopDescription.DesktopCoordinates.top - offsetY - centerY) / static_cast<FLOAT>(centerY),
										0.0f);
	vertices[1].Pos = DirectX::XMFLOAT3((destDirty.left + desktopDescription.DesktopCoordinates.left - offsetX - centerX) / static_cast<FLOAT>(centerX),
										-1 * (destDirty.top + desktopDescription.DesktopCoordinates.top - offsetY - centerY) / static_cast<FLOAT>(centerY),
										0.0f);
	vertices[2].Pos = DirectX::XMFLOAT3((destDirty.right + desktopDescription.DesktopCoordinates.left - offsetX - centerX) / static_cast<FLOAT>(centerX),
										-1 * (destDirty.bottom + desktopDescription.DesktopCoordinates.top - offsetY - centerY) / static_cast<FLOAT>(centerY),
										0.0f);
	
	// Second triangle
	vertices[3].Pos = vertices[2].Pos;
	vertices[4].Pos = vertices[1].Pos;
	vertices[5].Pos = DirectX::XMFLOAT3((destDirty.right + desktopDescription.DesktopCoordinates.left - offsetX - centerX) / static_cast<FLOAT>(centerX),
										-1 * (destDirty.top + desktopDescription.DesktopCoordinates.top - offsetY - centerY) / static_cast<FLOAT>(centerY),
										0.0f);

	// Remaining texture coordinates
	vertices[3].TexCoord = vertices[2].TexCoord;
	vertices[4].TexCoord = vertices[1].TexCoord;
}

