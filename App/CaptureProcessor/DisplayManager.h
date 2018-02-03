#pragma once

#include "DirectXResources.h"

#include "DuplicationManager.h"

#include "Vertex.h"

// For handling the compositing of duplicates onto the main surface
class DisplayManager
{
public:
	DisplayManager();
	~DisplayManager();
	void InitD3D(DirectXResources& data);
	bool ProcessFrame(const DuplicationManager& duplicationManager, Microsoft::WRL::ComPtr<ID3D11Texture2D> sharedSurface, int offsetX, int offsetY);
	void CleanRefs();

private:
	bool ProcessMoves(Microsoft::WRL::ComPtr<ID3D11Texture2D> sharedSurface, DXGI_OUTDUPL_MOVE_RECT* moveRects, unsigned int moveCount, int offsetX, int offsetY, const DXGI_OUTPUT_DESC& desktopDescription, int texWidth, int texHeight);
	void ConvertMoveRect(RECT* sourceRect, RECT* destRect, const DXGI_OUTPUT_DESC& desktopDescription, const DXGI_OUTDUPL_MOVE_RECT& moveRect, int texWidth, int texHeight);

	bool ProcessDirty(Microsoft::WRL::ComPtr<ID3D11Texture2D> sourceSurface, Microsoft::WRL::ComPtr<ID3D11Texture2D> sharedSurface, RECT* dirtyRects, unsigned int dirtyCount, int offsetX, int offsetY, const DXGI_OUTPUT_DESC& desktopDescription);
	void BuildDirtyVerts(Vertex* vertices, const RECT& dirtyRect, int offsetX, int offsetY, const DXGI_OUTPUT_DESC& desktopDescription, const D3D11_TEXTURE2D_DESC& sharedDescription, const D3D11_TEXTURE2D_DESC& sourceDescription);
	
	
private:
	// The D3D device
	Microsoft::WRL::ComPtr<ID3D11Device>			m_Device;

	// The D3D device context
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>	m_DeviceContext;

	// Temporary surface for handling movements
	Microsoft::WRL::ComPtr<ID3D11Texture2D>		m_MoveSurface;

	// Vertex / pixel shaders for drawing updates
	Microsoft::WRL::ComPtr<ID3D11VertexShader>		m_VertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>		m_PixelShader;

	Microsoft::WRL::ComPtr<ID3D11InputLayout>		m_InputLayout;

	// Render target view
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>	m_RTV;

	// Sampler state
	Microsoft::WRL::ComPtr<ID3D11SamplerState>		m_SamplerLinear;

	// Vertex buffer for dirty rects
	std::vector<Vertex>		m_DirtyRectVertices;
};
