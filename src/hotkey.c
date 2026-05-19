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
 *   - Normalize CapsLock LED state after suppression (fixes LED stuck on some hardware)
 */
#include <windows.h>

#define WM_DO_SWITCH (WM_USER + 1)

static HHOOK g_hook    = NULL;
static DWORD g_tid     = 0;
static BOOL  g_capsDown = FALSE; /* tracks physical CapsLock state to block auto-repeat */

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
    int   n     = 0;

    /* press only what isn't already held */
    if (!ctrlHeld)  { in[n].type = INPUT_KEYBOARD; in[n].ki.wVk = VK_CONTROL; n++; }
    if (!shiftHeld) { in[n].type = INPUT_KEYBOARD; in[n].ki.wVk = VK_SHIFT;   n++; }

    /* release only what we ourselves pressed */
    if (!shiftHeld) { in[n].type = INPUT_KEYBOARD; in[n].ki.wVk = VK_SHIFT;   in[n].ki.dwFlags = KEYEVENTF_KEYUP; n++; }
    if (!ctrlHeld)  { in[n].type = INPUT_KEYBOARD; in[n].ki.wVk = VK_CONTROL; in[n].ki.dwFlags = KEYEVENTF_KEYUP; n++; }

    if (n > 0) SendInput(n, in, sizeof(INPUT));
}

/* -----------------------------------------------------------------------
 * fixCapsLockLed
 *
 * On some hardware/drivers, CapsLock LED still toggles even when the hook
 * returns 1 (suppressed).  Inject a synthetic CapsLock press (flagged as
 * LLKHF_INJECTED so the hook ignores it) to flip the LED back to OFF.
 * ----------------------------------------------------------------------- */
static void fixCapsLockLed(void) {
    if (!(GetKeyState(VK_CAPITAL) & 0x0001)) return; /* LED already OFF, nothing to do */

    INPUT caps[2] = {0};
    caps[0].type = INPUT_KEYBOARD; caps[0].ki.wVk = VK_CAPITAL;
    caps[1].type = INPUT_KEYBOARD; caps[1].ki.wVk = VK_CAPITAL; caps[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, caps, sizeof(INPUT));
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

        if (p->vkCode == VK_CAPITAL && !(p->flags & LLKHF_INJECTED)) {
            if (wParam == WM_KEYDOWN && !g_capsDown) {
                g_capsDown = TRUE;
                PostThreadMessage(g_tid, WM_DO_SWITCH, 0, 0);
            } else if (wParam == WM_KEYUP) {
                g_capsDown = FALSE;
            }
            return 1; /* suppress CapsLock entirely — never reaches any app */
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

    g_tid  = GetCurrentThreadId();
    g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboardProc, NULL, 0);
    if (!g_hook) {
        CloseHandle(hMutex);
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_DO_SWITCH) {
            switchLanguage();
            fixCapsLockLed();
        } else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    UnhookWindowsHookEx(g_hook);
    CloseHandle(hMutex);
    return 0;
}
