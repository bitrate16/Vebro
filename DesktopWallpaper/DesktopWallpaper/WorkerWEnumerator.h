#pragma once

// Utility used to find the window located between desktop icons and desktop background
namespace WorkerWEnumerator {

	int enumLevel = 0;
	int maxEnumLevel = 2;
	/*
	 * Prints out debug info with all enumerated windows on the system
	 */
	BOOL CALLBACK EnumWindowsPrint(HWND hWnd, long lParam) {
		if (IsWindowVisible(hWnd) && (maxEnumLevel == -1 || enumLevel <= maxEnumLevel)) {
			wchar_t className[255];
			GetClassName(hWnd, (LPWSTR)className, 254);

			wchar_t windowTitle[255];
			GetWindowText(hWnd, (LPWSTR)windowTitle, 254);

			std::wcout << std::wstring(enumLevel * 2, ' ');

			std::wstringstream stream;
			stream << "0x" << std::hex << std::setw(8) << std::setfill(L'0') << std::uppercase << (INT64)hWnd << std::nouppercase << std::dec;
			std::wstring result(stream.str());

			std::wcout << result;
			std::wcout << " \"" << windowTitle << "\" " << className << std::endl;
			std::wcout << std::flush;

			enumLevel++;
			EnumChildWindows(hWnd, EnumWindowsPrint, 0);
			enumLevel--;
		}
		return TRUE;
	};

	HWND workerw = NULL;
	/*
	 * Enumerates all windows to find WorkerW
	 */
	BOOL CALLBACK EnumWindowsWorker(HWND hWnd, long lParam) {
		HWND p = FindWindowEx(hWnd, NULL, L"SHELLDLL_DefView", NULL);

		if (p != NULL)
			workerw = FindWindowEx(NULL, hWnd, L"WorkerW", NULL);

		return TRUE;
	};

	/*
	 * Returns WorkerW HWND
	 */
	HWND enumerateForWorkerW() {
		HWND progman = FindWindow(L"Progman", NULL);
		DWORD_PTR result = 0;

		SendMessageTimeoutW(progman, 0x052C, NULL, NULL, SMTO_NORMAL, 1000, &result);
#ifdef PRINT_WINDOWS_ENUM
		EnumWindows(EnumWindowsPrint, 0);
#endif

		EnumWindows(EnumWindowsWorker, 0);

		return workerw;
	}
}
