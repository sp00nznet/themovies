/*
 * The Movies - static recompilation host.
 *
 * A 32-bit host on pcrecomp's runtime/native32 (the native bridge, callbacks
 * and machine lock; see its header). What is here is only what is specific to
 * this game: where the image goes, the command line, and the fault report.
 * docs/host.md has the reasoning.
 *
 * Linked at /BASE:0x60000000 (CMakeLists.txt) so 0x00400000..0x01106000 is
 * free when main() maps the image.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <d3d9.h>

#include "native32.h"
#include "recomp_trace.h"

extern const uint32_t tm_entry_va;     /* recomp_dispatch.c */

#define TM_IMAGE_BASE 0x00400000u

static DWORD g_watchdog_s;
static int   g_headless;

/* --headless: nothing reaches the screen. Over RDP a window lands on whatever
 * device is connected (REPO_RULES section 13), so a message box prints, and
 * the game's window is real but never shown: D3D9 renders into a hidden
 * window as happily as a visible one. */
#define ARG(n) MEM32(g_esp + 4 + 4 * (n))
static const char* gstr(uint32_t va) { return va ? (const char*)(uintptr_t)va : "(null)"; }

static void shim_MessageBoxA(void) {
    fprintf(stderr, "[messagebox] %s: %s\n", gstr(ARG(2)), gstr(ARG(1)));
    g_eax = IDOK;
    g_esp += 4 + 4 * 4;
}

static void shim_MessageBoxW(void) {
    fprintf(stderr, "[messagebox] %ls: %ls\n", (const wchar_t*)(uintptr_t)ARG(2),
            (const wchar_t*)(uintptr_t)ARG(1));
    g_eax = IDOK;
    g_esp += 4 + 4 * 4;
}

/* The real CreateWindowExA minus WS_VISIBLE. Windows calls the guest's window
 * procedure from inside it (WM_CREATE...); native32's lock nests, and the
 * callback runs below these arguments on the guest stack. */
static void shim_CreateWindowExA(void) {
    HWND h = CreateWindowExA(ARG(0), (LPCSTR)(uintptr_t)ARG(1), (LPCSTR)(uintptr_t)ARG(2),
                             ARG(3) & ~WS_VISIBLE, (int)ARG(4), (int)ARG(5), (int)ARG(6),
                             (int)ARG(7), (HWND)(uintptr_t)ARG(8), (HMENU)(uintptr_t)ARG(9),
                             (HINSTANCE)(uintptr_t)ARG(10), (LPVOID)(uintptr_t)ARG(11));
    fprintf(stderr, "[headless] CreateWindowExA(\"%s\", %dx%d) from sub_%08X -> hidden hwnd %p\n",
            gstr(ARG(2)), (int)ARG(6), (int)ARG(7), g_cur_func, (void*)h);
    g_eax = (uint32_t)(uintptr_t)h;
    g_esp += 4 + 12 * 4;
}

static void shim_ShowWindow(void) {
    g_eax = 0;                         /* "was hidden", which is true */
    g_esp += 4 + 2 * 4;
}

/* Force a windowed D3D9 device. An exclusive-fullscreen device changes the
 * display mode even for a hidden window, which is exactly what must not happen
 * to an RDP session, and the game has a Fullscreen setting of its own. The
 * game reaches D3D9 only through GetProcAddress("Direct3DCreate9"), so that is
 * where it is wrapped; CreateDevice is patched in the IDirect3D9 vtable. */
typedef HRESULT (WINAPI *create_device_t)(void* self, UINT adapter, DWORD type, HWND focus,
                                          DWORD flags, D3DPRESENT_PARAMETERS* pp, void** out);
typedef void* (WINAPI *d3dcreate9_t)(UINT sdk);
static create_device_t g_real_create_device;
static d3dcreate9_t g_real_d3dcreate9;

static HRESULT WINAPI hl_CreateDevice(void* self, UINT adapter, DWORD type, HWND focus,
                                      DWORD flags, D3DPRESENT_PARAMETERS* pp, void** out) {
    fprintf(stderr, "[headless] CreateDevice adapter %u type %lu flags 0x%lX: %ux%u fmt %d x%u "
                    "ms %d swap %d depth %d/%d interval 0x%X %s -> windowed\n",
            adapter, type, flags, pp->BackBufferWidth, pp->BackBufferHeight, pp->BackBufferFormat,
            pp->BackBufferCount, pp->MultiSampleType, pp->SwapEffect,
            pp->EnableAutoDepthStencil, pp->AutoDepthStencilFormat, pp->PresentationInterval,
            pp->Windowed ? "windowed" : "FULLSCREEN");
    /* Windowed, the back buffer must match the desktop: the game asks for a
     * 16-bit A1R5G5B5 mode, which is D3DERR_NOTAVAILABLE on a 32-bit desktop. */
    pp->Windowed = TRUE;
    pp->FullScreen_RefreshRateInHz = 0;
    pp->BackBufferFormat = D3DFMT_UNKNOWN;
    HRESULT hr = g_real_create_device(self, adapter, type, focus, flags, pp, out);
    fprintf(stderr, "[headless] CreateDevice -> 0x%08lX\n", hr);
    return hr;
}

static void* WINAPI hl_Direct3DCreate9(UINT sdk) {
    void** d3d = (void**)g_real_d3dcreate9(sdk);
    if (d3d && !g_real_create_device) {
        void** vt = *(void***)d3d;
        DWORD old;
        g_real_create_device = (create_device_t)vt[16];     /* IDirect3D9::CreateDevice */
        VirtualProtect(&vt[16], 4, PAGE_READWRITE, &old);
        vt[16] = (void*)hl_CreateDevice;
        VirtualProtect(&vt[16], 4, old, &old);
    }
    fprintf(stderr, "[headless] Direct3DCreate9(%u) -> %p\n", sdk, (void*)d3d);
    if (d3d) {
        IDirect3D9* i = (IDirect3D9*)d3d;
        D3DADAPTER_IDENTIFIER9 id;
        D3DDISPLAYMODE dm;
        D3DCAPS9 caps;
        UINT n = i->lpVtbl->GetAdapterCount(i);
        i->lpVtbl->GetAdapterIdentifier(i, 0, 0, &id);
        i->lpVtbl->GetAdapterDisplayMode(i, 0, &dm);
        HRESULT hc = i->lpVtbl->GetDeviceCaps(i, 0, D3DDEVTYPE_HAL, &caps);
        fprintf(stderr, "[headless] %u adapter(s); 0: %s, desktop %ux%u fmt %d, HAL caps 0x%08lX\n",
                n, id.Description, dm.Width, dm.Height, dm.Format, hc);
    }
    return d3d;
}

static void shim_GetProcAddress(void) {
    HMODULE m = (HMODULE)(uintptr_t)ARG(0);
    const char* name = (const char*)(uintptr_t)ARG(1);
    FARPROC p = GetProcAddress(m, name);
    if (p && ARG(1) >> 16 && !strcmp(name, "Direct3DCreate9")) {
        g_real_d3dcreate9 = (d3dcreate9_t)p;
        p = (FARPROC)hl_Direct3DCreate9;
    }
    g_eax = (uint32_t)(uintptr_t)p;
    g_esp += 4 + 2 * 4;
}

/* The guest is MoviesSE.exe in the game folder, not this host. It finds its
 * data from GetModuleFileNameA -- with the host's path it looked for data\
 * under build\, found nothing, and a file-resolver fallback recursed until
 * the stack ran out -- and its hInstance (resources, window classes) comes
 * from GetModuleHandleA(NULL), which has to be the guest image. */
static char g_guest_exe[MAX_PATH], g_guest_cmdline[MAX_PATH + 3];

static void shim_GetModuleHandleA(void) {
    g_eax = ARG(0) ? (uint32_t)(uintptr_t)GetModuleHandleA((LPCSTR)(uintptr_t)ARG(0))
                   : TM_IMAGE_BASE;
    g_esp += 4 + 1 * 4;
}

static void shim_GetModuleFileNameA(void) {
    uint32_t h = ARG(0), size = ARG(2);
    char* out = (char*)(uintptr_t)ARG(1);
    if (h == 0 || h == TM_IMAGE_BASE) {
        uint32_t n = (uint32_t)strlen(g_guest_exe);
        if (size) {
            uint32_t k = n < size ? n : size - 1;
            memcpy(out, g_guest_exe, k);
            out[k] = 0;
            n = k;
        }
        g_eax = n;
    } else {
        g_eax = GetModuleFileNameA((HMODULE)(uintptr_t)h, out, size);
    }
    g_esp += 4 + 3 * 4;
}

static void shim_GetCommandLineA(void) {
    g_eax = (uint32_t)(uintptr_t)g_guest_cmdline;
    g_esp += 4;
}

#define GUEST_SHIMS \
    { "GetModuleHandleA", shim_GetModuleHandleA }, \
    { "GetModuleFileNameA", shim_GetModuleFileNameA }, \
    { "GetCommandLineA", shim_GetCommandLineA }

static native32_shim_t g_shims[] = { GUEST_SHIMS };

static native32_shim_t g_headless_shims[] = {
    GUEST_SHIMS,
    { "GetProcAddress", shim_GetProcAddress },
    { "MessageBoxA", shim_MessageBoxA },
    { "MessageBoxW", shim_MessageBoxW },
    { "CreateWindowExA", shim_CreateWindowExA },
    { "ShowWindow", shim_ShowWindow },
};

recomp_func_t recomp_lookup_manual(uint32_t va) { (void)va; return NULL; }

void recomp_not_lifted(uint32_t va) {
    fprintf(stderr,
        "\n[not-lifted] sub_%08X  (called from 0x%08X)\n"
        "  Widen the closure:  py -3 run_lift.py --roots 0x%08X  (or --max N, or --all)\n",
        va, g_cur_func, va);
    recomp_dump_trace("not-lifted");
    native32_dump_icalls(8);
    fflush(stderr);
    TerminateProcess(GetCurrentProcess(), 2);
}

/* Added after native32's own handler, so callbacks are resolved first and
 * only real faults get here. */
static LONG CALLBACK crash(EXCEPTION_POINTERS* ep) {
    EXCEPTION_RECORD* r = ep->ExceptionRecord;
    if ((r->ExceptionCode & 0xF0000000u) != 0xC0000000u) return EXCEPTION_CONTINUE_SEARCH;
    fprintf(stderr, "\n=== fault 0x%08lX at 0x%p ===\n", r->ExceptionCode, r->ExceptionAddress);
    if (r->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && r->NumberParameters >= 2) {
        ULONG_PTR op = r->ExceptionInformation[0];
        uint32_t at = (uint32_t)r->ExceptionInformation[1];
        fprintf(stderr, "  %s of 0x%08X%s\n", op == 0 ? "read" : op == 1 ? "write" : "execute", at,
                native32_in_guest(at) ? " (inside the guest image)" : at < 0x10000 ? " (null/low)" : "");
    }
    recomp_trace_flush();
    fprintf(stderr, "  in lifted sub_%08X, last native call %s\n", g_cur_func, g_cur_import);
    fprintf(stderr, "  eax=%08X ecx=%08X edx=%08X ebx=%08X esp=%08X ebp=%08X esi=%08X edi=%08X\n",
            g_eax, g_ecx, g_edx, g_ebx, g_esp, g_ebp, g_esi, g_edi);
    native32_dump_icalls(12);
    recomp_dump_trace("fault");
    fflush(stderr);
    TerminateProcess(GetCurrentProcess(), 3);
    return EXCEPTION_CONTINUE_SEARCH;
}

static DWORD WINAPI watchdog(LPVOID unused) {
    (void)unused;
    Sleep(g_watchdog_s * 1000);
    fprintf(stderr, "\n[watchdog] %lu s: in sub_%08X, last native call %s, %u indirect calls\n",
            g_watchdog_s, g_cur_func, g_cur_import, g_icall_count);
    native32_dump_icalls(8);
    fflush(stderr);
    TerminateProcess(GetCurrentProcess(), 4);
    return 0;
}

int main(int argc, char** argv) {
    const char* exe = "work\\MoviesSE.unpacked.exe";
    const char* game = "game";
    char exe_full[MAX_PATH];
    int run = 0;
    for (int i = 1; i < argc; i++) {
        int n = recomp_trace_arg(argc, argv, i);
        if (n) { i += n - 1; continue; }
        if (!strcmp(argv[i], "--run")) run = 1;
        else if (!strcmp(argv[i], "--headless")) g_headless = 1;
        else if (!strcmp(argv[i], "--exe") && i + 1 < argc) exe = argv[++i];
        else if (!strcmp(argv[i], "--game") && i + 1 < argc) game = argv[++i];
        else if (!strcmp(argv[i], "--watchdog") && i + 1 < argc) g_watchdog_s = strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--native-trace")) native32_trace_native = 1;
        else if (!strcmp(argv[i], "--callbacks")) native32_trace_callbacks = 1;
        else {
            printf("usage: themovies [--run] [--headless] [--exe work\\MoviesSE.unpacked.exe] [--game game]\n"
                   "                 [--watchdog S] [--native-trace] [--callbacks]\n");
            recomp_trace_help();
            return argv[i][1] == 'h' || argv[i][2] == 'h' ? 0 : 1;
        }
    }
    GetFullPathNameA(exe, MAX_PATH, exe_full, NULL);
    {
        char gd[MAX_PATH];
        GetFullPathNameA(game, MAX_PATH, gd, NULL);
        _snprintf(g_guest_exe, sizeof g_guest_exe - 1, "%s\\MoviesSE.exe", gd);
        _snprintf(g_guest_cmdline, sizeof g_guest_cmdline - 1, "\"%s\"", g_guest_exe);
    }

    native32_init();
    AddVectoredExceptionHandler(0, crash);
    printf("The Movies recomp host\n  lifted functions in dispatch: %u\n", recomp_dispatch_count);

    uint32_t span = native32_map(exe_full, TM_IMAGE_BASE);
    if (!span) { fprintf(stderr, "cannot map %s at 0x%08X\n", exe_full, TM_IMAGE_BASE); return 1; }
    printf("  mapped %s: 0x%08X-0x%08X\n", exe, TM_IMAGE_BASE, TM_IMAGE_BASE + span);
    if (g_headless ? native32_bind(TM_IMAGE_BASE, g_headless_shims,
                                   (int)(sizeof g_headless_shims / sizeof g_headless_shims[0]))
                   : native32_bind(TM_IMAGE_BASE, g_shims, (int)(sizeof g_shims / sizeof g_shims[0])))
        return 1;
    printf("  guest exe %s\n", g_guest_exe);

    if (!run) {
        printf("\n(dry run: image mapped and bound; --run enters 0x%08X)\n", tm_entry_va);
        return 0;
    }
    /* The game opens Data\ relative to its working directory, like the original. */
    if (!SetCurrentDirectoryA(game)) { fprintf(stderr, "cannot enter %s\n", game); return 1; }
    if (g_watchdog_s) CloseHandle(CreateThread(NULL, 0, watchdog, NULL, 0, NULL));
    printf("  entering 0x%08X\n\n", tm_entry_va);
    fflush(stdout);
    native32_call_guest(tm_entry_va, 0, NULL);
    printf("\nentry returned eax=%08X\n", g_eax);
    return (int)g_eax;
}
