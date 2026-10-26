/*
 * dsound_proxy.c - leitet alle Exporte von dsound.dll an die System-dsound.dll weiter.
 *
 * Teil des DK2-Hochaufloesungs-Fixes (dk2hires). Die Proxy-Rolle ist nur der Lademechanismus:
 * DKII-DX.exe importiert dsound.dll statisch, also laeuft unser DllMain vor dem ersten
 * Spielcode. Die echte dsound.dll wird beim ersten Aufruf aus dem Systemverzeichnis geladen.
 */
#include <windows.h>

typedef HRESULT (WINAPI *fn3)(void *, void *, void *);
typedef HRESULT (WINAPI *fn2)(void *, void *);
typedef HRESULT (WINAPI *fn0)(void);
typedef HRESULT (WINAPI *fn10)(void *, void *, void *, void *, HWND, DWORD, void *, void *, void *, void *);

static HMODULE real_dsound;

static FARPROC real_proc(const char *name)
{
    if (!real_dsound) {
        char path[MAX_PATH];
        UINT n = GetSystemDirectoryA(path, MAX_PATH - 12);
        if (n == 0 || n >= MAX_PATH - 12) return NULL;
        lstrcatA(path, "\\dsound.dll");
        real_dsound = LoadLibraryA(path);
        if (!real_dsound) return NULL;
    }
    return GetProcAddress(real_dsound, name);
}

#define E_NOREAL ((HRESULT)0x80004005L) /* E_FAIL */

#define FWD3(name) \
    HRESULT WINAPI px_##name(void *a, void *b, void *c) { \
        fn3 f = (fn3)real_proc(#name); return f ? f(a, b, c) : E_NOREAL; }
#define FWD2(name) \
    HRESULT WINAPI px_##name(void *a, void *b) { \
        fn2 f = (fn2)real_proc(#name); return f ? f(a, b) : E_NOREAL; }
#define FWD0(name) \
    HRESULT WINAPI px_##name(void) { \
        fn0 f = (fn0)real_proc(#name); return f ? f() : E_NOREAL; }

FWD3(DirectSoundCreate)
FWD2(DirectSoundEnumerateA)
FWD2(DirectSoundEnumerateW)
FWD0(DllCanUnloadNow)
FWD3(DllGetClassObject)
FWD3(DirectSoundCaptureCreate)
FWD2(DirectSoundCaptureEnumerateA)
FWD2(DirectSoundCaptureEnumerateW)
FWD2(GetDeviceID)
FWD3(DirectSoundCreate8)
FWD3(DirectSoundCaptureCreate8)
FWD0(DllRegisterServer)
FWD0(DllUnregisterServer)

HRESULT WINAPI px_DirectSoundFullDuplexCreate(void *a, void *b, void *c, void *d, HWND e, DWORD f,
                                              void *g, void *h, void *i, void *j)
{
    fn10 fn = (fn10)real_proc("DirectSoundFullDuplexCreate");
    return fn ? fn(a, b, c, d, e, f, g, h, i, j) : E_NOREAL;
}
