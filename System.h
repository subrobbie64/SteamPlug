#pragma once

#include <Windows.h>
typedef void* systhread_t;
typedef HANDLE syssema_t;
typedef unsigned int threadReturn;
#define THREAD_RETURN 0
#define SEMA_TIMEOUT WAIT_TIMEOUT
typedef HANDLE sysevent_t;

typedef threadReturn(WINAPI* ThreadFunc)(void* arg);

enum Color {
	NONE = 0,
	RED = 0x31,
	GREEN = 0x32,
	YELLOW = 0x33,
	BLUE = 0x34,
	MAGENTA = 0x35,
	CYAN = 0x36,
	WHITE = 0x37,
	LIGHTBLUE = 0x94,
};

void __declspec(noreturn) error(const char* str, ...);
void log(const char* str, ...);
#ifdef _DEBUG
#define debug log
#else
#define debug(a, ...) void(0)
#endif

void clearScreen();
void getTerminalSize(int* columns, int* rows);
void setTerminalCursorVisibility(BOOL visible);
void getTerminalCursorPosition(int* x, int* y);
void printXy(int x, int y, Color color, const char* str, ...);
bool enableVirtualTerminalMode();

namespace System {

	unsigned long long GetMicros();

	void Sleep(unsigned int millis);

	systhread_t CreateThread(ThreadFunc function, void* arg);
	void WaitThread(systhread_t thread);
	void CloseThread(systhread_t thread);

	sysevent_t CreateEventFlag();
	void SetEvent(sysevent_t event);
	void ResetEvent(sysevent_t event);
	void WaitEvent(sysevent_t event);
	bool WaitEvent(sysevent_t event, int timeOutMillis);

	void CreateSema(syssema_t* sema, int initialCount);
	void ClearSema(syssema_t* sema);
	void WaitSema(syssema_t* sema);
	int WaitSema(syssema_t* sema, int timeOutMillis);
	void SignalSema(syssema_t* sema);
	void DestroySema(syssema_t* sema);

	void setupInputIO(int pin);
	bool readInputIO(int pin);
}
