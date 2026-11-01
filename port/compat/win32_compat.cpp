// Win32 API subset on POSIX; see win32_compat.h. Files resolve case-insensitively with either slash, because the
// game's data is addressed by Windows-style names (hit-and-run-linux's radcore drive did the same).
// Uses malloc only: this runs inside the game's memory manager setup.
#include <windows.h>
#include <SDL.h>
#include <dirent.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static __thread DWORD s_last_error = 0;
DWORD GetLastError() { return s_last_error; }
void SetLastError(DWORD e) { s_last_error = e; }

void OutputDebugStringA(const char* s) { fputs(s, stderr); }
void Sleep(DWORD ms) { struct timespec t = { (time_t)(ms / 1000), (long)(ms % 1000) * 1000000L }; nanosleep(&t, NULL); }
static unsigned long long now_ns() { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000000000ull + t.tv_nsec; }
DWORD timeGetTime() { return (DWORD)(now_ns() / 1000000ull); }
DWORD GetTickCount() { return timeGetTime(); }
BOOL QueryPerformanceCounter(LARGE_INTEGER* v) { v->QuadPart = now_ns(); return TRUE; }
BOOL QueryPerformanceFrequency(LARGE_INTEGER* v) { v->QuadPart = 1000000000ll; return TRUE; }

// The game looks for Data/ next to its "module". KISAK_GAME_DIR points that at a separate game install.
DWORD GetModuleFileNameA(void*, char* out, DWORD size)
{
	const char* dir = getenv("KISAK_GAME_DIR");
	int n;
	if (dir && *dir) n = snprintf(out, size, "%s/KisakThug", dir);
	else { ssize_t r = readlink("/proc/self/exe", out, size - 1); n = r < 0 ? 0 : (int)r; out[n] = 0; }
	return n < 0 ? 0 : (DWORD)n;
}
BOOL PathRemoveFileSpecA(char* path)
{
	char* a = strrchr(path, '/'); char* b = strrchr(path, '\\'); char* c = a > b ? a : b;
	if (!c) return FALSE;
	*c = 0;
	return TRUE;
}
const char* PathFindFileNameA(const char* path)
{
	const char* a = strrchr(path, '/'); const char* b = strrchr(path, '\\'); const char* c = a > b ? a : b;
	return c ? c + 1 : path;
}
const char* PathFindExtensionA(const char* path)
{
	const char* name = PathFindFileNameA(path);
	const char* dot = strrchr(name, '.');
	return dot ? dot : name + strlen(name);
}

// Maps a Windows-style path onto the real file: separators to '/', each component matched ignoring case. A component
// that doesn't exist (a file about to be created) keeps its spelling.
static void resolve_path(const char* in, char* out, size_t size)
{
	char tmp[1024];
	snprintf(tmp, sizeof(tmp), "%s", in);
	for (char* p = tmp; *p; ++p) if (*p == '\\') *p = '/';
	struct stat st;
	if (stat(tmp, &st) == 0) { snprintf(out, size, "%s", tmp); return; }
	const char* rest = tmp;
	size_t len = 0;
	out[0] = 0;
	if (*rest == '/') { snprintf(out, size, "/"); len = 1; while (*rest == '/') ++rest; }
	while (*rest)
	{
		const char* end = strchr(rest, '/');
		size_t n = end ? (size_t)(end - rest) : strlen(rest);
		char part[256];
		if (n >= sizeof(part)) n = sizeof(part) - 1;
		memcpy(part, rest, n); part[n] = 0;
		DIR* d = opendir(len ? out : ".");
		if (d)
		{
			while (struct dirent* e = readdir(d))
				if (!strcasecmp(e->d_name, part)) { snprintf(part, sizeof(part), "%s", e->d_name); break; }
			closedir(d);
		}
		len += snprintf(out + len, size - len, "%s%s", (len && out[len - 1] != '/') ? "/" : "", part);
		rest += n;
		while (*rest == '/') ++rest;
	}
}
BOOL PathFileExistsA(const char* path)
{
	char real[1024]; struct stat st;
	resolve_path(path, real, sizeof(real));
	return stat(real, &st) == 0;
}

// ---- handles: tagged so CloseHandle and WaitForSingleObject work on any of them ----
enum { H_FILE = 0x46494C45, H_FIND, H_MUTEX, H_EVENT };
struct Handle { int kind; };
struct FileH : Handle { int fd; };
struct MutexH : Handle { pthread_mutex_t m; };
struct EventH : Handle { pthread_mutex_t m; pthread_cond_t c; bool manual, set; };
struct FindH : Handle { DIR* dir; char dirpath[1024]; char pattern[256]; };

static DWORD errno_to_win()
{
	switch (errno) { case ENOENT: return ERROR_FILE_NOT_FOUND; case ENOTDIR: return ERROR_PATH_NOT_FOUND; case EEXIST: return ERROR_ALREADY_EXISTS; default: return 1; }
}

HANDLE CreateFileA(const char* name, DWORD access, DWORD, void*, DWORD disposition, DWORD, HANDLE)
{
	char real[1024];
	resolve_path(name, real, sizeof(real));
	int fl = (access & GENERIC_WRITE) ? ((access & GENERIC_READ) ? O_RDWR : O_WRONLY) : O_RDONLY;
	switch (disposition)
	{
	case CREATE_NEW: fl |= O_CREAT | O_EXCL; break;
	case CREATE_ALWAYS: fl |= O_CREAT | O_TRUNC; break;
	case OPEN_ALWAYS: fl |= O_CREAT; break;
	case TRUNCATE_EXISTING: fl |= O_TRUNC; break;
	}
	int fd = open(real, fl, 0644);
	if (fd < 0) { s_last_error = errno_to_win(); return INVALID_HANDLE_VALUE; }
	FileH* h = (FileH*)malloc(sizeof(FileH));
	h->kind = H_FILE; h->fd = fd;
	return h;
}
// Overlapped reads complete immediately; the callers poll HasOverlappedIoCompleted / GetOverlappedResult.
BOOL ReadFile(HANDLE f, void* buf, DWORD n, DWORD* read_out, OVERLAPPED* o)
{
	FileH* h = (FileH*)f;
	ssize_t r = o ? pread(h->fd, buf, n, (off_t)o->Offset) : read(h->fd, buf, n);
	if (r < 0) { s_last_error = errno_to_win(); if (o) o->Internal = 1; return FALSE; }
	if (read_out) *read_out = (DWORD)r;
	if (o)
	{
		o->Internal = 0; o->InternalHigh = (DWORD)r;
		if (r == 0 && n != 0) { s_last_error = ERROR_HANDLE_EOF; return FALSE; }
	}
	return TRUE;
}
BOOL WriteFile(HANDLE f, const void* buf, DWORD n, DWORD* written, OVERLAPPED* o)
{
	FileH* h = (FileH*)f;
	ssize_t r = o ? pwrite(h->fd, buf, n, (off_t)o->Offset) : write(h->fd, buf, n);
	if (r < 0) { s_last_error = errno_to_win(); return FALSE; }
	if (written) *written = (DWORD)r;
	if (o) { o->Internal = 0; o->InternalHigh = (DWORD)r; }
	return TRUE;
}
BOOL GetOverlappedResult(HANDLE, OVERLAPPED* o, DWORD* n, BOOL)
{
	if (n) *n = o->InternalHigh;
	if (o->InternalHigh == 0) { s_last_error = ERROR_HANDLE_EOF; return FALSE; }
	return TRUE;
}
DWORD SetFilePointer(HANDLE f, LONG dist, LONG* high, DWORD method)
{
	off_t off = lseek(((FileH*)f)->fd, dist, method == FILE_BEGIN ? SEEK_SET : method == FILE_END ? SEEK_END : SEEK_CUR);
	if (high) *high = 0;
	return off < 0 ? INVALID_SET_FILE_POINTER : (DWORD)off;
}
DWORD GetFileSize(HANDLE f, DWORD* high)
{
	struct stat st;
	if (high) *high = 0;
	return fstat(((FileH*)f)->fd, &st) ? INVALID_FILE_SIZE : (DWORD)st.st_size;
}
BOOL FlushFileBuffers(HANDLE f) { return fsync(((FileH*)f)->fd) == 0; }
BOOL CloseHandle(HANDLE v)
{
	if (!v || v == INVALID_HANDLE_VALUE) return FALSE;
	Handle* h = (Handle*)v;
	if (h->kind == H_FILE) close(((FileH*)h)->fd);
	free(h);
	return TRUE;
}

static BOOL find_next(FindH* h, WIN32_FIND_DATAA* d)
{
	while (struct dirent* e = readdir(h->dir))
	{
		if (fnmatch(h->pattern, e->d_name, FNM_CASEFOLD)) continue;
		char full[1400]; struct stat st;
		snprintf(full, sizeof(full), "%s/%s", h->dirpath, e->d_name);
		if (stat(full, &st)) continue;
		memset(d, 0, sizeof(*d));
		d->dwFileAttributes = S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
		d->nFileSizeLow = (DWORD)st.st_size;
		// FILETIME: 100 ns units since 1601
		unsigned long long t = (st.st_mtime + 11644473600ull) * 10000000ull;
		d->ftLastWriteTime.dwLowDateTime = (DWORD)t; d->ftLastWriteTime.dwHighDateTime = (DWORD)(t >> 32);
		d->ftCreationTime = d->ftLastAccessTime = d->ftLastWriteTime;
		snprintf(d->cFileName, sizeof(d->cFileName), "%s", e->d_name);
		return TRUE;
	}
	s_last_error = 18; // ERROR_NO_MORE_FILES
	return FALSE;
}
HANDLE FindFirstFileA(const char* pattern, WIN32_FIND_DATAA* d)
{
	char dirbuf[1024];
	snprintf(dirbuf, sizeof(dirbuf), "%s", pattern);
	const char* name = PathFindFileNameA(pattern);
	if (!PathRemoveFileSpecA(dirbuf)) snprintf(dirbuf, sizeof(dirbuf), ".");
	FindH* h = (FindH*)malloc(sizeof(FindH));
	h->kind = H_FIND;
	resolve_path(dirbuf, h->dirpath, sizeof(h->dirpath));
	// "*.*" matches every name on Windows, including ones without a dot.
	snprintf(h->pattern, sizeof(h->pattern), "%s", strcmp(name, "*.*") ? name : "*");
	h->dir = opendir(h->dirpath);
	if (!h->dir) { free(h); s_last_error = ERROR_PATH_NOT_FOUND; return INVALID_HANDLE_VALUE; }
	if (!find_next(h, d)) { closedir(h->dir); free(h); s_last_error = ERROR_FILE_NOT_FOUND; return INVALID_HANDLE_VALUE; }
	return h;
}
BOOL FindNextFileA(HANDLE h, WIN32_FIND_DATAA* d) { return find_next((FindH*)h, d); }
BOOL FindClose(HANDLE v) { if (!v || v == INVALID_HANDLE_VALUE) return FALSE; FindH* h = (FindH*)v; closedir(h->dir); free(h); return TRUE; }

DWORD GetFileAttributesA(const char* name)
{
	char real[1024]; struct stat st;
	resolve_path(name, real, sizeof(real));
	if (stat(real, &st)) { s_last_error = errno_to_win(); return INVALID_FILE_ATTRIBUTES; }
	return S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
}
BOOL CreateDirectoryA(const char* name, void*)
{
	char real[1024];
	resolve_path(name, real, sizeof(real));
	if (mkdir(real, 0755)) { s_last_error = errno_to_win(); return FALSE; }
	return TRUE;
}
BOOL DeleteFileA(const char* name)
{
	char real[1024];
	resolve_path(name, real, sizeof(real));
	if (unlink(real)) { s_last_error = errno_to_win(); return FALSE; }
	return TRUE;
}
BOOL RemoveDirectoryA(const char* name)
{
	char real[1024];
	resolve_path(name, real, sizeof(real));
	if (rmdir(real)) { s_last_error = errno_to_win(); return FALSE; }
	return TRUE;
}

// ---- synchronisation ----
HANDLE CreateMutexA(void*, BOOL owned, const char*)
{
	MutexH* h = (MutexH*)malloc(sizeof(MutexH));
	h->kind = H_MUTEX;
	pthread_mutexattr_t a;
	pthread_mutexattr_init(&a);
	pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE); // Win32 mutexes are recursive
	pthread_mutex_init(&h->m, &a);
	pthread_mutexattr_destroy(&a);
	if (owned) pthread_mutex_lock(&h->m);
	return h;
}
BOOL ReleaseMutex(HANDLE v) { if (!v) return FALSE; return pthread_mutex_unlock(&((MutexH*)v)->m) == 0; }
HANDLE CreateEventA(void*, BOOL manual, BOOL initial, const char*)
{
	EventH* h = (EventH*)malloc(sizeof(EventH));
	h->kind = H_EVENT; h->manual = manual; h->set = initial;
	pthread_mutex_init(&h->m, NULL);
	pthread_cond_init(&h->c, NULL);
	return h;
}
BOOL SetEvent(HANDLE v) { EventH* h = (EventH*)v; pthread_mutex_lock(&h->m); h->set = true; pthread_cond_broadcast(&h->c); pthread_mutex_unlock(&h->m); return TRUE; }
BOOL ResetEvent(HANDLE v) { EventH* h = (EventH*)v; pthread_mutex_lock(&h->m); h->set = false; pthread_mutex_unlock(&h->m); return TRUE; }
static void deadline(struct timespec* t, DWORD ms)
{
	clock_gettime(CLOCK_REALTIME, t);
	t->tv_sec += ms / 1000; t->tv_nsec += (ms % 1000) * 1000000L;
	if (t->tv_nsec >= 1000000000L) { t->tv_sec++; t->tv_nsec -= 1000000000L; }
}
DWORD WaitForSingleObject(HANDLE v, DWORD ms)
{
	if (!v) { s_last_error = 6; return 0xFFFFFFFFu; } // WAIT_FAILED, ERROR_INVALID_HANDLE: the game does wait on a null mutex
	Handle* base = (Handle*)v;
	struct timespec t;
	if (ms != INFINITE) deadline(&t, ms);
	if (base->kind == H_MUTEX)
	{
		MutexH* h = (MutexH*)v;
		int r = ms == INFINITE ? pthread_mutex_lock(&h->m) : pthread_mutex_timedlock(&h->m, &t);
		return r ? WAIT_TIMEOUT : WAIT_OBJECT_0;
	}
	if (base->kind == H_EVENT)
	{
		EventH* h = (EventH*)v;
		int r = 0;
		pthread_mutex_lock(&h->m);
		while (!h->set && r == 0) r = ms == INFINITE ? pthread_cond_wait(&h->c, &h->m) : pthread_cond_timedwait(&h->c, &h->m, &t);
		bool got = h->set;
		if (got && !h->manual) h->set = false;
		pthread_mutex_unlock(&h->m);
		return got ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
	}
	return WAIT_OBJECT_0;
}

// ---- multimedia timers: each runs its callback on its own thread, like winmm's timer thread ----
struct TimerH { UINT delay; LPTIMECALLBACK cb; DWORD_PTR user; UINT flags; UINT id; volatile bool stop; pthread_t thread; TimerH* next; };
static pthread_mutex_t s_timers_lock = PTHREAD_MUTEX_INITIALIZER;
static TimerH* s_timers = NULL;
static UINT s_next_timer = 1;
static void* timer_thread(void* arg)
{
	TimerH* t = (TimerH*)arg;
	do
	{
		Sleep(t->delay);
		if (!t->stop) t->cb(t->id, 0, t->user, 0, 0);
	} while (!t->stop && (t->flags & TIME_PERIODIC));
	return NULL;
}
MMRESULT timeSetEvent(UINT delay, UINT, LPTIMECALLBACK cb, DWORD_PTR user, UINT flags)
{
	TimerH* t = (TimerH*)malloc(sizeof(TimerH));
	t->delay = delay; t->cb = cb; t->user = user; t->flags = flags; t->stop = false;
	pthread_mutex_lock(&s_timers_lock);
	t->id = s_next_timer++; t->next = s_timers; s_timers = t;
	pthread_mutex_unlock(&s_timers_lock);
	pthread_create(&t->thread, NULL, timer_thread, t);
	return t->id;
}
MMRESULT timeKillEvent(UINT id)
{
	pthread_mutex_lock(&s_timers_lock);
	TimerH** p = &s_timers;
	while (*p && (*p)->id != id) p = &(*p)->next;
	TimerH* t = *p;
	if (t) *p = t->next;
	pthread_mutex_unlock(&s_timers_lock);
	if (!t) return 1;
	t->stop = true;
	if (!pthread_equal(t->thread, pthread_self())) pthread_join(t->thread, NULL); else pthread_detach(t->thread);
	free(t);
	return 0;
}
MMRESULT timeBeginPeriod(UINT) { return 0; }
MMRESULT timeEndPeriod(UINT) { return 0; }
BOOL SystemParametersInfoA(UINT, UINT, void*, UINT) { return TRUE; }
LONG RegOpenKeyExA(HKEY, const char*, DWORD, DWORD, HKEY*) { return ERROR_FILE_NOT_FOUND; }
LONG RegQueryValueExA(HKEY, const char*, DWORD*, DWORD*, BYTE*, DWORD*) { return ERROR_FILE_NOT_FOUND; }
LONG RegCloseKey(HKEY) { return 0; }

// ---- files, more ----
BOOL GetFileSizeEx(HANDLE f, LARGE_INTEGER* size)
{
	struct stat st;
	if (fstat(((FileH*)f)->fd, &st)) return FALSE;
	size->QuadPart = st.st_size;
	return TRUE;
}
BOOL GetDiskFreeSpaceExA(const char* path, ULARGE_INTEGER* avail, ULARGE_INTEGER* total, ULARGE_INTEGER* free_total)
{
	struct statvfs v;
	if (statvfs(path ? path : ".", &v)) return FALSE;
	if (avail) avail->QuadPart = (unsigned long long)v.f_bavail * v.f_frsize;
	if (total) total->QuadPart = (unsigned long long)v.f_blocks * v.f_frsize;
	if (free_total) free_total->QuadPart = (unsigned long long)v.f_bfree * v.f_frsize;
	return TRUE;
}
BOOL GetFileAttributesExA(const char* name, GET_FILEEX_INFO_LEVELS, void* out)
{
	char real[1024]; struct stat st;
	resolve_path(name, real, sizeof(real));
	if (stat(real, &st)) { s_last_error = errno_to_win(); return FALSE; }
	WIN32_FILE_ATTRIBUTE_DATA* d = (WIN32_FILE_ATTRIBUTE_DATA*)out;
	memset(d, 0, sizeof(*d));
	d->dwFileAttributes = S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
	d->nFileSizeLow = (DWORD)st.st_size;
	unsigned long long t = (st.st_mtime + 11644473600ull) * 10000000ull;
	d->ftLastWriteTime.dwLowDateTime = (DWORD)t; d->ftLastWriteTime.dwHighDateTime = (DWORD)(t >> 32);
	d->ftCreationTime = d->ftLastAccessTime = d->ftLastWriteTime;
	return TRUE;
}
// A FILETIME stays UTC here; FileTimeToSystemTime does the local conversion so the pair gives local time as on Windows.
BOOL FileTimeToLocalFileTime(const FILETIME* in, FILETIME* out) { *out = *in; return TRUE; }
BOOL FileTimeToSystemTime(const FILETIME* in, SYSTEMTIME* out)
{
	unsigned long long t = ((unsigned long long)in->dwHighDateTime << 32) | in->dwLowDateTime;
	time_t secs = (time_t)(t / 10000000ull - 11644473600ull);
	struct tm tmv;
	localtime_r(&secs, &tmv);
	out->wYear = tmv.tm_year + 1900; out->wMonth = tmv.tm_mon + 1; out->wDayOfWeek = tmv.tm_wday; out->wDay = tmv.tm_mday;
	out->wHour = tmv.tm_hour; out->wMinute = tmv.tm_min; out->wSecond = tmv.tm_sec; out->wMilliseconds = 0;
	return TRUE;
}

// ---- window ----
BOOL ShowWindow(HWND w, int cmd)
{
	SDL_Window* s = (SDL_Window*)w;
	if (!s) return FALSE;
	switch (cmd)
	{
	case SW_HIDE: SDL_HideWindow(s); break;
	case SW_SHOWMINIMIZED: case SW_MINIMIZE: SDL_MinimizeWindow(s); break;
	default: SDL_ShowWindow(s); break;
	}
	return TRUE;
}
BOOL SetWindowPos(HWND w, HWND, int x, int y, int cx, int cy, UINT flags)
{
	SDL_Window* s = (SDL_Window*)w;
	if (!s) return FALSE;
	if (!(flags & SWP_NOSIZE)) SDL_SetWindowSize(s, cx, cy);
	if (!(flags & SWP_NOMOVE)) SDL_SetWindowPosition(s, x, y);
	return TRUE;
}
LONG SetWindowLongA(HWND w, int index, LONG value)
{
	// The only style change the game makes is borderless <-> decorated.
	if (w && index == GWL_STYLE) SDL_SetWindowBordered((SDL_Window*)w, (value & WS_POPUP) ? SDL_FALSE : SDL_TRUE);
	return 0;
}
BOOL AdjustWindowRect(RECT*, DWORD, BOOL) { return TRUE; } // SDL sizes are client sizes already
int ShowCursor(BOOL show) { SDL_ShowCursor(show ? SDL_ENABLE : SDL_DISABLE); return show ? 0 : -1; }
HWND GetActiveWindow() { return (HWND)SDL_GetKeyboardFocus(); }
int MessageBoxA(HWND w, const char* text, const char* caption, UINT type)
{
	fprintf(stderr, "[%s] %s\n", caption ? caption : "", text ? text : "");
	if (getenv("KISAK_NO_MSGBOX")) return (type & 0xF) == MB_ABORTRETRYIGNORE ? IDIGNORE : IDOK;
	if ((type & 0xF) == MB_ABORTRETRYIGNORE)
	{
		const SDL_MessageBoxButtonData buttons[] = { { 0, IDABORT, "Abort" }, { 0, IDRETRY, "Retry" }, { 0, IDIGNORE, "Ignore" } };
		const SDL_MessageBoxData data = { SDL_MESSAGEBOX_ERROR, (SDL_Window*)w, caption, text, 3, buttons, NULL };
		int id = IDIGNORE;
		SDL_ShowMessageBox(&data, &id);
		return id;
	}
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, caption, text, (SDL_Window*)w);
	return IDOK;
}
