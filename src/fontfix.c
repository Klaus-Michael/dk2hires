/*
 * fontfix.c - Fix 4 (optional): groessere Schriften in hohen Aufloesungen.
 *
 * Die Ingame-Schriften liegen als Bitmap-Dateien FONT_INGAME<n>.bf4 vor (n = 10..48). Das Spiel
 * waehlt die Groessen unabhaengig von der Bildschirmhoehe; bei 4K sind Infozeile und
 * Menuebeschriftungen ~12-14 Pixel hoch. Der Fix biegt das Oeffnen dieser Dateien auf die
 * naechstgroessere vorhandene Schrift um (Faktor = Bildhoehe / FontRefHeight, Standard 1080,
 * 0 = aus). Die Datei bringt
 * ihre eigenen Masse mit, das Layout rechnet damit weiter. Skaliert werden nur Schriften, die nach
 * dem Wechsel in die Spielaufloesung geladen werden (gemessen: 10 und 14); die beim Programmstart
 * geladenen (16/18/24) teilt sich das Spiel mit dem 640x480-Frontend und bleiben wie sie sind.
 */
#include <windows.h>
#include "dk2hires.h"

static const int sizes[] = {10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48};
static DWORD font_ref_h;
static HANDLE (WINAPI *real_CreateFileA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static int trace_files;

static int scaled_size(int n)
{
    DWORD h = *(volatile DWORD *)0x00756ef0;
    unsigned i;
    /* Schriften, die vor dem ersten Moduswechsel geladen werden (Hoehe 0: 16/18/24), nutzt auch das
     * 640x480-Frontend; die bleiben unveraendert, sonst werden dessen Menues zu gross. */
    int want;
    if (!font_ref_h || h <= font_ref_h) return n;
    want = (int)((n * h + font_ref_h / 2) / font_ref_h);
    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
        if (sizes[i] >= want) return sizes[i];
    return sizes[sizeof(sizes) / sizeof(sizes[0]) - 1];
}

static HANDLE WINAPI my_CreateFileA(LPCSTR name, DWORD acc, DWORD share, LPSECURITY_ATTRIBUTES sa,
                                    DWORD disp, DWORD flags, HANDLE tmpl)
{
    char buf[MAX_PATH];
    LPCSTR use = name;
    HANDLE h;
    if (name && font_ref_h) {
        const char *p = name, *base = name;
        for (; *p; p++) if (*p == '\\' || *p == '/') base = p + 1;
        if (*base && CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE, base, 11, "FONT_INGAME", 11) == CSTR_EQUAL) {
            int n = 0;
            const char *q = base + 11;
            while (*q >= '0' && *q <= '9') n = n * 10 + (*q++ - '0');
            if (n > 0 && !lstrcmpiA(q, ".bf4")) {
                int m = scaled_size(n);
                if (m == n) dk_log("fontfix: %s bleibt (Bildhoehe gerade %lu)", base, *(volatile DWORD *)0x00756ef0);
                if (m != n && (int)(base - name) + 32 < MAX_PATH) {
                    int prefix = (int)(base - name);
                    lstrcpynA(buf, name, prefix + 1);
                    wsprintfA(buf + prefix, "FONT_INGAME%d.bf4", m);
                    if (GetFileAttributesA(buf) != INVALID_FILE_ATTRIBUTES) {
                        dk_log("fontfix: %s -> FONT_INGAME%d.bf4", base, m);
                        use = buf;
                    }
                }
            }
        }
    }
    h = real_CreateFileA(use, acc, share, sa, disp, flags, tmpl);
    if (trace_files)
        dk_log("datei: %s (%s, %s)", use ? use : "(null)", (acc & GENERIC_WRITE) ? "schreibend" : "lesend",
               h == INVALID_HANDLE_VALUE ? "fehlt" : "ok");
    return h;
}

int fontfix_install(const char *ini)
{
    font_ref_h = GetPrivateProfileIntA("fix", "FontRefHeight", 1080, ini);
    trace_files = GetPrivateProfileIntA("diag", "TraceFiles", 0, ini);
    if (!font_ref_h && !trace_files) return 0;
    real_CreateFileA = iat_hook("KERNEL32.dll", "CreateFileA", (void *)my_CreateFileA);
    if (!real_CreateFileA) { dk_log("fontfix: CreateFileA-Import nicht gefunden"); return 0; }
    if (font_ref_h) dk_log("fontfix: aktiv, Schriftgroesse x Bildhoehe/%lu", font_ref_h);
    if (trace_files) dk_log("diag: Dateizugriffe werden protokolliert");
    return 1;
}
