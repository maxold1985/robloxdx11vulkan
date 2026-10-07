#ifdef _WIN32

#include "Dx11Renderer.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <d3d11.h>
#include <d3dcompiler.h>

#include "ComPtrLite.h"
#include "DirectXMathLite.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <utility>
#include <vector>

namespace
{
	struct Vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
	};

	struct ConstantBuffer
	{
		DirectX::XMFLOAT4X4 world;
		DirectX::XMFLOAT4X4 viewProjection;
		DirectX::XMFLOAT4 color;
	};

	constexpr Vertex CUBE_VERTICES[] = {
		{{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}},
		{{ 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}},
		{{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}},
		{{-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}},

		{{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}},
		{{ 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}},
		{{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}},
		{{-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}},

		{{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}},
		{{-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}},
		{{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}},
		{{-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}},

		{{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}},
		{{ 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}},
		{{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}},
		{{ 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}},

		{{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}},
		{{-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}},
		{{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}},
		{{ 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}},

		{{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}},
		{{-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}},
		{{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}},
		{{ 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}},
	};

	constexpr std::uint16_t CUBE_INDICES[] = {
		 0,  2,  1,  0,  3,  2,
		 4,  5,  6,  4,  6,  7,
		 8,  9, 10,  8, 10, 11,
		12, 14, 13, 12, 15, 14,
		16, 18, 17, 16, 19, 18,
		20, 21, 22, 20, 22, 23,
	};

	const char* VERTEX_SHADER_SOURCE = R"(
cbuffer ObjectBuffer : register(b0)
{
	row_major float4x4 World;
	row_major float4x4 ViewProjection;
	float4 ObjectColor;
};

struct VSInput
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float3 Normal : TEXCOORD0;
};

VSOutput main(VSInput input)
{
	VSOutput output;

	float4 worldPosition =
		mul(float4(input.Position, 1.0f), World);

	output.Position =
		mul(worldPosition, ViewProjection);

	output.Normal =
		normalize(
			mul(
				float4(input.Normal, 0.0f),
				World
			).xyz
		);

	return output;
}
)";

	const char* PIXEL_SHADER_SOURCE = R"(
cbuffer ObjectBuffer : register(b0)
{
	row_major float4x4 World;
	row_major float4x4 ViewProjection;
	float4 ObjectColor;
};

struct PSInput
{
	float4 Position : SV_POSITION;
	float3 Normal : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
	const float3 lightDirection =
		normalize(float3(-0.4f, 0.8f, -0.6f));

	float lighting =
		saturate(
			dot(
				normalize(input.Normal),
				lightDirection
			)
		);

	lighting =
		0.35f +
		lighting * 0.65f;

	return float4(
		ObjectColor.rgb * lighting,
		ObjectColor.a
	);
}
)";

	bool compileShader(
		const char* source,
		const char* entryPoint,
		const char* target,
		ComPtr<ID3DBlob>& bytecode
	)
	{
		ComPtr<ID3DBlob> errors;

		const HRESULT hr =
			D3DCompile(
				source,
				std::strlen(source),
				nullptr,
				nullptr,
				nullptr,
				entryPoint,
				target,
				D3DCOMPILE_ENABLE_STRICTNESS,
				0,
				&bytecode,
				&errors
			);

		if (FAILED(hr))
		{
			if (errors)
			{
				std::fprintf(
					stderr,
					"[DX11 shader] %s\n",
					static_cast<const char*>(
						errors->GetBufferPointer()
					)
				);
			}

			return false;
		}

		return true;
	}

	std::string keyFromVirtualKey(
		WPARAM key
	)
	{
		switch (key)
		{
		case 'W':
			return "W";

		case 'A':
			return "A";

		case 'S':
			return "S";

		case 'D':
			return "D";

		case VK_SPACE:
			return "Space";

		default:
			return {};
		}
	}
}

struct Dx11Renderer::Impl
{
	HWND window = nullptr;
	int width = 1280;
	int height = 720;
	bool running = false;

	InputCallback inputCallback;

	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	ComPtr<IDXGISwapChain> swapChain;

	ComPtr<ID3D11RenderTargetView> renderTargetView;
	ComPtr<ID3D11Texture2D> depthTexture;
	ComPtr<ID3D11DepthStencilView> depthStencilView;

	ComPtr<ID3D11VertexShader> vertexShader;
	ComPtr<ID3D11PixelShader> pixelShader;
	ComPtr<ID3D11InputLayout> inputLayout;

	ComPtr<ID3D11Buffer> vertexBuffer;
	ComPtr<ID3D11Buffer> indexBuffer;
	ComPtr<ID3D11Buffer> gridVertexBuffer;
	ComPtr<ID3D11Buffer> constantBuffer;

	UINT gridVertexCount = 0;

	ComPtr<ID3D11RasterizerState> rasterizerState;
	ComPtr<ID3D11BlendState> blendState;

	static LRESULT CALLBACK windowProc(
		HWND hwnd,
		UINT message,
		WPARAM wParam,
		LPARAM lParam
	)
	{
		Impl* self =
			reinterpret_cast<Impl*>(
				GetWindowLongPtrW(
					hwnd,
					GWLP_USERDATA
				)
			);

		if (message == WM_NCCREATE)
		{
			const auto* create =
				reinterpret_cast<
					CREATESTRUCTW*
				>(lParam);

			self =
				static_cast<Impl*>(
					create->lpCreateParams
				);

			SetWindowLongPtrW(
				hwnd,
				GWLP_USERDATA,
				reinterpret_cast<LONG_PTR>(
					self
				)
			);
		}

		if (self)
		{
			if (
				message == WM_KEYDOWN ||
				message == WM_SYSKEYDOWN
			)
			{
				const bool wasDown =
					(lParam & (1LL << 30)) != 0;

				if (!wasDown)
				{
					const std::string key =
						keyFromVirtualKey(wParam);

					if (
						!key.empty() &&
						self->inputCallback
					)
					{
						self->inputCallback(
							key,
							true
						);
					}
				}

				return 0;
			}

			if (
				message == WM_KEYUP ||
				message == WM_SYSKEYUP
			)
			{
				const std::string key =
					keyFromVirtualKey(wParam);

				if (
					!key.empty() &&
					self->inputCallback
				)
				{
					self->inputCallback(
						key,
						false
					);
				}

				return 0;
			}

			if (message == WM_CLOSE)
			{
				DestroyWindow(hwnd);
				return 0;
			}

			if (message == WM_DESTROY)
			{
				self->running = false;
				PostQuitMessage(0);
				return 0;
			}
		}

		return DefWindowProcW(
			hwnd,
			message,
			wParam,
			lParam
		);
	}

	bool createWindow(
		const wchar_t* title,
		int requestedWidth,
		int requestedHeight
	)
	{
		width = requestedWidth;
		height = requestedHeight;

		const HINSTANCE instance =
			GetModuleHandleW(nullptr);

		const wchar_t* className =
			L"RobloxDx11VulkanWindow";

		WNDCLASSEXW wc{};
		wc.cbSize = sizeof(wc);
		wc.style =
			CS_HREDRAW |
			CS_VREDRAW;
		wc.lpfnWndProc =
			&Impl::windowProc;
		wc.hInstance = instance;
		wc.hCursor =
			LoadCursorW(
				nullptr,
				IDC_ARROW
			);
		wc.lpszClassName =
			className;

		RegisterClassExW(&wc);

		RECT rect{
			0,
			0,
			width,
			height
		};

		AdjustWindowRect(
			&rect,
			WS_OVERLAPPEDWINDOW,
			FALSE
		);

		window =
			CreateWindowExW(
				0,
				className,
				title,
				WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				rect.right - rect.left,
				rect.bottom - rect.top,
				nullptr,
				nullptr,
				instance,
				this
			);

		if (!window)
			return false;

		ShowWindow(
			window,
			SW_SHOW
		);

		UpdateWindow(window);

		return true;
	}

	bool createDevice()
	{
		DXGI_SWAP_CHAIN_DESC swapDesc{};

		swapDesc.BufferCount = 2;
		swapDesc.BufferDesc.Width = width;
		swapDesc.BufferDesc.Height = height;
		swapDesc.BufferDesc.Format =
			DXGI_FORMAT_R8G8B8A8_UNORM;
		swapDesc.BufferUsage =
			DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapDesc.OutputWindow = window;
		swapDesc.SampleDesc.Count = 1;
		swapDesc.Windowed = TRUE;
		swapDesc.SwapEffect =
			DXGI_SWAP_EFFECT_DISCARD;

		D3D_FEATURE_LEVEL requested[] = {
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
		};

		D3D_FEATURE_LEVEL created{};

		const HRESULT hr =
			D3D11CreateDeviceAndSwapChain(
				nullptr,
				D3D_DRIVER_TYPE_HARDWARE,
				nullptr,
				0,
				requested,
				static_cast<UINT>(
					std::size(requested)
				),
				D3D11_SDK_VERSION,
				&swapDesc,
				&swapChain,
				&device,
				&created,
				&context
			);

		if (FAILED(hr))
		{
			std::fprintf(
				stderr,
				"[DX11] D3D11CreateDeviceAndSwapChain failed: 0x%08lx\n",
				static_cast<unsigned long>(
					hr
				)
			);

			return false;
		}

		return true;
	}

	bool createTargets()
	{
		ComPtr<ID3D11Texture2D> backBuffer;

		HRESULT hr =
			swapChain->GetBuffer(
				0,
				IID_PPV_ARGS(
					&backBuffer
				)
			);

		if (FAILED(hr))
			return false;

		hr =
			device->CreateRenderTargetView(
				backBuffer.Get(),
				nullptr,
				&renderTargetView
			);

		if (FAILED(hr))
			return false;

		D3D11_TEXTURE2D_DESC depthDesc{};
		depthDesc.Width = width;
		depthDesc.Height = height;
		depthDesc.MipLevels = 1;
		depthDesc.ArraySize = 1;
		depthDesc.Format =
			DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.Usage =
			D3D11_USAGE_DEFAULT;
		depthDesc.BindFlags =
			D3D11_BIND_DEPTH_STENCIL;

		hr =
			device->CreateTexture2D(
				&depthDesc,
				nullptr,
				&depthTexture
			);

		if (FAILED(hr))
			return false;

		hr =
			device->CreateDepthStencilView(
				depthTexture.Get(),
				nullptr,
				&depthStencilView
			);

		return SUCCEEDED(hr);
	}

	bool createPipeline()
	{
		ComPtr<ID3DBlob> vertexBytecode;
		ComPtr<ID3DBlob> pixelBytecode;

		if (
			!compileShader(
				VERTEX_SHADER_SOURCE,
				"main",
				"vs_4_0",
				vertexBytecode
			)
		)
		{
			return false;
		}

		if (
			!compileShader(
				PIXEL_SHADER_SOURCE,
				"main",
				"ps_4_0",
				pixelBytecode
			)
		)
		{
			return false;
		}

		HRESULT hr =
			device->CreateVertexShader(
				vertexBytecode->GetBufferPointer(),
				vertexBytecode->GetBufferSize(),
				nullptr,
				&vertexShader
			);

		if (FAILED(hr))
			return false;

		hr =
			device->CreatePixelShader(
				pixelBytecode->GetBufferPointer(),
				pixelBytecode->GetBufferSize(),
				nullptr,
				&pixelShader
			);

		if (FAILED(hr))
			return false;

		const D3D11_INPUT_ELEMENT_DESC layout[] = {
			{
				"POSITION",
				0,
				DXGI_FORMAT_R32G32B32_FLOAT,
				0,
				0,
				D3D11_INPUT_PER_VERTEX_DATA,
				0
			},
			{
				"NORMAL",
				0,
				DXGI_FORMAT_R32G32B32_FLOAT,
				0,
				12,
				D3D11_INPUT_PER_VERTEX_DATA,
				0
			},
		};

		hr =
			device->CreateInputLayout(
				layout,
				static_cast<UINT>(
					std::size(layout)
				),
				vertexBytecode->GetBufferPointer(),
				vertexBytecode->GetBufferSize(),
				&inputLayout
			);

		if (FAILED(hr))
			return false;

		D3D11_BUFFER_DESC vertexDesc{};
		vertexDesc.ByteWidth =
			sizeof(CUBE_VERTICES);
		vertexDesc.Usage =
			D3D11_USAGE_IMMUTABLE;
		vertexDesc.BindFlags =
			D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vertexData{};
		vertexData.pSysMem =
			CUBE_VERTICES;

		hr =
			device->CreateBuffer(
				&vertexDesc,
				&vertexData,
				&vertexBuffer
			);

		if (FAILED(hr))
			return false;

		D3D11_BUFFER_DESC indexDesc{};
		indexDesc.ByteWidth =
			sizeof(CUBE_INDICES);
		indexDesc.Usage =
			D3D11_USAGE_IMMUTABLE;
		indexDesc.BindFlags =
			D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA indexData{};
		indexData.pSysMem =
			CUBE_INDICES;

		hr =
			device->CreateBuffer(
				&indexDesc,
				&indexData,
				&indexBuffer
			);

		if (FAILED(hr))
			return false;

		std::vector<Vertex> gridVertices;

		constexpr int gridHalfCount = 20;
		constexpr float gridSpacing = 4.0f;
		constexpr float gridExtent =
			gridHalfCount * gridSpacing;

		for (
			int line = -gridHalfCount;
			line <= gridHalfCount;
			++line
		)
		{
			const float offset =
				line * gridSpacing;

			gridVertices.push_back(
				{
					{
						-gridExtent,
						0.0f,
						offset
					},
					{
						0.0f,
						1.0f,
						0.0f
					}
				}
			);

			gridVertices.push_back(
				{
					{
						gridExtent,
						0.0f,
						offset
					},
					{
						0.0f,
						1.0f,
						0.0f
					}
				}
			);

			gridVertices.push_back(
				{
					{
						offset,
						0.0f,
						-gridExtent
					},
					{
						0.0f,
						1.0f,
						0.0f
					}
				}
			);

			gridVertices.push_back(
				{
					{
						offset,
						0.0f,
						gridExtent
					},
					{
						0.0f,
						1.0f,
						0.0f
					}
				}
			);
		}

		gridVertexCount =
			static_cast<UINT>(
				gridVertices.size()
			);

		D3D11_BUFFER_DESC gridDesc{};
		gridDesc.ByteWidth =
			static_cast<UINT>(
				gridVertices.size() *
				sizeof(Vertex)
			);
		gridDesc.Usage =
			D3D11_USAGE_IMMUTABLE;
		gridDesc.BindFlags =
			D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA gridData{};
		gridData.pSysMem =
			gridVertices.data();

		hr =
			device->CreateBuffer(
				&gridDesc,
				&gridData,
				&gridVertexBuffer
			);

		if (FAILED(hr))
			return false;

		D3D11_BUFFER_DESC constantDesc{};
		constantDesc.ByteWidth =
			sizeof(ConstantBuffer);
		constantDesc.Usage =
			D3D11_USAGE_DEFAULT;
		constantDesc.BindFlags =
			D3D11_BIND_CONSTANT_BUFFER;

		hr =
			device->CreateBuffer(
				&constantDesc,
				nullptr,
				&constantBuffer
			);

		if (FAILED(hr))
			return false;

		D3D11_RASTERIZER_DESC rasterizerDesc{};
		rasterizerDesc.FillMode =
			D3D11_FILL_SOLID;
		rasterizerDesc.CullMode =
			D3D11_CULL_NONE;
		rasterizerDesc.DepthClipEnable =
			TRUE;

		hr =
			device->CreateRasterizerState(
				&rasterizerDesc,
				&rasterizerState
			);

		if (FAILED(hr))
			return false;

		D3D11_BLEND_DESC blendDesc{};
		blendDesc.RenderTarget[0].BlendEnable =
			TRUE;

		blendDesc.RenderTarget[0].SrcBlend =
			D3D11_BLEND_SRC_ALPHA;

		blendDesc.RenderTarget[0].DestBlend =
			D3D11_BLEND_INV_SRC_ALPHA;

		blendDesc.RenderTarget[0].BlendOp =
			D3D11_BLEND_OP_ADD;

		blendDesc.RenderTarget[0].SrcBlendAlpha =
			D3D11_BLEND_ONE;

		blendDesc.RenderTarget[0].DestBlendAlpha =
			D3D11_BLEND_ZERO;

		blendDesc.RenderTarget[0].BlendOpAlpha =
			D3D11_BLEND_OP_ADD;

		blendDesc.RenderTarget[0].RenderTargetWriteMask =
			D3D11_COLOR_WRITE_ENABLE_ALL;

		hr =
			device->CreateBlendState(
				&blendDesc,
				&blendState
			);

		return SUCCEEDED(hr);
	}

	void setupFrame()
	{
		const float clearColor[4] = {
			0.07f,
			0.10f,
			0.16f,
			1.0f
		};

		context->ClearRenderTargetView(
			renderTargetView.Get(),
			clearColor
		);

		context->ClearDepthStencilView(
			depthStencilView.Get(),
			D3D11_CLEAR_DEPTH |
				D3D11_CLEAR_STENCIL,
			1.0f,
			0
		);

		ID3D11RenderTargetView* targets[] = {
			renderTargetView.Get()
		};

		context->OMSetRenderTargets(
			1,
			targets,
			depthStencilView.Get()
		);

		D3D11_VIEWPORT viewport{};
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width =
			static_cast<float>(width);
		viewport.Height =
			static_cast<float>(height);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		context->RSSetViewports(
			1,
			&viewport
		);

		context->RSSetState(
			rasterizerState.Get()
		);

		const float blendFactor[4] = {
			0.0f,
			0.0f,
			0.0f,
			0.0f
		};

		context->OMSetBlendState(
			blendState.Get(),
			blendFactor,
			0xffffffff
		);

		const UINT stride =
			sizeof(Vertex);
		const UINT offset = 0;

		ID3D11Buffer* buffers[] = {
			vertexBuffer.Get()
		};

		context->IASetVertexBuffers(
			0,
			1,
			buffers,
			&stride,
			&offset
		);

		context->IASetIndexBuffer(
			indexBuffer.Get(),
			DXGI_FORMAT_R16_UINT,
			0
		);

		context->IASetInputLayout(
			inputLayout.Get()
		);

		context->IASetPrimitiveTopology(
			D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
		);

		context->VSSetShader(
			vertexShader.Get(),
			nullptr,
			0
		);

		context->PSSetShader(
			pixelShader.Get(),
			nullptr,
			0
		);

		ID3D11Buffer* constantBuffers[] = {
			constantBuffer.Get()
		};

		context->VSSetConstantBuffers(
			0,
			1,
			constantBuffers
		);

		context->PSSetConstantBuffers(
			0,
			1,
			constantBuffers
		);
	}
};

Dx11Renderer::Dx11Renderer()
	: impl_(
		std::make_unique<Impl>()
	)
{
}

Dx11Renderer::~Dx11Renderer()
{
	if (
		impl_ &&
		impl_->window
	)
	{
		DestroyWindow(
			impl_->window
		);

		impl_->window = nullptr;
	}
}

bool Dx11Renderer::initialize(
	const wchar_t* title,
	int width,
	int height
)
{
	if (
		!impl_->createWindow(
			title,
			width,
			height
		)
	)
	{
		return false;
	}

	if (!impl_->createDevice())
		return false;

	if (!impl_->createTargets())
		return false;

	if (!impl_->createPipeline())
		return false;

	impl_->running = true;
	return true;
}

void Dx11Renderer::setInputCallback(
	InputCallback callback
)
{
	impl_->inputCallback =
		std::move(callback);
}

bool Dx11Renderer::pumpEvents()
{
	MSG message{};

	while (
		PeekMessageW(
			&message,
			nullptr,
			0,
			0,
			PM_REMOVE
		)
	)
	{
		if (message.message == WM_QUIT)
		{
			impl_->running = false;
			break;
		}

		TranslateMessage(&message);
		DispatchMessageW(&message);
	}

	return impl_->running;
}

void Dx11Renderer::render(
	const std::vector<
		RobloxObjectModel::RenderPartSnapshot
	>& parts,
	const RobloxObjectModel::RenderCameraSnapshot&
		camera
)
{
	if (!impl_->running)
		return;

	impl_->setupFrame();

	using namespace DirectX;

	XMFLOAT3 target{
		0.0f,
		0.0f,
		0.0f
	};

	if (camera.hasSubject)
	{
		target = {
			camera.targetX,
			camera.targetY,
			camera.targetZ
		};
	}

	const XMVECTOR eye =
		XMVectorSet(
			target.x + 14.0f,
			target.y + 10.0f,
			target.z + 18.0f,
			1.0f
		);

	const XMVECTOR focus =
		XMVectorSet(
			target.x,
			target.y,
			target.z,
			1.0f
		);

	const XMVECTOR up =
		XMVectorSet(
			0.0f,
			1.0f,
			0.0f,
			0.0f
		);

	const XMMATRIX view =
		XMMatrixLookAtLH(
			eye,
			focus,
			up
		);

	const float aspect =
		impl_->height > 0
			? static_cast<float>(
				impl_->width
			) /
				static_cast<float>(
					impl_->height
				)
			: 1.0f;

	const XMMATRIX projection =
		XMMatrixPerspectiveFovLH(
			XMConvertToRadians(
				70.0f
			),
			aspect,
			0.1f,
			2000.0f
		);

	const XMMATRIX viewProjection =
		view * projection;

	{
		ConstantBuffer gridConstants{};

		XMStoreFloat4x4(
			&gridConstants.world,
			XMMatrixIdentity()
		);

		XMStoreFloat4x4(
			&gridConstants.viewProjection,
			viewProjection
		);

		gridConstants.color =
			XMFLOAT4(
				0.22f,
				0.26f,
				0.32f,
				1.0f
			);

		impl_->context->UpdateSubresource(
			impl_->constantBuffer.Get(),
			0,
			nullptr,
			&gridConstants,
			0,
			0
		);

		const UINT stride =
			sizeof(Vertex);
		const UINT offset = 0;

		ID3D11Buffer* gridBuffers[] = {
			impl_->gridVertexBuffer.Get()
		};

		impl_->context->IASetVertexBuffers(
			0,
			1,
			gridBuffers,
			&stride,
			&offset
		);

		impl_->context->IASetPrimitiveTopology(
			D3D11_PRIMITIVE_TOPOLOGY_LINELIST
		);

		impl_->context->Draw(
			impl_->gridVertexCount,
			0
		);

		ID3D11Buffer* cubeBuffers[] = {
			impl_->vertexBuffer.Get()
		};

		impl_->context->IASetVertexBuffers(
			0,
			1,
			cubeBuffers,
			&stride,
			&offset
		);

		impl_->context->IASetPrimitiveTopology(
			D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
		);
	}

	for (const auto& part : parts)
	{
		if (
			part.transparency >= 1.0f
		)
		{
			continue;
		}

		const XMMATRIX world =
			XMMatrixScaling(
				std::max(
					part.sizeX,
					0.001f
				),
				std::max(
					part.sizeY,
					0.001f
				),
				std::max(
					part.sizeZ,
					0.001f
				)
			) *
			XMMatrixTranslation(
				part.positionX,
				part.positionY,
				part.positionZ
			);

		ConstantBuffer constants{};

		XMStoreFloat4x4(
			&constants.world,
			world
		);

		XMStoreFloat4x4(
			&constants.viewProjection,
			viewProjection
		);

		constants.color =
			XMFLOAT4(
				std::clamp(
					part.colorR,
					0.0f,
					1.0f
				),
				std::clamp(
					part.colorG,
					0.0f,
					1.0f
				),
				std::clamp(
					part.colorB,
					0.0f,
					1.0f
				),
				std::clamp(
					1.0f -
						part.transparency,
					0.0f,
					1.0f
				)
			);

		impl_->context->UpdateSubresource(
			impl_->constantBuffer.Get(),
			0,
			nullptr,
			&constants,
			0,
			0
		);

		impl_->context->DrawIndexed(
			static_cast<UINT>(
				std::size(
					CUBE_INDICES
				)
			),
			0,
			0
		);
	}

	impl_->swapChain->Present(
		1,
		0
	);
}

bool Dx11Renderer::isRunning() const
{
	return
		impl_ &&
		impl_->running;
}

#endif
