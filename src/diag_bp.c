/*
 * diag_bp.c - Diagnose-Haltepunkte im eigenen Prozess (nur fuer die Ursachensuche).
 * Liest "bp=" aus dk2hires.ini (Hex-Adressen, kommagetrennt). An jeder Adresse steht
 * waehrend des Laufs ein INT3; der Vectored Exception Handler notiert Register und die
 * ersten Stackworte in einem Ringpuffer, fuehrt den Originalbefehl per Einzelschritt aus
 * und setzt den Haltepunkt wieder. Beim Absturz wird der Ringpuffer ins Log geschrieben.
 */
#include <windows.h>
#include "dk2hires.h"

#define MAX_BP 32
#define RING 96

typedef struct { DWORD eip, esp, ecx, edx, tid, eax, ebx, esi, edi, ebp; DWORD st[9]; DWORD dr[6][4]; DWORD ext[8]; } hit_t;

static DWORD bp_addr[MAX_BP];
static BYTE bp_orig[MAX_BP];
static int bp_count;
static int log_all;
static LONG log_max = 0x7fffffff;
static void dump_hit(LONG k);
static hit_t ring[RING];
static volatile LONG ring_pos;
static DWORD tls_pending = TLS_OUT_OF_INDEXES;

static DWORD parse_hex(const char **s)
{
    DWORD v = 0;
    const char *p = *s;
    while (*p == ' ' || *p == ',') p++;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    for (;; p++) {
        char c = *p;
        if (c >= '0' && c <= '9') v = v * 16 + (DWORD)(c - '0');
        else if (c >= 'a' && c <= 'f') v = v * 16 + (DWORD)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v = v * 16 + (DWORD)(c - 'A' + 10);
        else break;
    }
    *s = p;
    return v;
}

static void write_byte(DWORD addr, BYTE b)
{
    DWORD old;
    VirtualProtect((void *)addr, 1, PAGE_EXECUTE_READWRITE, &old);
    *(volatile BYTE *)addr = b;
    VirtualProtect((void *)addr, 1, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void *)addr, 1);
}

void diag_bp_init(const char *ini)
{
    char buf[512];
    const char *p;
    GetPrivateProfileStringA("diag", "bp", "", buf, sizeof(buf), ini);
    if (!buf[0]) return;
    log_all = GetPrivateProfileIntA("diag", "logall", 0, ini);
    log_max = GetPrivateProfileIntA("diag", "logmax", 0x7fffffff, ini);
    tls_pending = TlsAlloc();
    for (p = buf; *p && bp_count < MAX_BP;) {
        DWORD a = parse_hex(&p);
        if (a < 0x401000 || a >= 0x66c000) { if (!*p) break; p++; continue; }
        bp_addr[bp_count] = a;
        bp_orig[bp_count] = *(BYTE *)a;
        write_byte(a, 0xCC);
        dk_log("diag: Haltepunkt %08lx (Originalbyte %02x)", a, bp_orig[bp_count]);
        bp_count++;
    }
}

static int find_bp(DWORD a)
{
    int i;
    for (i = 0; i < bp_count; i++) if (bp_addr[i] == a) return i;
    return -1;
}

/* liefert 1, wenn die Ausnahme hier behandelt wurde */
int diag_bp_handle(PEXCEPTION_POINTERS ep)
{
    PCONTEXT c = ep->ContextRecord;
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    int i;
    if (!bp_count) return 0;
    if (code == EXCEPTION_BREAKPOINT) {
        DWORD a = (DWORD)ep->ExceptionRecord->ExceptionAddress;
        i = find_bp(a);
        if (i < 0) return 0;
        {
            LONG k = InterlockedIncrement(&ring_pos) - 1;
            hit_t *h = &ring[k % RING];
            DWORD *sp = (DWORD *)c->Esp;
            int j;
            h->eip = a; h->esp = c->Esp; h->ecx = c->Ecx; h->edx = c->Edx; h->tid = GetCurrentThreadId();
            h->eax = c->Eax; h->ebx = c->Ebx; h->esi = c->Esi; h->edi = c->Edi; h->ebp = c->Ebp;
            for (j = 0; j < 9; j++) h->st[j] = sp[j];
            (void)k;
            for (j = 0; j < 8; j++) h->ext[j] = 0xdeaddead;
            {
                static const DWORD offs[8] = {0x24f, 0x253, 0x277, 0x27b, 0x293, 0x0, 0x4, 0x8};
                DWORD v = sp[1];
                MEMORY_BASIC_INFORMATION mbi;
                if (v >= 0x10000 && VirtualQuery((void *)v, &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT &&
                    !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
                    v + 0x298 <= (DWORD)mbi.BaseAddress + mbi.RegionSize)
                    for (j = 0; j < 8; j++) h->ext[j] = *(DWORD *)(v + offs[j]);
            }
            for (j = 0; j < 6; j++) {
                DWORD v = sp[j + 1];
                MEMORY_BASIC_INFORMATION mbi;
                int k2;
                for (k2 = 0; k2 < 4; k2++) h->dr[j][k2] = 0xdeaddead;
                if (v >= 0x10000 && VirtualQuery((void *)v, &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT &&
                    !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
                    v + 16 <= (DWORD)mbi.BaseAddress + mbi.RegionSize)
                    for (k2 = 0; k2 < 4; k2++) h->dr[j][k2] = ((DWORD *)v)[k2];
            }
        }
        if (log_all && ring_pos <= log_max) dump_hit(ring_pos - 1);
        write_byte(a, bp_orig[i]);
        c->Eip = a;
        c->EFlags |= 0x100;
        TlsSetValue(tls_pending, (void *)(a + 0));
        TlsSetValue(tls_pending, (void *)(DWORD_PTR)(i + 1));
        return 1;
    }
    if (code == EXCEPTION_SINGLE_STEP) {
        DWORD_PTR v = (DWORD_PTR)TlsGetValue(tls_pending);
        if (!v) return 0;
        write_byte(bp_addr[v - 1], 0xCC);
        TlsSetValue(tls_pending, 0);
        c->EFlags &= ~0x100u;
        return 1;
    }
    return 0;
}

void diag_bp_dump(void)
{
    LONG end = ring_pos, k;
    LONG start = end > RING ? end - RING : 0;
    if (!bp_count) return;
    dk_log("diag: letzte %ld Haltepunkt-Treffer (aelteste zuerst)", end - start);
    for (k = start; k < end; k++) dump_hit(k);
}

static void dump_hit(LONG k)
{
    {
        hit_t *h = &ring[k % RING];
        dk_log("  #%ld tid=%lx bp=%08lx esp=%08lx ecx=%08lx edx=%08lx ret=%08lx a: %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx",
               k, h->tid, h->eip, h->esp, h->ecx, h->edx, h->st[0], h->st[1], h->st[2], h->st[3], h->st[4],
               h->st[5], h->st[6], h->st[7], h->st[8]);
        dk_log("      *a1={%08lx %08lx %08lx %08lx} *a2={%08lx %08lx %08lx %08lx} *a3={%08lx %08lx %08lx %08lx} *a4={%08lx %08lx %08lx %08lx} *a5={%08lx %08lx %08lx %08lx}",
               h->dr[0][0], h->dr[0][1], h->dr[0][2], h->dr[0][3], h->dr[1][0], h->dr[1][1], h->dr[1][2], h->dr[1][3],
               h->dr[2][0], h->dr[2][1], h->dr[2][2], h->dr[2][3], h->dr[3][0], h->dr[3][1], h->dr[3][2], h->dr[3][3],
               h->dr[4][0], h->dr[4][1], h->dr[4][2], h->dr[4][3]);
        dk_log("      eax=%08lx ebx=%08lx esi=%08lx edi=%08lx ebp=%08lx", h->eax, h->ebx, h->esi, h->edi, h->ebp);
        dk_log("      a1+{24f,253,277,27b,293,0,4,8}={%08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx}",
               h->ext[0], h->ext[1], h->ext[2], h->ext[3], h->ext[4], h->ext[5], h->ext[6], h->ext[7]);
    }
}

/* Vergleicht den Code der laufenden EXE mit der Datei auf der Platte und protokolliert
 * alle veraenderten Bereiche (zeigt, was andere Patches, z. B. GOGs patch.dll, geaendert haben). */
void diag_diff_exe(const char *when)
{
    char path[MAX_PATH];
    HANDLE f;
    DWORD size, got, i, start = 0, ranges = 0;
    BYTE *file;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    IMAGE_SECTION_HEADER *sec;
    int s;
    GetModuleFileNameA(NULL, path, MAX_PATH);
    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    size = GetFileSize(f, NULL);
    file = HeapAlloc(GetProcessHeap(), 0, size);
    if (!file) { CloseHandle(f); return; }
    ReadFile(f, file, size, &got, NULL);
    CloseHandle(f);
    dos = (IMAGE_DOS_HEADER *)file;
    nt = (IMAGE_NT_HEADERS *)(file + dos->e_lfanew);
    sec = IMAGE_FIRST_SECTION(nt);
    dk_log("diag: Code-Vergleich EXE Speicher/Datei (%s)", when);
    for (s = 0; s < nt->FileHeader.NumberOfSections; s++, sec++) {
        BYTE *mem = (BYTE *)(0x400000 + sec->VirtualAddress);
        BYTE *raw = file + sec->PointerToRawData;
        DWORD n = sec->SizeOfRawData < sec->Misc.VirtualSize ? sec->SizeOfRawData : sec->Misc.VirtualSize;
        if (!(sec->Characteristics & IMAGE_SCN_CNT_CODE)) continue;
        start = 0xffffffff;
        for (i = 0; i <= n; i++) {
            int diff = i < n && mem[i] != raw[i];
            if (diff && start == 0xffffffff) start = i;
            if (!diff && start != 0xffffffff && (i - start) > 0) {
                /* kleine Luecken zusammenfassen */
                DWORD j = i, gap = 0;
                while (j < n && gap < 4 && mem[j] == raw[j]) { j++; gap++; }
                if (j < n && gap < 4) { i = j; continue; }
                dk_log("  geaendert %08lx..%08lx (%lu Byte): %02x %02x %02x %02x %02x %02x", 0x400000 + sec->VirtualAddress + start,
                       0x400000 + sec->VirtualAddress + i - 1, i - start,
                       mem[start], mem[start + 1], mem[start + 2], mem[start + 3], mem[start + 4], mem[start + 5]);
                ranges++;
                start = 0xffffffff;
            }
        }
    }
    dk_log("diag: %lu veraenderte Bereiche", ranges);
    HeapFree(GetProcessHeap(), 0, file);
}

/* sucht in .data/.bss der EXE nach Werten, die zur Bildschirmaufloesung passen (int und float) */
static DWORD scan_w, scan_h;
static void scan_res_once(void)
{
    DWORD a;
    float fw = (float)scan_w, fh = (float)scan_h, fw2 = fw / 2, fh2 = fh / 2, asp = fw / fh;
    int n = 0;
    dk_log("diag: Suche nach Aufloesungswerten %lux%lu in 0x0068e000..0x007a7000", scan_w, scan_h);
    for (a = 0x0068e000; a < 0x007a6ffc && n < 120; a++) {
        DWORD v = *(DWORD *)a;
        float f = *(float *)a;
        const char *what = NULL;
        if (v == scan_w && *(DWORD *)(a + 4) == scan_h) what = "int w,h";
        else if (v == scan_w) what = "int w";
        else if (v == scan_h) what = "int h";
        else if (f == fw) what = "float w";
        else if (f == fh) what = "float h";
        else if (f == fw2) what = "float w/2";
        else if (f == fh2) what = "float h/2";
        else if (f > asp - 0.0005f && f < asp + 0.0005f) what = "float w/h";
        if (what) { dk_log("  %08lx: %s", a, what); n++; }
    }
}

static char dump_spec[256];

static void dump_floats(void)
{
    const char *p = dump_spec;
    while (*p) {
        DWORD a = parse_hex(&p), len = 0x40, i;
        if (*p == ':') { p++; len = parse_hex(&p); }
        if (*p == ',') p++;
        if (!a) break;
        dk_log("diag: Speicher %08lx (+%lx) als float/hex:", a, len);
        for (i = 0; i < len; i += 16) {
            float *f = (float *)(a + i);
            DWORD *d = (DWORD *)(a + i);
            char line[200];
            int fi[4], k;
            for (k = 0; k < 4; k++) fi[k] = (int)(f[k] * 1000.0f);
            wsprintfA(line, "  %08lx: %08lx %08lx %08lx %08lx | %d.%03d %d.%03d %d.%03d %d.%03d", a + i, d[0], d[1], d[2], d[3],
                      fi[0] / 1000, (fi[0] < 0 ? -fi[0] : fi[0]) % 1000, fi[1] / 1000, (fi[1] < 0 ? -fi[1] : fi[1]) % 1000,
                      fi[2] / 1000, (fi[2] < 0 ? -fi[2] : fi[2]) % 1000, fi[3] / 1000, (fi[3] < 0 ? -fi[3] : fi[3]) % 1000);
            dk_log("%s", line);
        }
    }
}

static DWORD WINAPI scan_thread(LPVOID p)
{
    Sleep((DWORD)(DWORD_PTR)p);
    if (scan_w) scan_res_once();
    if (dump_spec[0]) dump_floats();
    return 0;
}

void diag_dump_later(const char *ini, DWORD delay_ms)
{
    GetPrivateProfileStringA("diag", "Dump", "", dump_spec, sizeof(dump_spec), ini);
    if (dump_spec[0]) CloseHandle(CreateThread(NULL, 0, scan_thread, (LPVOID)(DWORD_PTR)delay_ms, 0, NULL));
}

void diag_scan_res_later(DWORD w, DWORD h, DWORD delay_ms)
{
    scan_w = w; scan_h = h;
    CloseHandle(CreateThread(NULL, 0, scan_thread, (LPVOID)(DWORD_PTR)delay_ms, 0, NULL));
}
