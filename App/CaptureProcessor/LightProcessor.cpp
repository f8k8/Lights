#include "stdafx.h"

#include "LightProcessor.h"

#include "Vertex.h"

#include "VertexShader.h"
#include "PixelShader.h"
#include "DownsamplePixelShader.h"

#include "DirectXResources.h"

using namespace Microsoft::WRL;

LightProcessor::LightProcessor() : 
	m_Device(nullptr),
	m_Factory(nullptr),
	m_DeviceContext(nullptr),
	m_RTV(nullptr),
	m_SamplerLinear(nullptr),
	m_BlendState(nullptr),
	m_VertexShader(nullptr),
	m_PixelShader(nullptr),
	m_InputLayout(nullptr),
	m_SharedSurface(nullptr),
	m_LightSurface(nullptr),
	m_StagingLightSurface(nullptr),
	m_KeyMutex(nullptr),
	m_LightSurfaceWidth(0),
	m_LightSurfaceHeight(0),
	m_UnexpectedErrorEvent(nullptr),
	m_ExpectedErrorEvent(nullptr)
{
	m_ColourScale[0] = m_ColourScale[1] = m_ColourScale[2] = 1.0f;
}

LightProcessor::~LightProcessor()
{
	
}

bool LightProcessor::Initialise(int singleOutput, int lightTextureWidth, int lightTextureHeight, HANDLE unexpectedErrorEvent, HANDLE expectedErrorEvent)
{
	HRESULT hr;

	m_ExpectedErrorEvent = expectedErrorEvent;
	m_UnexpectedErrorEvent = unexpectedErrorEvent;

	m_LightValues.resize(lightTextureWidth * lightTextureHeight);

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
	for (unsigned int driverTypeIndex = 0; driverTypeIndex < numDriverTypes; ++driverTypeIndex)
	{
		hr = D3D11CreateDevice(nullptr, driverTypes[driverTypeIndex], nullptr, 
#if defined(_DEBUG) 
			D3D11_CREATE_DEVICE_DEBUG,
#else 
			0,
#endif
			featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &m_Device, &featureLevel, &m_DeviceContext);
		if (SUCCEEDED(hr))
		{
			// Device creation succeeded, no need to loop anymore
			break;
		}
	}
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, expectedErrorEvent, unexpectedErrorEvent);
		return false;
	}

	// Get DXGI factory
	ComPtr<IDXGIDevice> dxgiDevice = nullptr;
	hr = m_Device.As(&dxgiDevice);
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, nullptr, expectedErrorEvent, unexpectedErrorEvent);
		return false;
	}

	ComPtr<IDXGIAdapter> dxgiAdapter = nullptr;
	hr = dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf());
	dxgiDevice = nullptr;
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, expectedErrorEvent, unexpectedErrorEvent);
		return false;
	}

	hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), &m_Factory);
	dxgiAdapter = nullptr;
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, expectedErrorEvent, unexpectedErrorEvent);
		return false;
	}

	// Create shared texture
	if (!CreateSharedSurface(singleOutput))
	{
		return false;
	}
	
	// Make new render target view
	m_LightSurfaceWidth = lightTextureWidth;
	m_LightSurfaceHeight = lightTextureHeight;
	if (!CreateRenderTarget(m_LightSurfaceWidth, m_LightSurfaceHeight))
	{
		return false;
	}
	
	// Set view port
	SetViewPort(m_LightSurfaceWidth, m_LightSurfaceHeight);

	// Create the sample state
	D3D11_SAMPLER_DESC samplerDescription;
	RtlZeroMemory(&samplerDescription, sizeof(samplerDescription));
	samplerDescription.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDescription.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDescription.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDescription.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDescription.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDescription.MinLOD = 0;
	samplerDescription.MaxLOD = D3D11_FLOAT32_MAX;
	hr = m_Device->CreateSamplerState(&samplerDescription, &m_SamplerLinear);
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, expectedErrorEvent, unexpectedErrorEvent);
		return false;
	}

	// Create the blend state
	D3D11_BLEND_DESC blendStateDescription;
	blendStateDescription.AlphaToCoverageEnable = FALSE;
	blendStateDescription.IndependentBlendEnable = FALSE;
	blendStateDescription.RenderTarget[0].BlendEnable = TRUE;
	blendStateDescription.RenderTarget[0].SrcBlend = D3D11_BLEND_BLEND_FACTOR;
	blendStateDescription.RenderTarget[0].DestBlend = D3D11_BLEND_INV_BLEND_FACTOR;
	blendStateDescription.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendStateDescription.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendStateDescription.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendStateDescription.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendStateDescription.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = m_Device->CreateBlendState(&blendStateDescription, &m_BlendState);
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, expectedErrorEvent, unexpectedErrorEvent);
		return false;
	}

	// Initialize shaders
	if (!InitShaders())
	{
		return false;
	}
	
	return true;
}

//
// Returns shared handle
//
HANDLE LightProcessor::GetSharedSurfaceHandle()
{
	HANDLE handle = nullptr;

	// QI IDXGIResource interface to synchronized shared surface.
	ComPtr<IDXGIResource> DXGIResource = nullptr;
	HRESULT hr = m_SharedSurface.As(&DXGIResource);
	if (SUCCEEDED(hr))
	{
		// Obtain handle to IDXGIResource object.
		DXGIResource->GetSharedHandle(&handle);
		DXGIResource = nullptr;
	}

	return handle;
}

void LightProcessor::SetColourScale(float r, float g, float b)
{
	m_ColourScale[0] = r;
	m_ColourScale[1] = g;
	m_ColourScale[2] = b;
}

int LightProcessor::GetOutputCount() const
{
	return m_OutputCount;
}

const RECT& LightProcessor::GetDesktopBounds() const
{
	return m_DesktopBounds;
}

bool LightProcessor::ProcessFrame()
{
	HRESULT hr = m_KeyMutex->AcquireSync(0, 100);
	if (hr == static_cast<HRESULT>(WAIT_TIMEOUT))
	{
		// Another thread has the keyed mutex so try again later
		return false;
	}
	else if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

	// Set up the vertices
	Vertex vertices[6];
	vertices[0].Pos = DirectX::XMFLOAT3(-1, -1, 0);
	vertices[0].TexCoord = DirectX::XMFLOAT2(0, 1);
	vertices[1].Pos = DirectX::XMFLOAT3(-1, 1, 0);
	vertices[1].TexCoord = DirectX::XMFLOAT2(0, 0);
	vertices[2].Pos = DirectX::XMFLOAT3(1, -1, 0);
	vertices[2].TexCoord = DirectX::XMFLOAT2(1, 1);

	vertices[3].Pos = DirectX::XMFLOAT3(1, -1, 0);
	vertices[3].TexCoord = DirectX::XMFLOAT2(1, 1);
	vertices[4].Pos = DirectX::XMFLOAT3(-1, 1, 0);
	vertices[4].TexCoord = DirectX::XMFLOAT2(0, 0);
	vertices[5].Pos = DirectX::XMFLOAT3(1, 1, 0);
	vertices[5].TexCoord = DirectX::XMFLOAT2(1, 0);

	// Create vertex buffer
	D3D11_BUFFER_DESC bufferDesc;
	RtlZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = 6 * sizeof(Vertex);
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA initData;
	RtlZeroMemory(&initData, sizeof(initData));
	initData.pSysMem = &vertices[0];

	ComPtr<ID3D11Buffer> vertexBuffer = nullptr;
	hr = m_Device->CreateBuffer(&bufferDesc, &initData, &vertexBuffer);
	if (FAILED(hr))
	{
		m_KeyMutex->ReleaseSync(0);

		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}
	unsigned int stride = sizeof(Vertex);
	unsigned int offset = 0;
	m_DeviceContext->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
	
	// Create a shader resource view for the source
	D3D11_TEXTURE2D_DESC textureDescription;
	m_SharedSurface->GetDesc(&textureDescription);
	D3D11_SHADER_RESOURCE_VIEW_DESC shaderDesc;
	shaderDesc.Format = textureDescription.Format;
	shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shaderDesc.Texture2D.MostDetailedMip = 0;// textureDescription.MipLevels - 1;
	shaderDesc.Texture2D.MipLevels = -1;// textureDescription.MipLevels;

	ComPtr<ID3D11ShaderResourceView> shaderResource = nullptr;
	hr = m_Device->CreateShaderResourceView(m_SharedSurface.Get(), &shaderDesc, &shaderResource);
	if (FAILED(hr))
	{
		vertexBuffer = nullptr;

		m_KeyMutex->ReleaseSync(0);

		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

	m_DeviceContext->GenerateMips(shaderResource.Get());

	// Create the constant buffer
	D3D11_BUFFER_DESC constantBufferDesc;
	RtlZeroMemory(&constantBufferDesc, sizeof(constantBufferDesc));
	constantBufferDesc.ByteWidth = sizeof(DownsamplePixelShaderConstants);
	constantBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantBufferDesc.CPUAccessFlags = 0;
	constantBufferDesc.MiscFlags = 0;
	constantBufferDesc.StructureByteStride = 0;

	// Fill in the subresource data
	DownsamplePixelShaderConstants constants;
	constants.SampleWidth = 1.0f / (float)m_LightSurfaceWidth;
	constants.SampleHeight = 1.0f / (float)m_LightSurfaceHeight;
	constants.ColourScale[0] = m_ColourScale[0];
	constants.ColourScale[1] = m_ColourScale[1];
	constants.ColourScale[2] = m_ColourScale[2];
	D3D11_SUBRESOURCE_DATA constantBufferInitData;
	constantBufferInitData.pSysMem = &constants;
	constantBufferInitData.SysMemPitch = 0;
	constantBufferInitData.SysMemSlicePitch = 0;

	// Create the buffer
	ComPtr<ID3D11Buffer> constantBuffer = nullptr;
	hr = m_Device->CreateBuffer(&constantBufferDesc, &constantBufferInitData, &constantBuffer);
	if (FAILED(hr))
	{
		m_KeyMutex->ReleaseSync(0);

		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

	// Set up shader / blending states
	FLOAT blendFactor[4] = { 0.7f, 0.7f, 0.7f, 1.0f };
	m_DeviceContext->OMSetBlendState(m_BlendState.Get(), blendFactor, 0xFFFFFFFF);
	m_DeviceContext->OMSetRenderTargets(1, m_RTV.GetAddressOf(), nullptr);
	m_DeviceContext->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	m_DeviceContext->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	m_DeviceContext->PSSetShaderResources(0, 1, shaderResource.GetAddressOf());
	m_DeviceContext->PSSetSamplers(0, 1, m_SamplerLinear.GetAddressOf());
	m_DeviceContext->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
	m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	// Draw the vertices
	m_DeviceContext->Draw(6, 0);

	// Cleanup
	vertexBuffer = nullptr;
	shaderResource = nullptr;

	m_KeyMutex->ReleaseSync(0);

	// Copy the light surface to our staging surface so we can read it on the CPU
	m_DeviceContext->CopyResource(m_StagingLightSurface.Get(), m_LightSurface.Get());

	// Now read out our texture data
	D3D11_MAP eMapType = D3D11_MAP_READ;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	hr = m_DeviceContext->Map(m_StagingLightSurface.Get(), 0, eMapType, NULL, &mappedResource);
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return true;
	}

	BYTE* lightBytes = (BYTE*)mappedResource.pData;
	unsigned int rowPitch = mappedResource.RowPitch;

	// Copy the rows to our light values (reversing every odd row)
	BYTE* lightRow = lightBytes;
	__int32* outputValues = &m_LightValues[0];
	for (int rowIndex = 0; rowIndex < m_LightSurfaceHeight; ++rowIndex, lightRow += rowPitch, outputValues += m_LightSurfaceWidth)
	{
		if ((rowIndex % 2) == 1)
		{
			for (int column = 0, textureColumn = m_LightSurfaceWidth - 1; column < m_LightSurfaceWidth; ++column, --textureColumn)
			{
				outputValues[column] = ((__int32*)lightRow)[textureColumn];
			}
		}
		else
		{
			memcpy(outputValues, lightRow, 4 * m_LightSurfaceWidth);
		}
	}

	m_DeviceContext->Unmap(m_StagingLightSurface.Get(), 0);

	return true;
}

const std::vector<__int32> LightProcessor::GetLightValues() const
{
	return m_LightValues;
}

bool LightProcessor::CreateRenderTarget(unsigned int width, unsigned int height)
{
	// Create the texture to render into
	D3D11_TEXTURE2D_DESC lightTextureDescription;
	RtlZeroMemory(&lightTextureDescription, sizeof(D3D11_TEXTURE2D_DESC));
	lightTextureDescription.Width = width;
	lightTextureDescription.Height = height;
	lightTextureDescription.MipLevels = 1;
	lightTextureDescription.ArraySize = 1;
	lightTextureDescription.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	lightTextureDescription.SampleDesc.Count = 1;
	lightTextureDescription.Usage = D3D11_USAGE_DEFAULT;
	lightTextureDescription.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	lightTextureDescription.CPUAccessFlags = 0;
	lightTextureDescription.MiscFlags = 0;
	HRESULT hr = m_Device->CreateTexture2D(&lightTextureDescription, nullptr, &m_LightSurface);
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

	// Create a render target view so we can render our scaled screen down
	hr = m_Device->CreateRenderTargetView(m_LightSurface.Get(), nullptr, &m_RTV);
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

	// Set new render target
	m_DeviceContext->OMSetRenderTargets(1, m_RTV.GetAddressOf(), nullptr);

	// Create the staging light surface so we can get CPU access
	D3D11_TEXTURE2D_DESC stagingTextureDescription;
	RtlZeroMemory(&stagingTextureDescription, sizeof(D3D11_TEXTURE2D_DESC));
	stagingTextureDescription.Width = width;
	stagingTextureDescription.Height = height;
	stagingTextureDescription.MipLevels = 1;
	stagingTextureDescription.ArraySize = 1;
	stagingTextureDescription.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	stagingTextureDescription.SampleDesc.Count = 1;
	stagingTextureDescription.Usage = D3D11_USAGE_STAGING;
	stagingTextureDescription.BindFlags = 0;
	stagingTextureDescription.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingTextureDescription.MiscFlags = 0;
	hr = m_Device->CreateTexture2D(&stagingTextureDescription, nullptr, &m_StagingLightSurface);
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}
	
	return true;
}

void LightProcessor::SetViewPort(unsigned int width, unsigned int height)
{
	D3D11_VIEWPORT viewport;
	viewport.Width = static_cast<FLOAT>(width);
	viewport.Height = static_cast<FLOAT>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	m_DeviceContext->RSSetViewports(1, &viewport);
}

bool LightProcessor::InitShaders()
{
	HRESULT hr;

	UINT size = ARRAYSIZE(g_VS);
	hr = m_Device->CreateVertexShader(g_VS, size, nullptr, &m_VertexShader);
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

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

	size = ARRAYSIZE(g_DownSamplePS);
	hr = m_Device->CreatePixelShader(g_DownSamplePS, size, nullptr, &m_PixelShader);
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

	return true;
}

bool LightProcessor::CreateSharedSurface(int singleOutput)
{
	HRESULT hr;

	// Get DXGI resources
	ComPtr<IDXGIDevice> dxgiDevice = nullptr;
	hr = m_Device.As(&dxgiDevice);
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

	ComPtr<IDXGIAdapter> dxgiAdapter = nullptr;
	hr = dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf());
	dxgiDevice = nullptr;
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

	// Set initial values so that we always catch the right coordinates
	m_DesktopBounds.left = INT_MAX;
	m_DesktopBounds.right = INT_MIN;
	m_DesktopBounds.top = INT_MAX;
	m_DesktopBounds.bottom = INT_MIN;

	ComPtr<IDXGIOutput> dxgiOutput = nullptr;

	// Figure out right dimensions for full size desktop texture and # of outputs to duplicate
	if (singleOutput < 0)
	{
		// Use all available outputs
		hr = S_OK;
		for (m_OutputCount = 0; SUCCEEDED(hr); ++m_OutputCount)
		{
			hr = dxgiAdapter->EnumOutputs(m_OutputCount, &dxgiOutput);
			if (dxgiOutput && (hr != DXGI_ERROR_NOT_FOUND))
			{
				DXGI_OUTPUT_DESC desktopDescription;
				dxgiOutput->GetDesc(&desktopDescription);
				dxgiOutput = nullptr;

				m_DesktopBounds.left = min(desktopDescription.DesktopCoordinates.left, m_DesktopBounds.left);
				m_DesktopBounds.top = min(desktopDescription.DesktopCoordinates.top, m_DesktopBounds.top);
				m_DesktopBounds.right = max(desktopDescription.DesktopCoordinates.right, m_DesktopBounds.right);
				m_DesktopBounds.bottom = max(desktopDescription.DesktopCoordinates.bottom, m_DesktopBounds.bottom);
			}
		}
		--m_OutputCount;
	}
	else
	{
		// Use a single output
		hr = dxgiAdapter->EnumOutputs(singleOutput, &dxgiOutput);
		if (FAILED(hr))
		{
			dxgiAdapter = nullptr;
			SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
			return false;
		}
		DXGI_OUTPUT_DESC desktopDescription;
		dxgiOutput->GetDesc(&desktopDescription);
		m_DesktopBounds = desktopDescription.DesktopCoordinates;

		if (dxgiOutput)
		{
			dxgiOutput = nullptr;
		}

		m_OutputCount = 1;
	}
	
	dxgiAdapter = nullptr;
	
	if (m_OutputCount <= 0)
	{
		// We could not find any outputs, the system must be in a transition so return expected error
		// so we will attempt to recreate
		return false;
	}

	// Create shared texture for all duplication threads to draw into
	D3D11_TEXTURE2D_DESC desktopTextureDescription;
	RtlZeroMemory(&desktopTextureDescription, sizeof(D3D11_TEXTURE2D_DESC));
	desktopTextureDescription.Width = m_DesktopBounds.right - m_DesktopBounds.left;
	desktopTextureDescription.Height = m_DesktopBounds.bottom - m_DesktopBounds.top;
	desktopTextureDescription.MipLevels = 12;
	desktopTextureDescription.ArraySize = 1;
	desktopTextureDescription.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desktopTextureDescription.SampleDesc.Count = 1;
	desktopTextureDescription.Usage = D3D11_USAGE_DEFAULT;
	desktopTextureDescription.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desktopTextureDescription.CPUAccessFlags = 0;
	desktopTextureDescription.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_GENERATE_MIPS;

	hr = m_Device->CreateTexture2D(&desktopTextureDescription, nullptr, &m_SharedSurface);
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

	// Get keyed mutex
	hr = m_SharedSurface.As(&m_KeyMutex);
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

	D3D11_TEXTURE2D_DESC stagingTextureDescription;
	RtlZeroMemory(&stagingTextureDescription, sizeof(D3D11_TEXTURE2D_DESC));
	stagingTextureDescription.Width = m_DesktopBounds.right - m_DesktopBounds.left;
	stagingTextureDescription.Height = m_DesktopBounds.bottom - m_DesktopBounds.top;
	stagingTextureDescription.MipLevels = 1;
	stagingTextureDescription.ArraySize = 1;
	stagingTextureDescription.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	stagingTextureDescription.SampleDesc.Count = 1;
	stagingTextureDescription.Usage = D3D11_USAGE_STAGING;
	stagingTextureDescription.BindFlags = 0;
	stagingTextureDescription.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingTextureDescription.MiscFlags = 0;
	hr = m_Device->CreateTexture2D(&stagingTextureDescription, nullptr, &m_StagingLightSurface);
	if (FAILED(hr))
	{
		SetAppropriateEvent(hr, SystemTransitionsExpectedErrors, m_ExpectedErrorEvent, m_UnexpectedErrorEvent);
		return false;
	}

	return true;
}
