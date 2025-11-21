#include "System.h"
#include <Windows.h>
#include <sysinfoapi.h>
#include <stdio.h>
#include <stdarg.h>

void __declspec(noreturn) error(const char* str, ...) {
	char buffer[1024];
	va_list args;
	va_start(args, str);
	vsnprintf(buffer, 1024, str, args);
	va_end(args);
	buffer[1023] = '\0';
	fprintf(stderr, "ERROR: %s", buffer);
	fflush(stderr);
	getchar();
	exit(1);
}

void log(const char* str, ...) {
	char buffer[1024];
	va_list args;
	va_start(args, str);
	vsnprintf(buffer, 1024, str, args);
	va_end(args);
	buffer[1023] = '\0';
	fprintf(stderr, "%s", buffer);
	fflush(stderr);
}

std::string hexString(const unsigned char* data, unsigned long length) {
	std::string result;
	char buf[4];
	for (unsigned long i = 0; i < length; i++) {
		sprintf(buf, "%02X ", data[i]);
		result += buf;
	}
	return result;
}

namespace Terminal {
	void getTerminalSize(int* columns, int* rows) {
		CONSOLE_SCREEN_BUFFER_INFO csbi;

		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
		*columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
		*rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
	}

	void setTerminalCursorVisibility(BOOL visible) {
		HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		CONSOLE_CURSOR_INFO info;
		if (GetConsoleCursorInfo(consoleHandle, &info) == 0)
			info.dwSize = 25;
		info.bVisible = visible;
		SetConsoleCursorInfo(consoleHandle, &info);
	}

	void getTerminalCursorPosition(int* x, int* y) {
		*x = *y = 0;
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
			*x = csbi.dwCursorPosition.X;
			*y = csbi.dwCursorPosition.Y;
		}
	}

	void printXy(int x, int y, Color color, const char* str, ...) {
		char buffer[8192];
		va_list args;
		va_start(args, str);

		char* pos = buffer;
		if (x > 0)
			pos += sprintf(pos, "\033[%d;%dH", y, x);
		if (color != NONE)
			pos += sprintf(pos, "\x1B[%02Xm", color);
		int strLen = vsnprintf(pos, 8192 - 20, str, args);
		pos += strLen;
		if (color != NONE)
			pos += sprintf(pos, "\033[0m");
		va_end(args);
		if ((x <= 0) || (y <= 0)) {
			int w, h;
			getTerminalSize(&w, &h);
			if (x == 0)
				x = (w - strLen) / 2 + 1;
			else
				x = w - strLen + 1;
			if (y <= 0)
				y = h / 2 + 1;
			printf("\033[%d;%dH", y, x);
		}
		printf("%s", buffer);
	}

	bool enableVirtualTerminalMode() {
		// Set output mode to handle virtual terminal sequences
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hOut == INVALID_HANDLE_VALUE)
			return false;

		DWORD dwMode = 0;
		if (!GetConsoleMode(hOut, &dwMode))
			return false;

		dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		if (!SetConsoleMode(hOut, dwMode))
			return false;

		return true;
	}

	void clearScreen() {
		printf("\033[2J");
	}
}

namespace System {

	unsigned long long GetMicros() {
		FILETIME time;
		GetSystemTimeAsFileTime(&time);
		return ((static_cast<unsigned long long>(time.dwHighDateTime) << 32) | time.dwLowDateTime) / 10;
	}

	void Sleep(unsigned int millis) {
		::Sleep(millis);
	}

	systhread_t CreateThread(ThreadFunc function, void* arg) {
		return (systhread_t)::CreateThread(
			NULL,              // no security attribute 
			0,                 // default stack size 
			(LPTHREAD_START_ROUTINE)function,    // thread proc
			(LPVOID)arg,    // thread parameter 
			0,                 // not suspended 
			NULL);      // returns thread ID
	}

	void WaitThread(systhread_t thread) {
		WaitForSingleObject((HANDLE)thread, INFINITE);
		CloseHandle((HANDLE)thread);
	}

	void CloseThread(systhread_t thread) {
		CloseHandle(thread);
	}

	void CreateSema(syssema_t* sema, int initialCount) {
		*sema = CreateSemaphore(NULL, initialCount, 1, NULL);
	}

	void ClearSema(syssema_t* sema) {
		do {
		} while (WaitForSingleObject(*sema, 0) == WAIT_OBJECT_0);
	}

	void WaitSema(syssema_t* sema) {
		WaitForSingleObject(*sema, INFINITE);
	}

	int WaitSema(syssema_t* sema, int waitMillis) {
		if (WaitForSingleObject(*sema, waitMillis) == WAIT_OBJECT_0)
			return 0;
		return SEMA_TIMEOUT;
	}

	void SignalSema(syssema_t* sema) {
		ReleaseSemaphore(*sema, 1, NULL);
	}

	void DestroySema(syssema_t* sema) {
		CloseHandle(*sema);
	}

	sysevent_t CreateEventFlag() {
		return ::CreateEvent(NULL, FALSE, FALSE, NULL); // auto reset, initially not signaled
	}

	void SetEvent(sysevent_t event) {
		::SetEvent(event);
	}

	void ResetEvent(sysevent_t event) {
		::ResetEvent(event);
	}

	void WaitEvent(sysevent_t event) {
		WaitForSingleObject(event, INFINITE);
	}

	bool WaitEvent(sysevent_t event, int timeOutMillis) {
		return WaitForSingleObject(event, timeOutMillis) != WAIT_TIMEOUT;
	}

	void DestroyEventFlag(sysevent_t event) {
		CloseHandle(event);
	}
}
