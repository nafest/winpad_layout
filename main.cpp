// winpad_layout
//
// main.cpp - all the logic for winpad_layout
//
// Copyright (c) 2017 Stefan Winkler
// License: MIT License (for full license see LICENSE)

#include <windows.h>
#include <dwmapi.h>
#include "resource.h"

#include <algorithm>

// given the rectangle of the current monitors'
// workspace, return the rectangle of the
// given quadrant
RECT GetQuadrant(RECT monitor_rect, int quadrant)
{
	RECT r;

	int row = 2 - (quadrant - 1) / 3;
	int col = (quadrant - 1) % 3;

	int width = monitor_rect.right - monitor_rect.left;
	int height = monitor_rect.bottom - monitor_rect.top;

	r.left = monitor_rect.left + col * width / 3;
	r.right = monitor_rect.left + (col + 1) * width / 3;
	r.top = monitor_rect.top + row * height / 3;
	r.bottom = monitor_rect.top + (row + 1) * height / 3;

	return r;
}

// return true if window is currently maximized
bool IsMaximized(HWND window)
{
	WINDOWPLACEMENT placement;
	placement.length = sizeof(placement);

	GetWindowPlacement(window, &placement);

	return placement.showCmd == SW_SHOWMAXIMIZED;
}

// return true, if the two quadrants q1 and q2
// give a maximized window
bool ShouldBeMaximized(int q1, int q2)
{
	if (q1 == 1 && q2 == 9)
		return true;
	if (q1 == 9 && q2 == 1)
		return true;
	if (q1 == 3 && q2 == 7)
		return true;
	if (q1 == 7 && q2 == 3)
		return true;

	return false;
}

// on windows 10 some window borders are invisible.
// Get their width to compensate them
RECT GetWindowBorder(HWND window)
{
	RECT  window_rect;
	GetWindowRect(window, &window_rect);

	RECT  extended_rect;
	DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &extended_rect, sizeof(RECT));

	RECT  border;
	border.left = window_rect.left - extended_rect.left;
	border.top = window_rect.top - extended_rect.top;
	border.right = window_rect.right - extended_rect.right;
	border.bottom = window_rect.bottom - extended_rect.bottom;

	return border;
}

// low level keyboard callback, which is registered with
// SetWindowsHookEx to listen for all keyboard events
LRESULT CALLBACK KeybdProc(int aCode, WPARAM wParam, LPARAM lParam)
{
	static bool ctrl_down = FALSE;
	static int first_quadrant = -1;
	KBDLLHOOKSTRUCT &event = *(PKBDLLHOOKSTRUCT)lParam;

	if (aCode < 0)
		return CallNextHookEx(0, aCode, wParam, lParam);

	auto vk = event.vkCode;

	// track the state of the left control key
	if (vk == VK_LCONTROL && wParam == WM_KEYDOWN)
		ctrl_down = true;
	if (vk == VK_LCONTROL && wParam == WM_KEYUP)
	{
		ctrl_down = false;
		first_quadrant = -1;
	}

	int quadrant = -1;

	// ignore the ctrl + numpad key down events,
	// but avoid that they are forwared to other
	// hooks or applications
	if (wParam == WM_KEYDOWN && ctrl_down)
	{
		if (vk >= VK_NUMPAD1 && vk <= VK_NUMPAD9)
	      return 1;
	}

	// has any of the numbers on the numpad been
	// released?
	if (wParam == WM_KEYUP && ctrl_down)
	{
		if (vk >= VK_NUMPAD1 && vk <= VK_NUMPAD9)
			quadrant = vk - VK_NUMPAD1 + 1;
	}

	if (first_quadrant == -1 && quadrant != -1)
	{
		// remember the first quadrant and wait for
		// the second one;
		first_quadrant = quadrant;
		return 1;
	}

	if (first_quadrant != -1 && quadrant != -1)
	{
		auto active_window = GetForegroundWindow();
		auto border = GetWindowBorder(active_window);

		RECT window_rect;
		GetWindowRect(active_window, &window_rect);

		if (ShouldBeMaximized(first_quadrant, quadrant))
		{
			if (!IsMaximized(active_window))
				ShowWindow(active_window, SW_MAXIMIZE);
		}
		else
		{
			auto monitor = MonitorFromRect(&window_rect, MONITOR_DEFAULTTONEAREST);

			auto monitor_info = MONITORINFOEX();
			monitor_info.cbSize = sizeof(monitor_info);
			GetMonitorInfo(monitor, &monitor_info);

			auto rect1 = GetQuadrant(monitor_info.rcWork, first_quadrant);
			auto rect2 = GetQuadrant(monitor_info.rcWork, quadrant);

			RECT new_rect;
			new_rect.left = std::min<LONG>(rect1.left, rect2.left) + border.left;
			new_rect.top = std::min<LONG>(rect1.top, rect2.top) + border.top;
			new_rect.right = std::max<LONG>(rect1.right, rect2.right) + border.right;
			new_rect.bottom = std::max<LONG>(rect1.bottom, rect2.bottom) + border.bottom;

			if (IsMaximized(active_window))
			{
				ShowWindowAsync(active_window, SW_SHOWNORMAL);
			}
			SetWindowPos(active_window, HWND_TOP, new_rect.left, new_rect.top,
				new_rect.right - new_rect.left, new_rect.bottom - new_rect.top,
				SWP_NOSENDCHANGING);
		}

		// reset the state to wait for the next events
		first_quadrant = -1;
		return 1;
	}

	return CallNextHookEx(0, aCode, wParam, lParam);
}


// window procedure for the hidden window to handle events
// of the tray icon
LRESULT CALLBACK HiddenWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case  WM_USER + 1:
		switch (lParam)
		{
		case WM_RBUTTONUP:
			// the right mouse mutton has been pressed on the tray icon,
			// show a popup menu with the item "Close"
			HMENU menu = CreatePopupMenu();
			InsertMenu(menu, -1, MF_BYPOSITION, 156, L"Close");
			POINT pt;
			GetCursorPos(&pt);
			TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
			DestroyMenu(menu);
			break;
		}
		break;
	case WM_MENUSELECT:
		// since "Close" is the only option in the menu,
		// exit a menu item has been selected
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int CALLBACK WinMain(
	_In_ HINSTANCE hInstance,
	_In_ HINSTANCE hPrevInstance,
	_In_ LPSTR     lpCmdLine,
	_In_ int       nCmdShow
)
{
	// open a hidden window because we need one for creating
	// the tray icon on liston on its events
	WNDCLASSEX wx = {};
	wx.cbSize = sizeof(WNDCLASSEX);
	wx.lpfnWndProc = HiddenWindowProc;
	wx.hInstance = hInstance;
	wx.lpszClassName = L"DUMMY_CLASS";
	RegisterClassEx(&wx);
	auto hWnd = CreateWindowEx(0, L"DUMMY_CLASS", L"dummy_name", 0, 0, 0,
		0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

	// create the icon for the tray
	NOTIFYICONDATA notify_icon;
	notify_icon.cbSize = sizeof(notify_icon);
	notify_icon.uID = 42;
	notify_icon.uCallbackMessage = WM_USER + 1;

	auto hIcon = (HICON)LoadImage(hInstance,
		MAKEINTRESOURCE(IDI_ICON1),
		IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON),
		LR_DEFAULTCOLOR);
	notify_icon.hIcon = hIcon;
	notify_icon.hWnd = hWnd;
	notify_icon.uFlags = NIF_ICON | NIF_MESSAGE;

	Shell_NotifyIcon(NIM_ADD, &notify_icon);

	auto hook = SetWindowsHookEx(WH_KEYBOARD_LL, KeybdProc,
		GetModuleHandle(NULL), 0);

	/* enter the message loop */
	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}