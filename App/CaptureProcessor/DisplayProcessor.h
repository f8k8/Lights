#pragma once

class DisplayProcessor
{
public:
	DisplayProcessor();
	~DisplayProcessor();

	bool InitOutput(int singleOutput, int lightTextureWidth, int lightTextureHeight);
	void CleanRefs();
	HANDLE GetSharedSurfaceHandle();

	int GetOutputCount() const;
	const RECT& GetDesktopBounds() const;

	bool ProcessFrame();

private:
	bool CreateRenderTarget(unsigned int width, unsigned int height);
	void SetViewPort(unsigned int width, unsigned int height);
	bool InitShaders();
	bool CreateSharedSurface(int singleOutput);
	bool DrawFrame();

private:
	int						m_OutputCount;
	RECT					m_DesktopBounds;

	// The device
	Microsoft::WRL::ComPtr<ID3D11Device>	m_Device;
	Microsoft::WRL::ComPtr<IDXGIFactory2>			m_Factory;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>	m_DeviceContext;
	
	// Resources for rendering
	int						m_LightSurfaceWidth;
	int						m_LightSurfaceHeight;
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