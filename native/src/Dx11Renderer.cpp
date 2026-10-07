#ifdef _WIN32

#include "Dx11Renderer.h"
#include "EngineLog.h"
#include "PropertiesWindow.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <d3d11.h>
#include "ComPtrLite.h"
#include "DirectXMathLite.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace
{
	std::wstring utf8ToWide(
		const std::string& text
	)
	{
		if (text.empty())
			return {};

		const int required =
			MultiByteToWideChar(
				CP_UTF8,
				0,
				text.data(),
				static_cast<int>(
					text.size()
				),
				nullptr,
				0
			);

		if (required <= 0)
		{
			return std::wstring(
				text.begin(),
				text.end()
			);
		}

		std::wstring result(
			static_cast<std::size_t>(
				required
			),
			L'\0'
		);

		MultiByteToWideChar(
			CP_UTF8,
			0,
			text.data(),
			static_cast<int>(
				text.size()
			),
			result.data(),
			required
		);

		return result;
	}

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

	std::wstring executableDirectory()
	{
		wchar_t path[32768] = {};

		const DWORD length =
			GetModuleFileNameW(
				nullptr,
				path,
				static_cast<DWORD>(
					std::size(path)
				)
			);

		if (
			length == 0 ||
			length >= std::size(path)
		)
		{
			return L".";
		}

		std::wstring result(
			path,
			path + length
		);

		const std::wstring::size_type slash =
			result.find_last_of(
				L"\\/"
			);

		if (
			slash ==
			std::wstring::npos
		)
		{
			return L".";
		}

		result.resize(slash);
		return result;
	}

	std::wstring shaderPath(
		const wchar_t* fileName
	)
	{
		std::wstring result =
			executableDirectory();

		result += L"\\shaders\\";
		result += fileName;

		return result;
	}

	bool loadBinaryFile(
		const std::wstring& path,
		std::vector<std::uint8_t>& output
	)
	{
		HANDLE file =
			CreateFileW(
				path.c_str(),
				GENERIC_READ,
				FILE_SHARE_READ,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				nullptr
			);

		if (
			file ==
			INVALID_HANDLE_VALUE
		)
		{
			std::fwprintf(
				stderr,
				L"[DX11 shader] arquivo nao encontrado: %ls\\n",
				path.c_str()
			);

			EngineLog::write(
				EngineLog::Component::Renderer,
				"shader file not found"
			);

			return false;
		}

		LARGE_INTEGER fileSize{};

		if (
			!GetFileSizeEx(
				file,
				&fileSize
			) ||
			fileSize.QuadPart <= 0 ||
			fileSize.QuadPart >
				static_cast<LONGLONG>(
					0xffffffffu
				)
		)
		{
			CloseHandle(file);
			return false;
		}

		output.resize(
			static_cast<std::size_t>(
				fileSize.QuadPart
			)
		);

		DWORD bytesRead = 0;

		const BOOL ok =
			ReadFile(
				file,
				output.data(),
				static_cast<DWORD>(
					output.size()
				),
				&bytesRead,
				nullptr
			);

		CloseHandle(file);

		if (
			!ok ||
			bytesRead !=
				output.size()
		)
		{
			output.clear();
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
	PropertyCallback propertyCallback;

	PropertiesWindow propertiesWindow;

	struct LogButton
	{
		HWND handle = nullptr;
		EngineLog::Component component =
			EngineLog::Component::Luau;
		int id = 0;
	};

	std::vector<LogButton> logButtons;

	HWND logEdit = nullptr;
	HWND clearLogButton = nullptr;
	HFONT uiFont = nullptr;

	std::size_t lastRenderedPartCount =
		static_cast<std::size_t>(-1);

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

	static constexpr int MENU_PROPERTIES =
		4300;

	static constexpr int LOG_BUTTON_BASE =
		4100;

	static constexpr int LOG_CLEAR_BUTTON =
		4199;

	void appendLogEntry(
		const EngineLog::Entry& entry
	)
	{
		if (!logEdit)
			return;

		std::string line;
		line.reserve(
			entry.text.size() + 32
		);

		line += "[";
		line += EngineLog::name(
			entry.component
		);
		line += "] ";
		line += entry.text;
		line += "\r\n";

		const std::wstring wide =
			utf8ToWide(line);

		const LRESULT length =
			SendMessageW(
				logEdit,
				WM_GETTEXTLENGTH,
				0,
				0
			);

		SendMessageW(
			logEdit,
			EM_SETSEL,
			static_cast<WPARAM>(
				length
			),
			static_cast<LPARAM>(
				length
			)
		);

		SendMessageW(
			logEdit,
			EM_REPLACESEL,
			FALSE,
			reinterpret_cast<LPARAM>(
				wide.c_str()
			)
		);

		SendMessageW(
			logEdit,
			EM_SCROLLCARET,
			0,
			0
		);
	}

	void updateLogButton(
		LogButton& button
	)
	{
		if (!button.handle)
			return;

		std::string label =
			EngineLog::name(
				button.component
			);

		label += EngineLog::isEnabled(
			button.component
		)
			? " [ON]"
			: " [OFF]";

		const std::wstring wide =
			utf8ToWide(label);

		SetWindowTextW(
			button.handle,
			wide.c_str()
		);
	}

	void layoutLogPanel()
	{
		if (!window)
			return;

		RECT client{};
		GetClientRect(
			window,
			&client
		);

		const int clientWidth =
			std::max(
				1L,
				client.right -
					client.left
			);

		const int clientHeight =
			std::max(
				1L,
				client.bottom -
					client.top
			);

		const int margin = 6;
		const int buttonHeight = 24;
		const int buttonGap = 4;
		const int panelHeight =
			std::min(
				240,
				std::max(
					120,
					clientHeight / 3
				)
			);

		const int panelTop =
			std::max(
				0,
				clientHeight -
					panelHeight
			);

		const int buttonWidth = 124;

		const int availableWidth =
			std::max(
				buttonWidth,
				clientWidth -
					margin * 2 -
					80
			);

		const int buttonsPerRow =
			std::max(
				1,
				availableWidth /
					(
						buttonWidth +
						buttonGap
					)
			);

		int maxRow = 0;

		for (
			std::size_t index = 0;
			index < logButtons.size();
			++index
		)
		{
			const int row =
				static_cast<int>(
					index
				) /
				buttonsPerRow;

			const int column =
				static_cast<int>(
					index
				) %
				buttonsPerRow;

			maxRow =
				std::max(
					maxRow,
					row
				);

			MoveWindow(
				logButtons[index].handle,
				margin +
					column *
					(
						buttonWidth +
						buttonGap
					),
				panelTop +
					margin +
					row *
					(
						buttonHeight +
						buttonGap
					),
				buttonWidth,
				buttonHeight,
				TRUE
			);
		}

		const int clearWidth = 70;

		if (clearLogButton)
		{
			MoveWindow(
				clearLogButton,
				std::max(
					margin,
					clientWidth -
						margin -
						clearWidth
				),
				panelTop + margin,
				clearWidth,
				buttonHeight,
				TRUE
			);
		}

		const int buttonRows =
			logButtons.empty()
				? 0
				: maxRow + 1;

		const int editTop =
			panelTop +
			margin +
			buttonRows *
				(
					buttonHeight +
					buttonGap
				);

		if (logEdit)
		{
			MoveWindow(
				logEdit,
				margin,
				editTop,
				std::max(
					1,
					clientWidth -
						margin * 2
				),
				std::max(
					1,
					clientHeight -
						editTop -
						margin
				),
				TRUE
			);
		}
	}

	bool createLogPanel()
	{
		uiFont =
			static_cast<HFONT>(
				GetStockObject(
					DEFAULT_GUI_FONT
				)
			);

		logButtons.clear();

		for (
			std::size_t index = 0;
			index <
				EngineLog::componentCount();
			++index
		)
		{
			LogButton button;
			button.component =
				EngineLog::componentAt(
					index
				);

			button.id =
				LOG_BUTTON_BASE +
				static_cast<int>(
					index
				);

			button.handle =
				CreateWindowExW(
					0,
					L"BUTTON",
					L"",
					WS_CHILD |
						WS_VISIBLE |
						BS_PUSHBUTTON,
					0,
					0,
					100,
					24,
					window,
					reinterpret_cast<HMENU>(
						static_cast<INT_PTR>(
							button.id
						)
					),
					GetModuleHandleW(
						nullptr
					),
					nullptr
				);

			if (!button.handle)
				return false;

			SendMessageW(
				button.handle,
				WM_SETFONT,
				reinterpret_cast<WPARAM>(
					uiFont
				),
				TRUE
			);

			logButtons.push_back(
				button
			);

			updateLogButton(
				logButtons.back()
			);
		}

		clearLogButton =
			CreateWindowExW(
				0,
				L"BUTTON",
				L"Clear",
				WS_CHILD |
					WS_VISIBLE |
					BS_PUSHBUTTON,
				0,
				0,
				70,
				24,
				window,
				reinterpret_cast<HMENU>(
					static_cast<INT_PTR>(
						LOG_CLEAR_BUTTON
					)
				),
				GetModuleHandleW(nullptr),
				nullptr
			);

		if (!clearLogButton)
			return false;

		SendMessageW(
			clearLogButton,
			WM_SETFONT,
			reinterpret_cast<WPARAM>(
				uiFont
			),
			TRUE
		);

		logEdit =
			CreateWindowExW(
				WS_EX_CLIENTEDGE,
				L"EDIT",
				L"",
				WS_CHILD |
					WS_VISIBLE |
					WS_VSCROLL |
					ES_LEFT |
					ES_MULTILINE |
					ES_AUTOVSCROLL |
					ES_READONLY |
					ES_NOHIDESEL,
				0,
				0,
				100,
				100,
				window,
				nullptr,
				GetModuleHandleW(nullptr),
				nullptr
			);

		if (!logEdit)
			return false;

		SendMessageW(
			logEdit,
			WM_SETFONT,
			reinterpret_cast<WPARAM>(
				uiFont
			),
			TRUE
		);

		layoutLogPanel();

		for (
			const EngineLog::Entry& entry :
			EngineLog::snapshot()
		)
		{
			appendLogEntry(entry);
		}

		EngineLog::setSink(
			[this](
				const EngineLog::Entry& entry
			)
			{
				appendLogEntry(entry);
			}
		);

		EngineLog::write(
			EngineLog::Component::Renderer,
			"log panel initialized"
		);

		return true;
	}

	void handleLogCommand(
		int commandId
	)
	{
		if (
			commandId ==
			LOG_CLEAR_BUTTON
		)
		{
			EngineLog::clear();

			if (logEdit)
				SetWindowTextW(
					logEdit,
					L""
				);

			SetFocus(window);
			return;
		}

		const int index =
			commandId -
			LOG_BUTTON_BASE;

		if (
			index < 0 ||
			static_cast<std::size_t>(
				index
			) >= logButtons.size()
		)
		{
			return;
		}

		LogButton& button =
			logButtons[
				static_cast<std::size_t>(
					index
				)
			];

		const bool enabled =
			EngineLog::toggle(
				button.component
			);

		updateLogButton(button);

		EngineLog::writef(
			EngineLog::Component::Renderer,
			"log %s: %s",
			EngineLog::name(
				button.component
			),
			enabled
				? "ON"
				: "OFF"
		);

		SetFocus(window);
	}

	bool createMenuBar()
	{
		HMENU menuBar =
			CreateMenu();

		HMENU viewMenu =
			CreatePopupMenu();

		if (
			!menuBar ||
			!viewMenu
		)
		{
			if (viewMenu)
				DestroyMenu(viewMenu);

			if (menuBar)
				DestroyMenu(menuBar);

			return false;
		}

		if (
			!AppendMenuW(
				viewMenu,
				MF_STRING,
				MENU_PROPERTIES,
				L"Properties"
			)
		)
		{
			DestroyMenu(viewMenu);
			DestroyMenu(menuBar);
			return false;
		}

		if (
			!AppendMenuW(
				menuBar,
				MF_POPUP,
				reinterpret_cast<UINT_PTR>(
					viewMenu
				),
				L"View"
			)
		)
		{
			DestroyMenu(viewMenu);
			DestroyMenu(menuBar);
			return false;
		}

		if (
			!SetMenu(
				window,
				menuBar
			)
		)
		{
			DestroyMenu(menuBar);
			return false;
		}

		DrawMenuBar(window);
		return true;
	}

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
				message == WM_COMMAND &&
				LOWORD(wParam) ==
					MENU_PROPERTIES
			)
			{
				self->propertiesWindow.show();
				return 0;
			}

			if (
				message == WM_COMMAND &&
				HIWORD(wParam) ==
					BN_CLICKED
			)
			{
				self->handleLogCommand(
					LOWORD(wParam)
				);

				return 0;
			}

			if (message == WM_SIZE)
			{
				self->layoutLogPanel();
			}

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
				MAKEINTRESOURCEW(32512)
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

		if (!createMenuBar())
			return false;

		if (
			!propertiesWindow.initialize(
				window
			)
		)
		{
			return false;
		}

		if (propertyCallback)
		{
			propertiesWindow.setApplyCallback(
				propertyCallback
			);
		}

		if (!createLogPanel())
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
			EngineLog::writef(
				EngineLog::Component::Renderer,
				"D3D11CreateDeviceAndSwapChain failed 0x%08lx",
				static_cast<unsigned long>(
					hr
				)
			);

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
		std::vector<std::uint8_t>
			vertexBytecode;

		std::vector<std::uint8_t>
			pixelBytecode;

		if (
			!loadBinaryFile(
				shaderPath(L"CubeVS.cso"),
				vertexBytecode
			)
		)
		{
			return false;
		}

		if (
			!loadBinaryFile(
				shaderPath(L"CubePS.cso"),
				pixelBytecode
			)
		)
		{
			return false;
		}

		HRESULT hr =
			device->CreateVertexShader(
				vertexBytecode.data(),
				vertexBytecode.size(),
				nullptr,
				&vertexShader
			);

		if (FAILED(hr))
			return false;

		EngineLog::write(
			EngineLog::Component::Renderer,
			"vertex shader loaded"
		);

		hr =
			device->CreatePixelShader(
				pixelBytecode.data(),
				pixelBytecode.size(),
				nullptr,
				&pixelShader
			);

		if (FAILED(hr))
			return false;

		EngineLog::write(
			EngineLog::Component::Renderer,
			"pixel shader loaded"
		);

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
				vertexBytecode.data(),
				vertexBytecode.size(),
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
	EngineLog::setSink(
		{}
	);

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

	EngineLog::writef(
		EngineLog::Component::Renderer,
		"DX11 initialized %dx%d",
		width,
		height
	);

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

void Dx11Renderer::setPropertyCallback(
	PropertyCallback callback
)
{
	impl_->propertyCallback =
		std::move(callback);

	impl_->propertiesWindow
		.setApplyCallback(
			impl_->propertyCallback
		);
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

	impl_->propertiesWindow.setParts(
		parts
	);

	if (
		impl_->lastRenderedPartCount !=
			parts.size()
	)
	{
		impl_->lastRenderedPartCount =
			parts.size();

		EngineLog::writef(
			EngineLog::Component::Renderer,
			"Workspace render parts: %u",
			static_cast<unsigned int>(
				parts.size()
			)
		);
	}

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
