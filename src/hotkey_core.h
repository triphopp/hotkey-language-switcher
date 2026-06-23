#ifndef HKLS_HOTKEY_CORE_H
#define HKLS_HOTKEY_CORE_H

#include <windows.h>

#define HKLS_SWITCH_DEBOUNCE_MS 120u
#define HKLS_STUCK_KEY_RESET_MS 1000u
#define HKLS_CAPS_NORMALIZE_MARKER ((ULONG_PTR)0x484B4C53u) /* "HKLS" */

typedef enum HklsHookAction {
    HKLS_ACTION_PASS = 0,
    HKLS_ACTION_SUPPRESS = 1,
    HKLS_ACTION_SUPPRESS_AND_SWITCH = 2
} HklsHookAction;

typedef struct HklsHookState {
    BOOL capsDown;
    DWORD capsDownTick;
    DWORD lastSwitchTick;
} HklsHookState;

static void hklsInitHookState(HklsHookState *state) {
    state->capsDown = FALSE;
    state->capsDownTick = 0;
    state->lastSwitchTick = 0;
}

static HklsHookAction hklsHandleKeyboardEvent(
    HklsHookState *state,
    DWORD vkCode,
    WPARAM message,
    DWORD flags,
    ULONG_PTR extraInfo,
    DWORD now
) {
    if (vkCode != VK_CAPITAL) {
        return HKLS_ACTION_PASS;
    }

    {
        BOOL injected = (flags & LLKHF_INJECTED) != 0;
        BOOL ownNormalizeEvent = injected && extraInfo == HKLS_CAPS_NORMALIZE_MARKER;

        if (ownNormalizeEvent) {
            return HKLS_ACTION_PASS;
        }

        if (injected) {
            return HKLS_ACTION_SUPPRESS;
        }
    }

    if (message == WM_KEYUP || message == WM_SYSKEYUP) {
        state->capsDown = FALSE;
        return HKLS_ACTION_SUPPRESS;
    }

    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) {
        if (state->capsDown && now - state->capsDownTick > HKLS_STUCK_KEY_RESET_MS) {
            state->capsDown = FALSE;
        }

        if (!state->capsDown) {
            state->capsDown = TRUE;
            state->capsDownTick = now;

            if (state->lastSwitchTick == 0 || now - state->lastSwitchTick >= HKLS_SWITCH_DEBOUNCE_MS) {
                state->lastSwitchTick = now;
                return HKLS_ACTION_SUPPRESS_AND_SWITCH;
            }
        }
    }

    return HKLS_ACTION_SUPPRESS;
}

static int hklsBuildSwitchInputs(BOOL ctrlHeld, BOOL shiftHeld, INPUT in[4]) {
    int n = 0;
    ZeroMemory(in, sizeof(INPUT) * 4);

    if (!ctrlHeld) {
        in[n].type = INPUT_KEYBOARD;
        in[n].ki.wVk = VK_CONTROL;
        n++;
    }

    if (!shiftHeld) {
        in[n].type = INPUT_KEYBOARD;
        in[n].ki.wVk = VK_SHIFT;
        n++;
    }

    if (!shiftHeld) {
        in[n].type = INPUT_KEYBOARD;
        in[n].ki.wVk = VK_SHIFT;
        in[n].ki.dwFlags = KEYEVENTF_KEYUP;
        n++;
    }

    if (!ctrlHeld) {
        in[n].type = INPUT_KEYBOARD;
        in[n].ki.wVk = VK_CONTROL;
        in[n].ki.dwFlags = KEYEVENTF_KEYUP;
        n++;
    }

    return n;
}

static BOOL hklsShouldNormalizeCapsLock(SHORT capsState) {
    return (capsState & 0x0001) != 0;
}

static int hklsBuildCapsNormalizeInputs(INPUT caps[2]) {
    ZeroMemory(caps, sizeof(INPUT) * 2);

    caps[0].type = INPUT_KEYBOARD;
    caps[0].ki.wVk = VK_CAPITAL;
    caps[0].ki.dwExtraInfo = HKLS_CAPS_NORMALIZE_MARKER;

    caps[1].type = INPUT_KEYBOARD;
    caps[1].ki.wVk = VK_CAPITAL;
    caps[1].ki.dwFlags = KEYEVENTF_KEYUP;
    caps[1].ki.dwExtraInfo = HKLS_CAPS_NORMALIZE_MARKER;

    return 2;
}

#endif
