/*
 * Hotkey Language Switcher
 * Remaps CapsLock -> Ctrl+Shift (Windows language switch) with near-zero latency.
 *
 * Build:
 *   gcc hotkey.c -o HotkeyLanguageSwitcher.exe -mwindows -O2
 *
 * Architecture:
 *   WH_KEYBOARD_LL hook returns in <1us (just posts a message).
 *   The message loop does the actual SendInput call asynchronously.
 *   This prevents Windows from timing out and removing the hook.
 *
 * Bug fixes:
 *   - Check GetAsyncKeyState before injecting modifiers (prevents sticky Tab/Shift)
 *   - Track key-down state to block WM_KEYDOWN auto-repeat (prevents rapid switching)
 *   - Block unknown injected CapsLock events (prevents startup/input-tool feedback loops)
 *   - Normalize CapsLock toggle state only with our own tagged synthetic event
 */
#include <windows.h>
#include "hotkey_core.h"

#define WM_DO_SWITCH (WM_USER + 1)

static HHOOK g_hook = NULL;
static DWORD g_tid = 0;
static HklsHookState g_hookState;

/* -----------------------------------------------------------------------
 * switchLanguage
 *
 * Injects Ctrl+Shift to trigger Windows language switch.
 *
 * IMPORTANT: checks GetAsyncKeyState first so we never inject a key-up
 * for a modifier the user is physically holding.  Without this check,
 * our synthetic Shift-up would desync Windows' modifier state and make
 * subsequent Tab / letter presses behave as if a modifier is stuck.
 * ----------------------------------------------------------------------- */
static void switchLanguage(void) {
    BOOL ctrlHeld  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    BOOL shiftHeld = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;

    INPUT in[4] = {0};
    int n = hklsBuildSwitchInputs(ctrlHeld, shiftHeld, in);

    if (n > 0) SendInput(n, in, sizeof(INPUT));
}

/* -----------------------------------------------------------------------
 * ensureCapsLockOff
 *
 * If Windows starts with CapsLock already toggled on, suppressing the
 * physical key would otherwise leave the user unable to turn it off.
 * This sends one tagged synthetic CapsLock press only when the toggle bit
 * is on; the hook allows only this tagged event through and blocks all
 * other injected CapsLock events.
 * ----------------------------------------------------------------------- */
static void ensureCapsLockOff(void) {
    if (!hklsShouldNormalizeCapsLock(GetKeyState(VK_CAPITAL))) return; /* already OFF */

    INPUT caps[2] = {0};
    int n = hklsBuildCapsNormalizeInputs(caps);

    SendInput(n, caps, sizeof(INPUT));
}

/* -----------------------------------------------------------------------
 * keyboardProc (WH_KEYBOARD_LL callback)
 *
 * Returns immediately after PostThreadMessage so Windows never times out
 * this hook (~300 ms limit).  The actual SendInput runs in the message loop.
 *
 * g_capsDown prevents WM_KEYDOWN auto-repeat from firing WM_DO_SWITCH
 * multiple times while the key is held.
 * ----------------------------------------------------------------------- */
static LRESULT CALLBACK keyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        const KBDLLHOOKSTRUCT *p = (const KBDLLHOOKSTRUCT *)lParam;

        switch (hklsHandleKeyboardEvent(&g_hookState, p->vkCode, wParam, p->flags, p->dwExtraInfo, GetTickCount())) {
            case HKLS_ACTION_SUPPRESS_AND_SWITCH:
                PostThreadMessage(g_tid, WM_DO_SWITCH, 0, 0);
                return 1;
            case HKLS_ACTION_SUPPRESS:
                return 1;
            case HKLS_ACTION_PASS:
            default:
                break;
        }
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

/* -----------------------------------------------------------------------
 * WinMain
 * ----------------------------------------------------------------------- */
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    (void)hInst; (void)hPrev; (void)lpCmd; (void)nShow;

    /* prevent multiple instances */
    HANDLE hMutex = CreateMutexA(NULL, TRUE, "HotkeyLangSwitcher_v2");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }

    /* above-normal priority reduces input latency noticeably */
    SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);

    MSG msg;

    g_tid = GetCurrentThreadId();
    hklsInitHookState(&g_hookState);
    PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE); /* ensure PostThreadMessage has a queue */

    g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboardProc, NULL, 0);
    if (!g_hook) {
        CloseHandle(hMutex);
        return 1;
    }

    ensureCapsLockOff();

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_DO_SWITCH) {
            switchLanguage();
            ensureCapsLockOff();
        } else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    UnhookWindowsHookEx(g_hook);
    CloseHandle(hMutex);
    return 0;
}
