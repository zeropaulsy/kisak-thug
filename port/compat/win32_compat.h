/* Win32 API subset the shared PC sources call, on POSIX. Implemented in win32_compat.cpp. */
#pragma once
#include <algorithm>
#include <signal.h>
template <class A, class B> inline auto max(A a, B b) -> decltype(a > b ? a : b) { return a > b ? a : b; }
template <class A, class B> inline auto min(A a, B b) -> decltype(a < b ? a : b) { return a < b ? a : b; }
typedef unsigned char byte;
#define CALLBACK
#define __debugbreak() raise(SIGTRAP)
#define CopyMemory(d, s, n) memcpy((d), (s), (n))
#define ZeroMemory(d, n) memset((d), 0, (n))

/* ---- types ---- */
typedef uintptr_t DWORD_PTR;
typedef union { struct { DWORD LowPart; DWORD HighPart; }; unsigned long long QuadPart; } ULARGE_INTEGER;
typedef struct { DWORD Internal; DWORD InternalHigh; DWORD Offset; DWORD OffsetHigh; HANDLE hEvent; } OVERLAPPED, *LPOVERLAPPED;
typedef struct { DWORD dwLowDateTime; DWORD dwHighDateTime; } FILETIME;
typedef struct { DWORD dwFileAttributes; FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
	DWORD nFileSizeHigh, nFileSizeLow, dwReserved0, dwReserved1; char cFileName[MAX_PATH]; char cAlternateFileName[14]; } WIN32_FIND_DATAA, WIN32_FIND_DATA;
typedef struct { unsigned short wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds; } SYSTEMTIME;
typedef void* HKEY;
typedef void (*LPTIMECALLBACK)(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

/* ---- constants ---- */
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define TRUNCATE_EXISTING 5
#define FILE_ATTRIBUTE_READONLY 0x01
#define FILE_ATTRIBUTE_HIDDEN 0x02
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define FILE_ATTRIBUTE_NORMAL 0x80
#define FILE_FLAG_OVERLAPPED 0x40000000
#define FILE_FLAG_NO_BUFFERING 0x20000000
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000
#define FILE_FLAG_RANDOM_ACCESS 0x10000000
#define FILE_BEGIN 0
#define FILE_CURRENT 1
#define FILE_END 2
#define INVALID_FILE_SIZE 0xFFFFFFFFu
#define INVALID_SET_FILE_POINTER 0xFFFFFFFFu
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFFu
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#endif
#define ERROR_SUCCESS 0
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_PATH_NOT_FOUND 3
#define ERROR_ALREADY_EXISTS 183
#define ERROR_HANDLE_EOF 38
#define ERROR_IO_PENDING 997
#define ERROR_IO_INCOMPLETE 996
#define INFINITE 0xFFFFFFFFu
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define HKEY_CURRENT_USER ((HKEY)(uintptr_t)0x80000001)
#define HKEY_LOCAL_MACHINE ((HKEY)(uintptr_t)0x80000002)
#define SPI_GETSCREENSAVETIMEOUT 14
#define SPI_SETSCREENSAVETIMEOUT 15
#define TIME_PERIODIC 1
#define TIME_ONESHOT 0
#define HasOverlappedIoCompleted(o) ((o)->Internal != 997)

/* ---- functions ---- */
void OutputDebugStringA(const char* s);
#define OutputDebugString OutputDebugStringA
void Sleep(DWORD ms);
DWORD timeGetTime();
DWORD GetTickCount();
BOOL QueryPerformanceCounter(LARGE_INTEGER* v);
BOOL QueryPerformanceFrequency(LARGE_INTEGER* v);
DWORD GetLastError();
void SetLastError(DWORD e);
DWORD GetModuleFileNameA(void* module, char* out, DWORD size);
BOOL PathRemoveFileSpecA(char* path);
BOOL PathFileExistsA(const char* path);
const char* PathFindExtensionA(const char* path);
const char* PathFindFileNameA(const char* path);
HANDLE CreateFileA(const char* name, DWORD access, DWORD share, void* security, DWORD disposition, DWORD flags, HANDLE templ);
#define CreateFile CreateFileA
BOOL ReadFile(HANDLE f, void* buf, DWORD n, DWORD* read, OVERLAPPED* o);
BOOL WriteFile(HANDLE f, const void* buf, DWORD n, DWORD* written, OVERLAPPED* o);
BOOL GetOverlappedResult(HANDLE f, OVERLAPPED* o, DWORD* n, BOOL wait);
DWORD SetFilePointer(HANDLE f, LONG dist, LONG* high, DWORD method);
DWORD GetFileSize(HANDLE f, DWORD* high);
BOOL FlushFileBuffers(HANDLE f);
BOOL CloseHandle(HANDLE h);
HANDLE FindFirstFileA(const char* pattern, WIN32_FIND_DATAA* d);
BOOL FindNextFileA(HANDLE h, WIN32_FIND_DATAA* d);
BOOL FindClose(HANDLE h);
#define FindFirstFile FindFirstFileA
#define FindNextFile FindNextFileA
DWORD GetFileAttributesA(const char* name);
#define GetFileAttributes GetFileAttributesA
BOOL CreateDirectoryA(const char* name, void* security);
#define CreateDirectory CreateDirectoryA
BOOL DeleteFileA(const char* name);
#define DeleteFile DeleteFileA
BOOL RemoveDirectoryA(const char* name);
#define RemoveDirectory RemoveDirectoryA
HANDLE CreateMutexA(void* security, BOOL owned, const char* name);
#define CreateMutex CreateMutexA
BOOL ReleaseMutex(HANDLE m);
DWORD WaitForSingleObject(HANDLE h, DWORD ms);
HANDLE CreateEventA(void* security, BOOL manual, BOOL initial, const char* name);
#define CreateEvent CreateEventA
BOOL SetEvent(HANDLE e);
BOOL ResetEvent(HANDLE e);
MMRESULT timeSetEvent(UINT delay, UINT resolution, LPTIMECALLBACK cb, DWORD_PTR user, UINT flags);
MMRESULT timeKillEvent(UINT id);
MMRESULT timeBeginPeriod(UINT p);
MMRESULT timeEndPeriod(UINT p);
BOOL SystemParametersInfoA(UINT action, UINT param, void* pv, UINT ini);
/* No registry on Linux: every key is absent, so callers take their defaults. */
LONG RegOpenKeyExA(HKEY key, const char* sub, DWORD opts, DWORD sam, HKEY* out);
LONG RegQueryValueExA(HKEY key, const char* name, DWORD* res, DWORD* type, BYTE* data, DWORD* size);
LONG RegCloseKey(HKEY key);

#define TIME_CALLBACK_FUNCTION 0
#define StrStrIA strcasestr
#define ERROR_DISK_FULL 112
#define ES_CONTINUOUS 0x80000000u
#define ES_SYSTEM_REQUIRED 1
#define ES_DISPLAY_REQUIRED 2
inline DWORD SetThreadExecutionState(DWORD) { return 0; }
BOOL GetFileSizeEx(HANDLE f, LARGE_INTEGER* size);
BOOL GetDiskFreeSpaceExA(const char* path, ULARGE_INTEGER* avail, ULARGE_INTEGER* total, ULARGE_INTEGER* free_total);
typedef struct { DWORD dwFileAttributes; FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime; DWORD nFileSizeHigh, nFileSizeLow; } WIN32_FILE_ATTRIBUTE_DATA;
enum GET_FILEEX_INFO_LEVELS { GetFileExInfoStandard };
BOOL GetFileAttributesExA(const char* name, GET_FILEEX_INFO_LEVELS level, void* out);
#define GetFileAttributesEx GetFileAttributesExA
BOOL FileTimeToLocalFileTime(const FILETIME* in, FILETIME* out);
BOOL FileTimeToSystemTime(const FILETIME* in, SYSTEMTIME* out);

/* Window calls in the shared code, on the SDL window hWnd holds (win32_compat.cpp). */
#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_SHOWMINIMIZED 2
#define SW_SHOWMAXIMIZED 3
#define SW_MINIMIZE 6
#define HWND_TOP ((HWND)0)
#define HWND_NOTOPMOST ((HWND)(intptr_t)-2)
#define SWP_NOSIZE 0x0001
#define SWP_NOMOVE 0x0002
#define SWP_NOZORDER 0x0004
#define SWP_FRAMECHANGED 0x0020
#define GWL_STYLE (-16)
#define WS_POPUP 0x80000000u
#define WS_VISIBLE 0x10000000u
#define WS_CAPTION 0x00C00000u
#define WS_SYSMENU 0x00080000u
#define WS_MINIMIZEBOX 0x00020000u
#define WS_OVERLAPPEDWINDOW 0x00CF0000u
#define MB_OK 0
#define MB_DEFBUTTON1 0
#define MB_YESNO 4
#define MB_ICONEXCLAMATION 0x30
#define IDYES 6
#define MB_ABORTRETRYIGNORE 2
#define MB_ICONHAND 0x10
#define MB_ICONERROR 0x10
#define MB_ICONSTOP 0x10
#define MB_ICONWARNING 0x30
#define IDOK 1
#define IDABORT 3
#define IDRETRY 4
#define IDIGNORE 5
BOOL ShowWindow(HWND w, int cmd);
BOOL SetWindowPos(HWND w, HWND after, int x, int y, int cx, int cy, UINT flags);
LONG SetWindowLongA(HWND w, int index, LONG value);
BOOL AdjustWindowRect(RECT* r, DWORD style, BOOL menu);
int ShowCursor(BOOL show);
HWND GetActiveWindow();
int MessageBoxA(HWND w, const char* text, const char* caption, UINT type);
#define MessageBox MessageBoxA

#define _WIN32_FIND_DATAA WIN32_FIND_DATAA
