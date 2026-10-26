/*
 * hooks.c - kleine Hilfen zum sicheren Patchen von Code in DKII-DX.exe.
 * Jeder Patch prueft vorher die erwarteten Originalbytes. Stimmt etwas nicht, wird nichts
 * geschrieben (anderer Build, anderer Patch schon aktiv).
 */
#include <windows.h>
#include "dk2hires.h"

int bytes_match(DWORD addr, const BYTE *expect, int len)
{
    int i;
    for (i = 0; i < len; i++)
        if (((const BYTE *)addr)[i] != expect[i]) return 0;
    return 1;
}

int write_code(DWORD addr, const BYTE *bytes, int len)
{
    DWORD old;
    int i;
    if (!VirtualProtect((void *)addr, (SIZE_T)len, PAGE_EXECUTE_READWRITE, &old)) return 0;
    for (i = 0; i < len; i++) ((volatile BYTE *)addr)[i] = bytes[i];
    VirtualProtect((void *)addr, (SIZE_T)len, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void *)addr, (SIZE_T)len);
    return 1;
}

/* schreibt "jmp target" an addr und fuellt bis len mit NOP auf */
int write_jmp(DWORD addr, DWORD target, int len)
{
    BYTE buf[32];
    int i;
    DWORD rel = target - (addr + 5);
    if (len < 5 || len > 32) return 0;
    buf[0] = 0xE9;
    buf[1] = (BYTE)rel; buf[2] = (BYTE)(rel >> 8); buf[3] = (BYTE)(rel >> 16); buf[4] = (BYTE)(rel >> 24);
    for (i = 5; i < len; i++) buf[i] = 0x90;
    return write_code(addr, buf, len);
}

/* ersetzt einen Eintrag in der Importtabelle der EXE; liefert den alten Zeiger */
void *iat_hook(const char *dll, const char *func, void *repl)
{
    BYTE *base = (BYTE *)GetModuleHandleA(NULL);
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *imp;
    if (!dir->VirtualAddress) return NULL;
    for (imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir->VirtualAddress); imp->Name; imp++) {
        IMAGE_THUNK_DATA *names, *addrs;
        if (lstrcmpiA((const char *)(base + imp->Name), dll)) continue;
        names = (IMAGE_THUNK_DATA *)(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        addrs = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
        for (; names->u1.AddressOfData; names++, addrs++) {
            IMAGE_IMPORT_BY_NAME *ibn;
            if (names->u1.Ordinal & IMAGE_ORDINAL_FLAG32) continue;
            ibn = (IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData);
            if (!lstrcmpA((const char *)ibn->Name, func)) {
                void *old = (void *)addrs->u1.Function;
                DWORD prot;
                VirtualProtect(&addrs->u1.Function, 4, PAGE_READWRITE, &prot);
                addrs->u1.Function = (DWORD)repl;
                VirtualProtect(&addrs->u1.Function, 4, prot, &prot);
                return old;
            }
        }
    }
    return NULL;
}
