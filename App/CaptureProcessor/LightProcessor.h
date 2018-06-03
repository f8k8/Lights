#pragma once

#include <vector>

// Creates and processes the shared surface to extract light values
class LightProcessor
{
public:
	LightProcessor();
	~LightProcessor();

	bool Initialise(int singleOutput, int lightTextureWidth, int lightTextureHeight, HANDLE unexpectedErrorEvent, HANDLE expectedErrorEvent);
	HANDLE GetSharedSurfaceHandle();

	void SetColourScale(float r, float g, float b);
	int GetOutputCount() const;
	const RECT& GetDesktopBounds() const;

	bool ProcessFrame();

	const std::vector<__int32> GetLightValues() const;

private:
	bool CreateRenderTarget(unsigned int width, unsigned int height);
	void SetViewPort(unsigned int width, unsigned int height);
	bool InitShaders();
	bool CreateSharedSurface(int singleOutput);

private:
	std::vector<__int32>	m_LightValues;

	int						m_OutputCount;
	RECT					m_DesktopBounds;

	HANDLE					m_UnexpectedErrorEvent;
	HANDLE					m_ExpectedErrorEvent;

	// The device
	Microsoft::WRL::ComPtr<ID3D11Device>	m_Device;
	Microsoft::WRL::ComPtr<IDXGIFactory2>			m_Factory;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>	m_DeviceContext;
	
	// Resources for rendering
	int						m_LightSurfaceWidth;
	int						m_LightSurfaceHeight;
	float					m_ColourScale[3];
	Microsoft::WRL::ComPtr<ID3D11Texture2D>		m_StagingLightSurface;
	Microsoft::WRL::ComPtr<ID3D11Texture2D>		m_LightSurface;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>	m_RTV;
	Microsoft::WRL::ComPtr<ID3D11SamplerState>		m_SamplerLinear;
	Microsoft::WRL::ComPtr<ID3D11BlendState>		m_BlendState;
	Microsoft::WRL::ComPtr<ID3D11VertexShader>		m_VertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>		m_PixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>		m_InputLayout;

	// Shared surface for compositing onto
	Microsoft::WRL::ComPtr<ID3D11Texture2D>		m_SharedSurface;

	// Mutex for accessing the shared surface
	Microsoft::WRL::ComPtr<IDXGIKeyedMutex>		m_KeyMutex;
};