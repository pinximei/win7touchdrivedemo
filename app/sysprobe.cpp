// sysprobe.cpp -- dump Win7 touch-related GetSystemMetrics + foreground hwnd.
// Quick replacement for PowerShell-based probing (which is way too slow on Win7).
#include <windows.h>
#include <stdio.h>

int main()
{
    int dig = GetSystemMetrics(SM_DIGITIZER);   // 94 on Win10, 94 = MAXIMUMTOUCHES on Win7
    int sm94 = GetSystemMetrics(94);
    int sm95 = GetSystemMetrics(95);
    int cx = GetSystemMetrics(SM_CXSCREEN);
    int cy = GetSystemMetrics(SM_CYSCREEN);

    printf("Screen %dx%d\n", cx, cy);
    printf("SM_DIGITIZER (94) = 0x%X\n", sm94);
    printf("SM_MAXIMUMTOUCHES (95) = %d\n", sm95);
    printf("\nDigitizer bits decode (NID_*):\n");
    printf("  0x01 INTEGRATED_TOUCH  : %d\n", (sm94 & 0x01) != 0);
    printf("  0x02 EXTERNAL_TOUCH    : %d\n", (sm94 & 0x02) != 0);
    printf("  0x04 INTEGRATED_PEN    : %d\n", (sm94 & 0x04) != 0);
    printf("  0x08 EXTERNAL_PEN      : %d\n", (sm94 & 0x08) != 0);
    printf("  0x40 MULTI_INPUT       : %d  <-- multi-touch present\n", (sm94 & 0x40) != 0);
    printf("  0x80 READY             : %d  <-- input pipeline ready\n", (sm94 & 0x80) != 0);

    HWND fg = GetForegroundWindow();
    char title[256] = {0}; char cls[256] = {0};
    if (fg) {
        GetWindowTextA(fg, title, sizeof(title));
        GetClassNameA(fg, cls, sizeof(cls));
    }
    printf("\nForeground hwnd = 0x%p  class='%s'  title='%s'\n", (void*)fg, cls, title);
    DWORD pid = 0; GetWindowThreadProcessId(fg, &pid);
    printf("  pid=%lu\n", pid);

    typedef BOOL (WINAPI *PFN_IsTouchWindow)(HWND, PULONG);
    HMODULE u = GetModuleHandleA("user32.dll");
    auto p = (PFN_IsTouchWindow)GetProcAddress(u, "IsTouchWindow");

    // Walk the entire window from fg downward at the test point (960, 540) and
    // print which one would receive WM_TOUCH at that point. Use ChildWindowFromPoint
    // recursively + IsTouchWindow.
    if (p) {
        ULONG flags = 0;
        printf("  fg IsTouchWindow=%d flags=0x%08X\n", p(fg, &flags), flags);
    }

    POINT pt = { cx / 2, cy / 2 };  // screen center
    HWND atPt = WindowFromPoint(pt);
    char ptcls[256] = {0}; char pttitle[256] = {0};
    if (atPt) {
        GetClassNameA(atPt, ptcls, sizeof(ptcls));
        GetWindowTextA(atPt, pttitle, sizeof(pttitle));
    }
    printf("\nWindowFromPoint(%d,%d) = 0x%p class='%s' title='%s'\n",
           pt.x, pt.y, (void*)atPt, ptcls, pttitle);

    if (p && atPt) {
        // walk up to top-level
        HWND walk = atPt;
        for (int i = 0; i < 16 && walk; ++i) {
            char wcls[256] = {0}; char wtit[256] = {0};
            GetClassNameA(walk, wcls, sizeof(wcls));
            GetWindowTextA(walk, wtit, sizeof(wtit));
            ULONG flg = 0;
            BOOL is = p(walk, &flg);
            printf("  [%d] hwnd=0x%p class='%s' touch=%d flags=0x%X title='%.40s'\n",
                   i, (void*)walk, wcls, is, flg, wtit);
            HWND parent = GetParent(walk);
            if (!parent) break;
            walk = parent;
        }
    }

    // Enumerate all top-level windows and report which ones are touch-enabled.
    if (p) {
        printf("\nAll top-level windows with IsTouchWindow=1:\n");
        struct EnumCtx { PFN_IsTouchWindow fn; int n; } ctx = { p, 0 };
        EnumWindows([](HWND h, LPARAM lp) -> BOOL {
            EnumCtx* c = reinterpret_cast<EnumCtx*>(lp);
            ULONG f = 0;
            if (c->fn(h, &f)) {
                char cls[128] = {0}, t[128] = {0};
                GetClassNameA(h, cls, sizeof(cls));
                GetWindowTextA(h, t, sizeof(t));
                printf("  hwnd=0x%p class='%s' flags=0x%X title='%.40s'\n",
                       (void*)h, cls, f, t);
                c->n++;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&ctx));
        printf("  total touch-enabled top-level: %d\n", ctx.n);
    }
    return 0;
}
