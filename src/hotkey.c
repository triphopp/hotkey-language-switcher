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
 */
#include <windows.h>

#define WM_DO_SWITCH (WM_USER + 1)

static HHOOK g_hook = NULL;
static DWORD g_tid  = 0;

static void switchLanguage(void) {
    INPUT in[4] = {0};

    in[0].type   = INPUT_KEYBOARD;
    in[0].ki.wVk = VK_CONTROL;

    in[1].type   = INPUT_KEYBOARD;
    in[1].ki.wVk = VK_SHIFT;

    in[2].type         = INPUT_KEYBOARD;
    in[2].ki.wVk       = VK_SHIFT;
    in[2].ki.dwFlags   = KEYEVENTF_KEYUP;

    in[3].type         = INPUT_KEYBOARD;
    in[3].ki.wVk       = VK_CONTROL;
    in[3].ki.dwFlags   = KEYEVENTF_KEYUP;

    SendInput(4, in, sizeof(INPUT));
}

static LRESULT CALLBACK keyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        const KBDLLHOOKSTRUCT *p = (const KBDLLHOOKSTRUCT *)lParam;

        if (p->vkCode == VK_CAPITAL && !(p->flags & LLKHF_INJECTED)) {
            if (wParam == WM_KEYDOWN) {
                PostThreadMessage(g_tid, WM_DO_SWITCH, 0, 0);
            }
            return 1; /* suppress CapsLock entirely */
        }
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

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
        } else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    UnhookWindowsHookEx(g_hook);
    CloseHandle(hMutex);
    return 0;
}
