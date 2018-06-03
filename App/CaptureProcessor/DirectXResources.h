#pragma once

// Shared DirectX resources
struct DirectXResources
{
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
	Microsoft::WRL::ComPtr<	ID3D11PixelShader> pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerLinear;
};

struct DownsamplePixelShaderConstants
{
	float SampleWidth;
	float SampleHeight;
	float Padding[2];
	float ColourScale[3];
	float Padding2;
};