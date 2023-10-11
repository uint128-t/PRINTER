#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib,"gdiplus.lib")
//#pragma comment(linker,"/entry:wWinMainCRTStartup /subsystem:console")
#include <windows.h>
#include <winreg.h>
#include <CommCtrl.h>
#include <string>
#include <iostream>
#include "resource.h"
#include <stdio.h>
#include <gdiplus.h>
using namespace Gdiplus;
HHOOK hHook{ NULL };
HHOOK hHook2{ NULL };
std::wstring text;
int fontsize = 20;
unsigned int delay;
int delaysetting;
bool linemode = false;
bool drawimage = false;
bool quit = false;
int DRAWWIDTH = -1;
int DRAWHEIGHT = -1;
bool isPrinting = false;
Image* image;
HDC memdc;
void PerciseSleep(DWORD milliseconds) {
	long long begin = GetTickCount64();
	while (GetTickCount64() < (begin + milliseconds));
}
void QuitCheck() {
	if (quit) {
		MessageBoxW(0, L"Printing aborted!", L"Abort", MB_OK | MB_ICONERROR);
		exit(0);
	}
}
void MoveCursorToScreenCoordinates(int x, int y) {
	// Calculate the normalized absolute coordinates
	double normalizedX = (double)x / GetSystemMetrics(SM_CXSCREEN);
	double normalizedY = (double)y / GetSystemMetrics(SM_CYSCREEN);

	// Convert the normalized coordinates to the range of 0-65535
	int absoluteX = static_cast<int>(normalizedX * 65535);
	int absoluteY = static_cast<int>(normalizedY * 65535);

	// Move the cursor
	mouse_event(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE, absoluteX, absoluteY, 0, 0);
}

void DrawHDC(HDC memdc, POINT cursorpos, int w, int h) {
	SetForegroundWindow(WindowFromPoint(cursorpos));
	PerciseSleep(delay);
	isPrinting = true;
	for (int x = 0; x < w; x++) {
		bool mousedown = false;
		if (!linemode) {
			for (int y = 0; y < h; y++) {
				QuitCheck();
				COLORREF clr = GetPixel(memdc, x, y);
				if (GetRValue(clr) < 40) {
					SetCursorPos(cursorpos.x + x, cursorpos.y + y);
					PerciseSleep(delay / 2);
					mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
					mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
					PerciseSleep(delay / 2 + delay % 2);
				}
			}
		}
		else {
			for (int y = 0; y < h; y++) {
				QuitCheck();
				COLORREF clr = GetPixel(memdc, x, y);
				if (GetRValue(clr) < 40) {
					MoveCursorToScreenCoordinates(cursorpos.x + x, cursorpos.y + y);
					mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
					while (GetRValue(clr) < 40) {
						y++;
						clr = GetPixel(memdc, x, y);
					}
					MoveCursorToScreenCoordinates(cursorpos.x+x, cursorpos.y+y - 1);
					mouse_event(MOUSEEVENTF_LEFTUP,0 , 0, 0, 0);
					PerciseSleep(delay);
				}
			}
		}
	}
}
void DitherHDC(HDC hdc, int w, int h) {
	for (int x = 0; x < w; x++) {
		for (int y = 0; y < h; y++) {
			COLORREF clr = GetPixel(hdc, x, y);
			unsigned char val = GetRValue(clr);
			unsigned char v = 255 * (rand() % 255 < val);
			SetPixel(hdc, x, y, RGB(v, v, v));
		}
	}
}
void MakeHDC() {
	HDC screen = GetDC(0);
	HBITMAP bmp;
	if (drawimage)
		bmp = CreateCompatibleBitmap(screen, image->GetWidth(), image->GetHeight());
	else
		bmp = CreateCompatibleBitmap(screen, fontsize * text.length(), fontsize);
	memdc = CreateCompatibleDC(screen);
	SelectObject(memdc, bmp);

	if (drawimage) {
		// prepare image
		Gdiplus::ColorMatrix matrix =
		{
			.3f, .3f, .3f,   0,   0,
			.6f, .6f, .6f,   0,   0,
			.1f, .1f, .1f,   0,   0,
			0,   0,   0,   1,   0,
			0,   0,   0,   0,   1
		};
		Gdiplus::ImageAttributes attr;
		attr.SetColorMatrix(&matrix,
			Gdiplus::ColorMatrixFlagsDefault, Gdiplus::ColorAdjustTypeBitmap);
		Graphics memdcg(memdc);
		Rect r(0, 0, image->GetWidth(), image->GetHeight());
		memdcg.FillRectangle(new SolidBrush(Color(255, 255, 255)), r);
		DRAWWIDTH = image->GetWidth();
		DRAWHEIGHT = image->GetHeight();
		memdcg.DrawImage(image, r, 0, 0, image->GetWidth(), image->GetHeight(), UnitPixel, &attr, 0, 0);
		DitherHDC(memdc, image->GetWidth(), image->GetHeight());
	}
	else {
		// prepare text
		RECT x{ 0,0,fontsize * text.length(),fontsize };
		HFONT font = CreateFontW(fontsize, 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, 0);
		SelectObject(memdc, font);
		RECT textpos{ 0,0,fontsize * text.length(),fontsize };
		FillRect(memdc, &x, CreateSolidBrush(RGB(255, 255, 255))); // unsafe: memory leak but program exits anyways
		DRAWHEIGHT = x.bottom;
		DRAWWIDTH = x.right;
		DrawText(memdc, text.c_str(), text.length() + 1, &textpos, DT_TOP | DT_LEFT);
	}
}
DWORD startplot(LPVOID _) {
	// plot based on cursor position
	POINT cursorpos;
	GetCursorPos(&cursorpos);
	if (drawimage) {
		DrawHDC(memdc, cursorpos, image->GetWidth(), image->GetHeight());
	}
	else {
		DrawHDC(memdc, cursorpos, fontsize * text.length(), fontsize);
	}
	exit(0);
	return 0; // nice
}
// Mouse handler for selecting position.
HWND previewHWND = (HWND)-1;
LRESULT CALLBACK m(const int code, const WPARAM wParam, const LPARAM lParam) {
	POINT p;
	GetCursorPos(&p);
	SetWindowPos(previewHWND, 0, p.x, p.y, DRAWWIDTH, DRAWHEIGHT, SWP_NOZORDER);
	if (wParam == WM_LBUTTONDOWN) {
		return 1;
	}
	if (wParam == WM_LBUTTONUP) {
		previewHWND = (HWND)-1;
		UnhookWindowsHookEx(hHook);
		CreateThread(0, 0, startplot, 0, 0, 0);
		return 1;
	}
	return CallNextHookEx(hHook, code, wParam, lParam);
}
DWORD qt(LPVOID _) {QuitCheck();};
// Keyboard hook for cancelling
LRESULT CALLBACK keyboard_hook(const int code, const WPARAM wParam, const LPARAM lParam) {
	if (wParam == WM_KEYDOWN) {
		KBDLLHOOKSTRUCT* kbdStruct = (KBDLLHOOKSTRUCT*)lParam;
		DWORD wVirtKey = kbdStruct->vkCode;
		if (wVirtKey == VK_ESCAPE) {
			quit = true;
			if (previewHWND) {
				UnhookWindowsHookEx(hHook);
				UnhookWindowsHookEx(hHook2);
				ShowWindow(previewHWND, SW_HIDE);
				quit = true;
				if (!isPrinting)CreateThread(0, 0, qt, 0, 0, 0);
				return 1;
			}
		}
	}
	return CallNextHookEx(hHook2, code, wParam, lParam);
}
INT_PTR dlgproc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	// Initalize controls
	case WM_INITDIALOG: {
		SendMessage(GetDlgItem(wnd, IDC_SLIDER1), TBM_SETRANGE, false, MAKELPARAM(10, 100));
		SendMessage(GetDlgItem(wnd, IDC_SLIDER1), TBM_SETTICFREQ, 10, 0);
		SendMessage(GetDlgItem(wnd, IDC_SLIDER1), TBM_SETPOS, true, 20);
		SendMessage(GetDlgItem(wnd, IDC_CHECK1), BM_SETCHECK, BST_CHECKED, 0);
		SetWindowText(GetDlgItem(wnd, IDC_EDIT2), std::to_wstring(delaysetting).c_str());
		HFONT font = CreateFont(20, 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, 0);
		SendMessage(GetDlgItem(wnd, 2022), WM_SETFONT, (WPARAM)font, 1);
		ShowWindow(GetDlgItem(wnd, IDC_BUTTONIMG), SW_HIDE);
		break;
	}
	// Font resize handler
	case WM_HSCROLL: {
		int fs = SendMessage(GetDlgItem(wnd, IDC_SLIDER1), TBM_GETPOS, 0, 0);
		std::string tx = (std::string("Size: ") +
			std::to_string(fs));
		SetDlgItemTextA(wnd, 2022, tx.c_str());
		fontsize = fs;
		HFONT font = CreateFont(fs, 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, 0);
		SendMessage(GetDlgItem(wnd, 2022), WM_SETFONT, (WPARAM)font, 1);
		DeleteObject(font);
		break;
	}
	case WM_COMMAND: {
		if (wParam == IDOK) {
			int its = GetWindowTextLengthW(GetDlgItem(wnd, IDC_EDIT));
			wchar_t* buffer = new wchar_t[its + 1];
			GetWindowTextW(GetDlgItem(wnd, IDC_EDIT), buffer, its + 1);
			text = std::wstring(buffer);
			int dls = GetWindowTextLengthW(GetDlgItem(wnd, IDC_EDIT2));
			wchar_t* buffer2 = new wchar_t[dls + 1];
			GetWindowTextW(GetDlgItem(wnd, IDC_EDIT2), buffer2, dls + 1);
			try { delay = std::stoul(std::wstring(buffer2)); }
			catch (std::invalid_argument) {
				MessageBox(wnd, L"Invalid number (delay should be a number 0 - 1000 in milliseconds)", L"Error", MB_OK | MB_ICONERROR);
				break;
			}
			catch (std::out_of_range) {
				MessageBox(wnd, L"Invalid number (delay should be a number 0 - 1000 in milliseconds)", L"Error", MB_OK | MB_ICONERROR);
				break;
			}
			if (delay > 1000) {
				MessageBox(wnd, L"Invalid number (delay should be a number 0 - 1000 in milliseconds)", L"Error", MB_OK | MB_ICONERROR);
				break;
			}
			linemode = IsDlgButtonChecked(wnd, IDC_CHECK1) == BST_CHECKED;
			EndDialog(wnd, 1);
			return 1;
		}
		else if (wParam == IDC_BUTTONIMG) {
			OPENFILENAME ofn;       // common dialog box structure
			TCHAR szFile[260] = { 0 };       // if using TCHAR macros

			// Initialize OPENFILENAME
			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = wnd;
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = sizeof(szFile);
			ofn.lpstrFilter = L"Image files\0*.png;*.jpg;*.bmp;*.tiff;*.jpeg;*.gif;*.wmf;*.emf;*.icon\0";
			ofn.nFilterIndex = 1;
			ofn.lpstrFileTitle = NULL;
			ofn.nMaxFileTitle = 0;
			ofn.lpstrInitialDir = NULL;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

			int dls = GetWindowTextLengthW(GetDlgItem(wnd, IDC_EDIT2));
			wchar_t* buffer2 = new wchar_t[dls + 1];
			GetWindowTextW(GetDlgItem(wnd, IDC_EDIT2), buffer2, dls + 1);
			try { delay = std::stoul(std::wstring(buffer2)); }
			catch (std::invalid_argument) {
				MessageBox(wnd, L"Invalid number (delay should be a number 0 - 1000 in milliseconds)", L"Error", MB_OK | MB_ICONERROR);
				break;
			}
			catch (std::out_of_range) {
				MessageBox(wnd, L"Invalid number (delay should be a number 0 - 1000 in milliseconds)", L"Error", MB_OK | MB_ICONERROR);
				break;
			}
			if (delay > 1000) {
				MessageBox(wnd, L"Invalid number (delay should be a number 0 - 1000 in milliseconds)", L"Error", MB_OK | MB_ICONERROR);
				break;
			}
			linemode = IsDlgButtonChecked(wnd, IDC_CHECK1) == BST_CHECKED;

			if (GetOpenFileName(&ofn) == TRUE)
			{
				image = new Image(ofn.lpstrFile);
				drawimage = true;
				EndDialog(wnd, 1);
				return 1;
			}
		}
		break;
	}
	// Dropdown handler
	case WM_NOTIFY: {	
		if (((LPNMHDR)lParam)->code == BCN_DROPDOWN) {
			NMBCDROPDOWN* pDropDown = (NMBCDROPDOWN*)lParam;
			if (pDropDown->hdr.hwndFrom = GetDlgItem(wnd, IDOK))
			{

				// Get screen coordinates of the button.
				POINT pt;
				pt.x = pDropDown->rcButton.left;
				pt.y = pDropDown->rcButton.bottom;
				ClientToScreen(pDropDown->hdr.hwndFrom, &pt);

				// Create a menu and add items.
				HMENU hSplitMenu = CreatePopupMenu();
				AppendMenu(hSplitMenu, MF_BYPOSITION, IDOK, L"Draw text");
				AppendMenu(hSplitMenu, MF_BYPOSITION, IDC_BUTTONIMG, L"Draw image");

				// Display the menu.
				TrackPopupMenu(hSplitMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, wnd, NULL);
				return TRUE;
			}
		}
		break;
	}
	case WM_CLOSE: {
		exit(0);
		break;
	}
	}
	return 0;
}
INT_PTR CALLBACK previewproc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam){
	switch (message)
	{
		case WM_INITDIALOG: {
			previewHWND = hwndDlg;
			SetWindowLong(hwndDlg, GWL_EXSTYLE, GetWindowLong(hwndDlg, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT);
			SetLayeredWindowAttributes(hwndDlg, RGB(255,255,255), 100, LWA_ALPHA|LWA_COLORKEY);
			// ShowWindow(hwndDlg, SW_SHOW);
			POINT p;
			GetCursorPos(&p);
			SetWindowPos(hwndDlg, 0, p.x, p.y, DRAWWIDTH, DRAWHEIGHT, SWP_NOZORDER);
			UpdateWindow(hwndDlg);
			break;
		}
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC bg = BeginPaint(hwndDlg, &ps);
			BitBlt(bg, 0, 0, 99999, 99999, memdc, 0, 0, SRCCOPY);
			EndPaint(hwndDlg, &ps);
			return 1;
		}
	}
	return 0;
}
DWORD displayPreview(LPVOID _) {
	DialogBox(GetModuleHandle(0), MAKEINTRESOURCE(IDD_PREVIEWDROP), 0, previewproc);
	return 0;
}
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	SetProcessDPIAware();
	GdiplusStartupInput startInput; // use default constructor to initialize struct
	ULONG_PTR gdiToken;
	Gdiplus::GdiplusStartup(&gdiToken, &startInput, NULL);
	srand(42);

	HKEY key;
	if (RegCreateKey(HKEY_CURRENT_USER, L"SOFTWARE\\TEXTIMAGEPLOTTER", &key)!=ERROR_SUCCESS) {
		MessageBox(0, L"Fatal Registry error! This shouldn't happen.", L"ERROR", MB_OK | MB_ICONERROR);
		exit(1);
	}
	int loadeddelay = -1;
	int buffersize = 4;
	LSTATUS result =  RegGetValue(key, L"", L"Delay", RRF_RT_ANY, 0, &loadeddelay, (LPDWORD) & buffersize);
	if (result == ERROR_SUCCESS) {
		delaysetting = loadeddelay;
	}
	else if (result == ERROR_FILE_NOT_FOUND) {
		delaysetting = 1;
	}
	else {
		MessageBox(0, L"Registry error! This shouldn't happen.", L"ERROR", MB_OK | MB_ICONERROR);
		delaysetting = 1;
	}

	DialogBox(hInstance, MAKEINTRESOURCE(IDD_ASKFORTEXT), 0, dlgproc);
	if (RegSetValueEx(key, L"Delay", 0, REG_DWORD, (const BYTE*)&delay, 4) != ERROR_SUCCESS) {
		MessageBox(0, L"Registry error! Failed to save delay setting.", L"ERROR", MB_OK | MB_ICONERROR);
	}
	MakeHDC();
	CreateThread(0,0,displayPreview,0,0,0);
	while (previewHWND == (HWND)-1);
	hHook2 = SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_hook, GetModuleHandle(0), 0);
	hHook = SetWindowsHookEx(WH_MOUSE_LL, m, GetModuleHandle(0), 0);
	MSG msg;
	while (GetMessage(&msg, 0, 0, 0));
	Sleep(4294967295);
}