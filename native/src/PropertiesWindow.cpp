#ifdef _WIN32

#include "PropertiesWindow.h"
#include "EngineLog.h"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <string>
#include <utility>

namespace
{
	constexpr int ID_PART_COMBO = 5001;

	constexpr int ID_POS_X = 5010;
	constexpr int ID_POS_Y = 5011;
	constexpr int ID_POS_Z = 5012;

	constexpr int ID_SIZE_X = 5020;
	constexpr int ID_SIZE_Y = 5021;
	constexpr int ID_SIZE_Z = 5022;

	constexpr int ID_TRANSPARENCY = 5030;

	constexpr int ID_COLOR_R = 5040;
	constexpr int ID_COLOR_G = 5041;
	constexpr int ID_COLOR_B = 5042;

	constexpr int ID_ANCHORED = 5050;
	constexpr int ID_CAN_COLLIDE = 5051;

	constexpr int ID_APPLY = 5060;
	constexpr int ID_REFRESH = 5061;

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

	void setEditFloat(
		HWND edit,
		float value
	)
	{
		if (!edit)
			return;

		wchar_t buffer[64] = {};

		std::swprintf(
			buffer,
			sizeof(buffer) /
				sizeof(buffer[0]),
			L"%.4f",
			static_cast<double>(
				value
			)
		);

		SetWindowTextW(
			edit,
			buffer
		);
	}

	bool readEditFloat(
		HWND edit,
		float& value
	)
	{
		if (!edit)
			return false;

		wchar_t buffer[128] = {};

		GetWindowTextW(
			edit,
			buffer,
			static_cast<int>(
				sizeof(buffer) /
					sizeof(buffer[0])
			)
		);

		wchar_t* end = nullptr;

		const double parsed =
			std::wcstod(
				buffer,
				&end
			);

		if (
			end == buffer ||
			!std::isfinite(parsed)
		)
		{
			return false;
		}

		while (
			end &&
			*end != L'\0'
		)
		{
			if (!iswspace(*end))
				return false;

			++end;
		}

		value =
			static_cast<float>(
				parsed
			);

		return true;
	}
}

struct PropertiesWindow::Impl
{
	HWND owner = nullptr;
	HWND window = nullptr;
	HFONT font = nullptr;

	HWND partCombo = nullptr;

	HWND posX = nullptr;
	HWND posY = nullptr;
	HWND posZ = nullptr;

	HWND sizeX = nullptr;
	HWND sizeY = nullptr;
	HWND sizeZ = nullptr;

	HWND transparency = nullptr;

	HWND colorR = nullptr;
	HWND colorG = nullptr;
	HWND colorB = nullptr;

	HWND anchored = nullptr;
	HWND canCollide = nullptr;

	HWND applyButton = nullptr;
	HWND refreshButton = nullptr;

	std::vector<
		RobloxObjectModel::RenderPartSnapshot
	> parts;

	ApplyCallback applyCallback;

	std::uint64_t selectedId = 0;

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
			auto* create =
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

		if (!self)
		{
			return DefWindowProcW(
				hwnd,
				message,
				wParam,
				lParam
			);
		}

		if (message == WM_CLOSE)
		{
			ShowWindow(
				hwnd,
				SW_HIDE
			);

			return 0;
		}

		if (message == WM_COMMAND)
		{
			const int id =
				LOWORD(wParam);

			const int code =
				HIWORD(wParam);

			if (
				id == ID_PART_COMBO &&
				code == CBN_SELCHANGE
			)
			{
				self->loadSelection();
				return 0;
			}

			if (
				id == ID_REFRESH &&
				code == BN_CLICKED
			)
			{
				self->loadSelection();
				return 0;
			}

			if (
				id == ID_APPLY &&
				code == BN_CLICKED
			)
			{
				self->apply();
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

	HWND createLabel(
		const wchar_t* text,
		int x,
		int y,
		int width,
		int height
	)
	{
		HWND control =
			CreateWindowExW(
				0,
				L"STATIC",
				text,
				WS_CHILD |
					WS_VISIBLE,
				x,
				y,
				width,
				height,
				window,
				nullptr,
				GetModuleHandleW(
					nullptr
				),
				nullptr
			);

		if (control)
		{
			SendMessageW(
				control,
				WM_SETFONT,
				reinterpret_cast<WPARAM>(
					font
				),
				TRUE
			);
		}

		return control;
	}

	HWND createEdit(
		int id,
		int x,
		int y,
		int width = 82
	)
	{
		HWND control =
			CreateWindowExW(
				WS_EX_CLIENTEDGE,
				L"EDIT",
				L"",
				WS_CHILD |
					WS_VISIBLE |
					ES_AUTOHSCROLL,
				x,
				y,
				width,
				23,
				window,
				reinterpret_cast<HMENU>(
					static_cast<INT_PTR>(
						id
					)
				),
				GetModuleHandleW(
					nullptr
				),
				nullptr
			);

		if (control)
		{
			SendMessageW(
				control,
				WM_SETFONT,
				reinterpret_cast<WPARAM>(
					font
				),
				TRUE
			);
		}

		return control;
	}

	bool createControls()
	{
		font =
			static_cast<HFONT>(
				GetStockObject(
					DEFAULT_GUI_FONT
				)
			);

		createLabel(
			L"BasePart",
			12,
			14,
			72,
			20
		);

		partCombo =
			CreateWindowExW(
				0,
				L"COMBOBOX",
				L"",
				WS_CHILD |
					WS_VISIBLE |
					CBS_DROPDOWNLIST |
					WS_VSCROLL,
				88,
				10,
				278,
				220,
				window,
				reinterpret_cast<HMENU>(
					static_cast<INT_PTR>(
						ID_PART_COMBO
					)
				),
				GetModuleHandleW(
					nullptr
				),
				nullptr
			);

		if (!partCombo)
			return false;

		SendMessageW(
			partCombo,
			WM_SETFONT,
			reinterpret_cast<WPARAM>(
				font
			),
			TRUE
		);

		createLabel(
			L"Position",
			12,
			55,
			72,
			20
		);

		createLabel(L"X", 90, 55, 16, 20);
		createLabel(L"Y", 184, 55, 16, 20);
		createLabel(L"Z", 278, 55, 16, 20);

		posX = createEdit(ID_POS_X, 106, 51);
		posY = createEdit(ID_POS_Y, 200, 51);
		posZ = createEdit(ID_POS_Z, 294, 51);

		createLabel(
			L"Size",
			12,
			91,
			72,
			20
		);

		createLabel(L"X", 90, 91, 16, 20);
		createLabel(L"Y", 184, 91, 16, 20);
		createLabel(L"Z", 278, 91, 16, 20);

		sizeX = createEdit(ID_SIZE_X, 106, 87);
		sizeY = createEdit(ID_SIZE_Y, 200, 87);
		sizeZ = createEdit(ID_SIZE_Z, 294, 87);

		createLabel(
			L"Transparency",
			12,
			127,
			88,
			20
		);

		transparency =
			createEdit(
				ID_TRANSPARENCY,
				106,
				123
			);

		createLabel(
			L"Color",
			12,
			163,
			72,
			20
		);

		createLabel(L"R", 90, 163, 16, 20);
		createLabel(L"G", 184, 163, 16, 20);
		createLabel(L"B", 278, 163, 16, 20);

		colorR = createEdit(ID_COLOR_R, 106, 159);
		colorG = createEdit(ID_COLOR_G, 200, 159);
		colorB = createEdit(ID_COLOR_B, 294, 159);

		anchored =
			CreateWindowExW(
				0,
				L"BUTTON",
				L"Anchored",
				WS_CHILD |
					WS_VISIBLE |
					BS_AUTOCHECKBOX,
				12,
				202,
				110,
				24,
				window,
				reinterpret_cast<HMENU>(
					static_cast<INT_PTR>(
						ID_ANCHORED
					)
				),
				GetModuleHandleW(nullptr),
				nullptr
			);

		canCollide =
			CreateWindowExW(
				0,
				L"BUTTON",
				L"CanCollide",
				WS_CHILD |
					WS_VISIBLE |
					BS_AUTOCHECKBOX,
				130,
				202,
				110,
				24,
				window,
				reinterpret_cast<HMENU>(
					static_cast<INT_PTR>(
						ID_CAN_COLLIDE
					)
				),
				GetModuleHandleW(nullptr),
				nullptr
			);

		applyButton =
			CreateWindowExW(
				0,
				L"BUTTON",
				L"Apply",
				WS_CHILD |
					WS_VISIBLE |
					BS_DEFPUSHBUTTON,
				210,
				244,
				75,
				26,
				window,
				reinterpret_cast<HMENU>(
					static_cast<INT_PTR>(
						ID_APPLY
					)
				),
				GetModuleHandleW(nullptr),
				nullptr
			);

		refreshButton =
			CreateWindowExW(
				0,
				L"BUTTON",
				L"Refresh",
				WS_CHILD |
					WS_VISIBLE |
					BS_PUSHBUTTON,
				291,
				244,
				75,
				26,
				window,
				reinterpret_cast<HMENU>(
					static_cast<INT_PTR>(
						ID_REFRESH
					)
				),
				GetModuleHandleW(nullptr),
				nullptr
			);

		for (
			HWND control : {
				anchored,
				canCollide,
				applyButton,
				refreshButton
			}
		)
		{
			if (!control)
				return false;

			SendMessageW(
				control,
				WM_SETFONT,
				reinterpret_cast<WPARAM>(
					font
				),
				TRUE
			);
		}

		return
			posX &&
			posY &&
			posZ &&
			sizeX &&
			sizeY &&
			sizeZ &&
			transparency &&
			colorR &&
			colorG &&
			colorB;
	}

	int selectedIndex() const
	{
		if (!partCombo)
			return -1;

		return static_cast<int>(
			SendMessageW(
				partCombo,
				CB_GETCURSEL,
				0,
				0
			)
		);
	}

	void loadSelection()
	{
		const int index =
			selectedIndex();

		if (
			index < 0 ||
			static_cast<std::size_t>(
				index
			) >= parts.size()
		)
		{
			selectedId = 0;
			return;
		}

		const auto& part =
			parts[
				static_cast<std::size_t>(
					index
				)
			];

		selectedId = part.id;

		setEditFloat(
			posX,
			part.positionX
		);

		setEditFloat(
			posY,
			part.positionY
		);

		setEditFloat(
			posZ,
			part.positionZ
		);

		setEditFloat(
			sizeX,
			part.sizeX
		);

		setEditFloat(
			sizeY,
			part.sizeY
		);

		setEditFloat(
			sizeZ,
			part.sizeZ
		);

		setEditFloat(
			transparency,
			part.transparency
		);

		setEditFloat(
			colorR,
			part.colorR
		);

		setEditFloat(
			colorG,
			part.colorG
		);

		setEditFloat(
			colorB,
			part.colorB
		);

		SendMessageW(
			anchored,
			BM_SETCHECK,
			part.anchored
				? BST_CHECKED
				: BST_UNCHECKED,
			0
		);

		SendMessageW(
			canCollide,
			BM_SETCHECK,
			part.canCollide
				? BST_CHECKED
				: BST_UNCHECKED,
			0
		);
	}

	void rebuildCombo()
	{
		if (!partCombo)
			return;

		std::uint64_t previousId =
			selectedId;

		SendMessageW(
			partCombo,
			CB_RESETCONTENT,
			0,
			0
		);

		int selected = -1;

		for (
			std::size_t index = 0;
			index < parts.size();
			++index
		)
		{
			std::string label =
				parts[index].name;

			label += "  [";
			label += std::to_string(
				parts[index].id
			);
			label += "]";

			const std::wstring wide =
				utf8ToWide(label);

			SendMessageW(
				partCombo,
				CB_ADDSTRING,
				0,
				reinterpret_cast<LPARAM>(
					wide.c_str()
				)
			);

			if (
				parts[index].id ==
				previousId
			)
			{
				selected =
					static_cast<int>(
						index
					);
			}
		}

		if (
			selected < 0 &&
			!parts.empty()
		)
		{
			selected = 0;
		}

		if (selected >= 0)
		{
			SendMessageW(
				partCombo,
				CB_SETCURSEL,
				static_cast<WPARAM>(
					selected
				),
				0
			);

			loadSelection();
		}
		else
		{
			selectedId = 0;
		}
	}

	bool samePartList(
		const std::vector<
			RobloxObjectModel::RenderPartSnapshot
		>& newParts
	) const
	{
		if (
			newParts.size() !=
			parts.size()
		)
		{
			return false;
		}

		for (
			std::size_t index = 0;
			index < parts.size();
			++index
		)
		{
			if (
				parts[index].id !=
					newParts[index].id ||
				parts[index].name !=
					newParts[index].name
			)
			{
				return false;
			}
		}

		return true;
	}

	void setPartsInternal(
		const std::vector<
			RobloxObjectModel::RenderPartSnapshot
		>& newParts
	)
	{
		const bool listChanged =
			!samePartList(
				newParts
			);

		parts = newParts;

		if (
			window &&
			IsWindowVisible(window) &&
			listChanged
		)
		{
			rebuildCombo();
		}
	}

	void apply()
	{
		if (
			!applyCallback ||
			selectedId == 0
		)
		{
			return;
		}

		RobloxObjectModel::
			RenderPartPropertyUpdate update;

		update.id = selectedId;

		bool valid =
			readEditFloat(
				posX,
				update.positionX
			) &&
			readEditFloat(
				posY,
				update.positionY
			) &&
			readEditFloat(
				posZ,
				update.positionZ
			) &&
			readEditFloat(
				sizeX,
				update.sizeX
			) &&
			readEditFloat(
				sizeY,
				update.sizeY
			) &&
			readEditFloat(
				sizeZ,
				update.sizeZ
			) &&
			readEditFloat(
				transparency,
				update.transparency
			) &&
			readEditFloat(
				colorR,
				update.colorR
			) &&
			readEditFloat(
				colorG,
				update.colorG
			) &&
			readEditFloat(
				colorB,
				update.colorB
			);

		if (!valid)
		{
			MessageBoxW(
				window,
				L"Invalid numeric value.",
				L"Properties",
				MB_OK |
					MB_ICONWARNING
			);

			return;
		}

		if (
			update.sizeX <= 0.0f ||
			update.sizeY <= 0.0f ||
			update.sizeZ <= 0.0f
		)
		{
			MessageBoxW(
				window,
				L"Size values must be greater than zero.",
				L"Properties",
				MB_OK |
					MB_ICONWARNING
			);

			return;
		}

		if (
			update.transparency < 0.0f ||
			update.transparency > 1.0f ||
			update.colorR < 0.0f ||
			update.colorR > 1.0f ||
			update.colorG < 0.0f ||
			update.colorG > 1.0f ||
			update.colorB < 0.0f ||
			update.colorB > 1.0f
		)
		{
			MessageBoxW(
				window,
				L"Transparency and Color values must be between 0 and 1.",
				L"Properties",
				MB_OK |
					MB_ICONWARNING
			);

			return;
		}

		update.anchored =
			SendMessageW(
				anchored,
				BM_GETCHECK,
				0,
				0
			) == BST_CHECKED;

		update.canCollide =
			SendMessageW(
				canCollide,
				BM_GETCHECK,
				0,
				0
			) == BST_CHECKED;

		if (!applyCallback(update))
		{
			MessageBoxW(
				window,
				L"The selected BasePart no longer exists.",
				L"Properties",
				MB_OK |
					MB_ICONWARNING
			);

			return;
		}

		EngineLog::writef(
			EngineLog::Component::ObjectModel,
			"Properties window applied id=%llu",
			static_cast<
				unsigned long long
			>(
				update.id
			)
		);
	}
};

PropertiesWindow::PropertiesWindow()
	: impl_(
		std::make_unique<Impl>()
	)
{
}

PropertiesWindow::~PropertiesWindow()
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

bool PropertiesWindow::initialize(
	HWND owner
)
{
	if (!impl_)
		return false;

	impl_->owner = owner;

	const HINSTANCE instance =
		GetModuleHandleW(nullptr);

	const wchar_t* className =
		L"RobloxDx11VulkanPropertiesWindow";

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
	wc.hbrBackground =
		reinterpret_cast<HBRUSH>(
			COLOR_WINDOW + 1
		);
	wc.lpszClassName =
		className;

	RegisterClassExW(&wc);

	impl_->window =
		CreateWindowExW(
			WS_EX_TOOLWINDOW,
			className,
			L"Properties",
			WS_OVERLAPPED |
				WS_CAPTION |
				WS_SYSMENU |
				WS_MINIMIZEBOX,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			398,
			320,
			owner,
			nullptr,
			instance,
			impl_.get()
		);

	if (!impl_->window)
		return false;

	if (!impl_->createControls())
		return false;

	impl_->rebuildCombo();

	return true;
}

void PropertiesWindow::setApplyCallback(
	ApplyCallback callback
)
{
	impl_->applyCallback =
		std::move(callback);
}

void PropertiesWindow::setParts(
	const std::vector<
		RobloxObjectModel::RenderPartSnapshot
	>& parts
)
{
	impl_->setPartsInternal(parts);
}

void PropertiesWindow::show()
{
	if (!impl_->window)
		return;

	impl_->rebuildCombo();

	ShowWindow(
		impl_->window,
		SW_SHOW
	);

	SetForegroundWindow(
		impl_->window
	);
}

void PropertiesWindow::hide()
{
	if (impl_->window)
	{
		ShowWindow(
			impl_->window,
			SW_HIDE
		);
	}
}

bool PropertiesWindow::isVisible() const
{
	return
		impl_ &&
		impl_->window &&
		IsWindowVisible(
			impl_->window
		) != FALSE;
}

#endif
