#pragma once

#include <vector>

// For handling the duplication of a single display
class DuplicationManager
{
public:
	DuplicationManager();
	~DuplicationManager();

	bool Initialise(Microsoft::WRL::ComPtr<ID3D11Device> device, unsigned int outputIndex, HANDLE unexpectedErrorEvent, HANDLE expectedErrorEvent);
	bool GetFrame(bool* timeout);
	bool ReleaseFrame();
	Microsoft::WRL::ComPtr<ID3D11Texture2D> GetTexture() const;

	int GetDirtyCount() const;
	RECT* GetDirtyRects() const;
	int GetMoveCount() const;
	DXGI_OUTDUPL_MOVE_RECT* GetMoveRects() const;

	const DXGI_OUTPUT_DESC& GetOutputDesc() const;

private:
	HANDLE					m_UnexpectedErrorEvent;
	HANDLE					m_ExpectedErrorEvent;

	// D3D device
	Microsoft::WRL::ComPtr<ID3D11Device>					m_Device;

	// Duplication interfacte
	Microsoft::WRL::ComPtr<IDXGIOutputDuplication>			m_Duplication;

	// The last duplicated frame
	Microsoft::WRL::ComPtr<ID3D11Texture2D>				m_AcquiredDesktopImage;

	// The last duplicated frame info
	DXGI_OUTDUPL_FRAME_INFO			m_FrameInfo;

	// The last duplicated metadata
	std::vector<BYTE>				m_Metadata;

	// These are raw pointers into the m_Metadata array
	RECT*							m_DirtyRects;
	unsigned int					m_DirtyCount;
	DXGI_OUTDUPL_MOVE_RECT*			m_MoveRects;
	unsigned int					m_MoveCount;

	// Output description
	DXGI_OUTPUT_DESC				m_OutputDesc;
	
};