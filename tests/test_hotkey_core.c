#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "../src/hotkey_core.h"

static int g_failures = 0;

#define EXPECT_TRUE(expr) do { \
    if (!(expr)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        g_failures++; \
    } \
} while (0)

#define EXPECT_EQ_INT(actual, expected) do { \
    int a_ = (int)(actual); \
    int e_ = (int)(expected); \
    if (a_ != e_) { \
        printf("FAIL %s:%d: %s == %d, expected %d\n", __FILE__, __LINE__, #actual, a_, e_); \
        g_failures++; \
    } \
} while (0)

#define EXPECT_EQ_DWORD(actual, expected) do { \
    DWORD a_ = (DWORD)(actual); \
    DWORD e_ = (DWORD)(expected); \
    if (a_ != e_) { \
        printf("FAIL %s:%d: %s == %lu, expected %lu\n", __FILE__, __LINE__, #actual, (unsigned long)a_, (unsigned long)e_); \
        g_failures++; \
    } \
} while (0)

#define EXPECT_EQ_PTR(actual, expected) do { \
    ULONG_PTR a_ = (ULONG_PTR)(actual); \
    ULONG_PTR e_ = (ULONG_PTR)(expected); \
    if (a_ != e_) { \
        printf("FAIL %s:%d: %s == 0x%lx, expected 0x%lx\n", __FILE__, __LINE__, #actual, (unsigned long)a_, (unsigned long)e_); \
        g_failures++; \
    } \
} while (0)

static HklsHookAction event(
    HklsHookState *state,
    DWORD vkCode,
    WPARAM message,
    DWORD flags,
    ULONG_PTR extraInfo,
    DWORD now
) {
    return hklsHandleKeyboardEvent(state, vkCode, message, flags, extraInfo, now);
}

static void test_non_caps_passes(void) {
    HklsHookState s;
    hklsInitHookState(&s);

    EXPECT_EQ_INT(event(&s, 'A', WM_KEYDOWN, 0, 0, 10), HKLS_ACTION_PASS);
    EXPECT_TRUE(!s.capsDown);
    EXPECT_EQ_DWORD(s.lastSwitchTick, 0);
}

static void test_physical_caps_switches_once_until_keyup(void) {
    HklsHookState s;
    hklsInitHookState(&s);

    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_KEYDOWN, 0, 0, 100), HKLS_ACTION_SUPPRESS_AND_SWITCH);
    EXPECT_TRUE(s.capsDown);
    EXPECT_EQ_DWORD(s.capsDownTick, 100);
    EXPECT_EQ_DWORD(s.lastSwitchTick, 100);

    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_KEYDOWN, 0, 0, 110), HKLS_ACTION_SUPPRESS);
    EXPECT_TRUE(s.capsDown);
    EXPECT_EQ_DWORD(s.lastSwitchTick, 100);

    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_KEYUP, 0, 0, 120), HKLS_ACTION_SUPPRESS);
    EXPECT_TRUE(!s.capsDown);

    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_KEYDOWN, 0, 0, 250), HKLS_ACTION_SUPPRESS_AND_SWITCH);
    EXPECT_EQ_DWORD(s.lastSwitchTick, 250);
}

static void test_debounce_blocks_rapid_repress(void) {
    HklsHookState s;
    hklsInitHookState(&s);

    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_KEYDOWN, 0, 0, 1000), HKLS_ACTION_SUPPRESS_AND_SWITCH);
    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_KEYUP, 0, 0, 1010), HKLS_ACTION_SUPPRESS);

    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_KEYDOWN, 0, 0, 1050), HKLS_ACTION_SUPPRESS);
    EXPECT_EQ_DWORD(s.lastSwitchTick, 1000);
    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_KEYUP, 0, 0, 1060), HKLS_ACTION_SUPPRESS);

    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_KEYDOWN, 0, 0, 1120), HKLS_ACTION_SUPPRESS_AND_SWITCH);
    EXPECT_EQ_DWORD(s.lastSwitchTick, 1120);
}

static void test_stuck_key_reset_after_timeout(void) {
    HklsHookState s;
    hklsInitHookState(&s);

    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_KEYDOWN, 0, 0, 500), HKLS_ACTION_SUPPRESS_AND_SWITCH);
    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_KEYDOWN, 0, 0, 1500), HKLS_ACTION_SUPPRESS);
    EXPECT_EQ_DWORD(s.lastSwitchTick, 500);

    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_KEYDOWN, 0, 0, 1501), HKLS_ACTION_SUPPRESS_AND_SWITCH);
    EXPECT_EQ_DWORD(s.capsDownTick, 1501);
    EXPECT_EQ_DWORD(s.lastSwitchTick, 1501);
}

static void test_injected_caps_from_other_program_is_blocked(void) {
    HklsHookState s;
    hklsInitHookState(&s);

    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_KEYDOWN, LLKHF_INJECTED, 0, 100), HKLS_ACTION_SUPPRESS);
    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_KEYUP, LLKHF_INJECTED, 0, 110), HKLS_ACTION_SUPPRESS);
    EXPECT_TRUE(!s.capsDown);
    EXPECT_EQ_DWORD(s.lastSwitchTick, 0);
}

static void test_own_normalize_caps_event_passes_through(void) {
    HklsHookState s;
    hklsInitHookState(&s);

    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_KEYDOWN, LLKHF_INJECTED, HKLS_CAPS_NORMALIZE_MARKER, 100), HKLS_ACTION_PASS);
    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_KEYUP, LLKHF_INJECTED, HKLS_CAPS_NORMALIZE_MARKER, 110), HKLS_ACTION_PASS);
    EXPECT_TRUE(!s.capsDown);
    EXPECT_EQ_DWORD(s.lastSwitchTick, 0);
}

static void test_syskey_paths_match_key_paths(void) {
    HklsHookState s;
    hklsInitHookState(&s);

    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_SYSKEYDOWN, 0, 0, 100), HKLS_ACTION_SUPPRESS_AND_SWITCH);
    EXPECT_TRUE(s.capsDown);
    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_SYSKEYUP, 0, 0, 130), HKLS_ACTION_SUPPRESS);
    EXPECT_TRUE(!s.capsDown);
}

static void test_unknown_caps_messages_are_suppressed(void) {
    HklsHookState s;
    hklsInitHookState(&s);

    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_CHAR, 0, 0, 100), HKLS_ACTION_SUPPRESS);
    EXPECT_TRUE(!s.capsDown);
    EXPECT_EQ_DWORD(s.lastSwitchTick, 0);
}

static void test_tick_wraparound_debounce(void) {
    HklsHookState s;
    hklsInitHookState(&s);
    s.lastSwitchTick = 0xFFFFFFF0u;

    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_KEYDOWN, 0, 0, 20), HKLS_ACTION_SUPPRESS);
    EXPECT_EQ_DWORD(s.lastSwitchTick, 0xFFFFFFF0u);
    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_KEYUP, 0, 0, 21), HKLS_ACTION_SUPPRESS);

    EXPECT_EQ_INT(event(&s, VK_CAPITAL, WM_KEYDOWN, 0, 0, 200), HKLS_ACTION_SUPPRESS_AND_SWITCH);
    EXPECT_EQ_DWORD(s.lastSwitchTick, 200);
}

static void expect_input(const INPUT *input, WORD vk, DWORD flags, ULONG_PTR extraInfo) {
    EXPECT_EQ_DWORD(input->type, INPUT_KEYBOARD);
    EXPECT_EQ_DWORD(input->ki.wVk, vk);
    EXPECT_EQ_DWORD(input->ki.dwFlags, flags);
    EXPECT_EQ_PTR(input->ki.dwExtraInfo, extraInfo);
}

static void test_switch_inputs_press_only_missing_modifiers(void) {
    INPUT in[4];
    int n;

    memset(in, 0x7f, sizeof(in));
    n = hklsBuildSwitchInputs(FALSE, FALSE, in);
    EXPECT_EQ_INT(n, 4);
    expect_input(&in[0], VK_CONTROL, 0, 0);
    expect_input(&in[1], VK_SHIFT, 0, 0);
    expect_input(&in[2], VK_SHIFT, KEYEVENTF_KEYUP, 0);
    expect_input(&in[3], VK_CONTROL, KEYEVENTF_KEYUP, 0);

    memset(in, 0x7f, sizeof(in));
    n = hklsBuildSwitchInputs(TRUE, FALSE, in);
    EXPECT_EQ_INT(n, 2);
    expect_input(&in[0], VK_SHIFT, 0, 0);
    expect_input(&in[1], VK_SHIFT, KEYEVENTF_KEYUP, 0);
    EXPECT_EQ_DWORD(in[2].type, 0);

    memset(in, 0x7f, sizeof(in));
    n = hklsBuildSwitchInputs(FALSE, TRUE, in);
    EXPECT_EQ_INT(n, 2);
    expect_input(&in[0], VK_CONTROL, 0, 0);
    expect_input(&in[1], VK_CONTROL, KEYEVENTF_KEYUP, 0);
    EXPECT_EQ_DWORD(in[2].type, 0);

    memset(in, 0x7f, sizeof(in));
    n = hklsBuildSwitchInputs(TRUE, TRUE, in);
    EXPECT_EQ_INT(n, 0);
    EXPECT_EQ_DWORD(in[0].type, 0);
}

static void test_caps_normalize_helpers(void) {
    INPUT caps[2];
    int n;

    EXPECT_TRUE(!hklsShouldNormalizeCapsLock(0));
    EXPECT_TRUE(!hklsShouldNormalizeCapsLock((SHORT)0x8000));
    EXPECT_TRUE(hklsShouldNormalizeCapsLock(1));
    EXPECT_TRUE(hklsShouldNormalizeCapsLock((SHORT)0x8001));

    memset(caps, 0x7f, sizeof(caps));
    n = hklsBuildCapsNormalizeInputs(caps);
    EXPECT_EQ_INT(n, 2);
    expect_input(&caps[0], VK_CAPITAL, 0, HKLS_CAPS_NORMALIZE_MARKER);
    expect_input(&caps[1], VK_CAPITAL, KEYEVENTF_KEYUP, HKLS_CAPS_NORMALIZE_MARKER);
}

int main(void) {
    test_non_caps_passes();
    test_physical_caps_switches_once_until_keyup();
    test_debounce_blocks_rapid_repress();
    test_stuck_key_reset_after_timeout();
    test_injected_caps_from_other_program_is_blocked();
    test_own_normalize_caps_event_passes_through();
    test_syskey_paths_match_key_paths();
    test_unknown_caps_messages_are_suppressed();
    test_tick_wraparound_debounce();
    test_switch_inputs_press_only_missing_modifiers();
    test_caps_normalize_helpers();

    if (g_failures) {
        printf("FAILED: %d assertion(s)\n", g_failures);
        return 1;
    }

    printf("PASS: hotkey core tests\n");
    return 0;
}
