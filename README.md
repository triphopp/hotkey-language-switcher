# Hotkey Language Switcher

โปรแกรม remap ปุ่ม **CapsLock** ให้สลับภาษา (Ctrl+Shift) บน Windows  
เบา, เร็ว, ไม่มี delay — ทำงานอัตโนมัติทุกครั้งหลังเปิดเครื่อง

---

## สิ่งที่โปรแกรมทำ

| ปุ่มที่กด | ผลลัพธ์ |
|-----------|---------|
| `CapsLock` | สลับภาษา input (Ctrl+Shift) |
| ปุ่มอื่นทุกปุ่ม | ทำงานปกติ ไม่กระทบ |

> CapsLock ถูก suppress ทั้งหมด — ไม่มี CAPS ON/OFF อีกต่อไป

---

## สถาปัตยกรรม (Architecture)

```
CapsLock pressed
      │
      ▼
WH_KEYBOARD_LL hook  ← intercepts at kernel level, works in ALL apps
      │  returns in <1µs (PostThreadMessage only)
      ▼
Message Loop (main thread)
      │
      ▼
SendInput(Ctrl↓, Shift↓, Shift↑, Ctrl↑)  ← atomic, modern API
      │
      ▼
Windows Language Switch
```

**ทำไมถึงเร็วและเสถียร:**
- Hook callback ทำแค่ `PostThreadMessage` แล้วคืนค่าทันที — ไม่มี blocking
- Windows จะ unhook อัตโนมัติถ้า hook ช้าเกิน ~300ms → การออกแบบนี้ป้องกันปัญหาดังกล่าว
- `SendInput` เป็น API สมัยใหม่ที่ส่ง keystrokes แบบ atomic (4 keys ในคำสั่งเดียว)
- Process priority = `ABOVE_NORMAL` ลด input latency อย่างเห็นได้ชัด

---

## ความต้องการของระบบ

- Windows 10 / 11 (64-bit หรือ 32-bit)
- ภาษา input ที่ต้องการต้องเพิ่มไว้ใน Windows Settings ก่อน
  - Settings → Time & Language → Language & Region → Add a language

---

## วิธีติดตั้ง (ผู้ใช้ทั่วไป)

### วิธีที่ 1 — ติดตั้งอัตโนมัติ (แนะนำ)

1. เปิดโฟลเดอร์ `installer/`
2. รัน **`startup.bat`** (คลิกขวา → Run as administrator ถ้าจำเป็น)
3. โปรแกรมจะถูก copy ไปยัง Startup folder และเริ่มทำงานทันที

```
installer/
├── HotkeyLanguageSwitcher.exe   ← ตัวโปรแกรม
├── startup.bat                  ← ติดตั้งเข้า Startup folder
├── config.bat                   ← ทางเลือก: ใช้ Registry Run key แทน
└── reset.bat                    ← ถอนการติดตั้ง
```

### วิธีที่ 2 — ใช้ Registry Run Key

1. วาง `HotkeyLanguageSwitcher.exe` ไว้ที่ path ที่ต้องการ (เช่น `C:\Tools\`)
2. รัน `installer/config.bat`
3. โปรแกรมจะเพิ่มตัวเองใน Registry เพื่อ auto-start

### วิธีที่ 3 — ติดตั้งด้วยตนเอง

เปิด PowerShell แล้วรัน:

```powershell
$exePath = "C:\Path\To\HotkeyLanguageSwitcher.exe"
$regPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
Set-ItemProperty -Path $regPath -Name "HotkeyLanguageSwitcher" -Value "`"$exePath`""
```

---

## วิธีถอนการติดตั้ง

```
รัน installer/reset.bat
```

หรือด้วยตนเอง:

```powershell
Remove-ItemProperty -Path "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run" -Name "HotkeyLanguageSwitcher"
```

แล้วลบไฟล์ `.exe` ออก

---

## วิธี Build จาก Source Code

ต้องการ: [MinGW-w64](https://www.mingw-w64.org/) หรือ MSYS2

```bash
gcc src/hotkey.c -o build/HotkeyLanguageSwitcher.exe -mwindows -O2
```

| Flag | ความหมาย |
|------|----------|
| `-mwindows` | ไม่เปิด console window, ใช้ `WinMain` entry point |
| `-O2` | compiler optimization ระดับ 2 (เร็วขึ้น) |

---

## การแก้ปัญหา (Troubleshooting)

### โปรแกรมไม่ทำงานกับบางแอป
โปรแกรมที่รันด้วยสิทธิ์ Administrator (elevated) เช่น Task Manager, บางเกม  
→ รัน `HotkeyLanguageSwitcher.exe` ด้วย **Run as administrator** ด้วยเช่นกัน

วิธีทำให้ auto-run as administrator:
1. คลิกขวาที่ `.exe` → Properties → Compatibility
2. เช็ค "Run this program as an administrator"

### ภาษาไม่สลับ
- ตรวจสอบว่ามีภาษามากกว่า 1 ภาษาใน Windows Settings
- Settings → Time & Language → Language & Region

### โปรแกรมรันอยู่แล้วแต่ไม่ตอบสนอง
โปรแกรมป้องกัน instance ซ้ำด้วย Mutex (`HotkeyLangSwitcher_v2`)  
เปิด Task Manager → ค้นหา `HotkeyLanguageSwitcher` → End task แล้วรันใหม่

### ถ้าใช้ startup.bat แล้วหน้าต่าง flash ขึ้นมา
ปกติสำหรับครั้งแรก เพราะ `.bat` รันค้างไว้ 3 วินาที  
ครั้งถัดไปที่เปิดเครื่อง โปรแกรมจะรันตรงๆ โดยไม่มี window

---

## รายละเอียดทางเทคนิค

### วิธีที่ hook ทำงาน

`WH_KEYBOARD_LL` เป็น global keyboard hook ที่ทำงานที่ระดับ kernel ก่อนที่ input จะถึงแอปใดๆ

```c
static LRESULT CALLBACK keyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        const KBDLLHOOKSTRUCT *p = (const KBDLLHOOKSTRUCT *)lParam;

        if (p->vkCode == VK_CAPITAL && !(p->flags & LLKHF_INJECTED)) {
            if (wParam == WM_KEYDOWN) {
                PostThreadMessage(g_tid, WM_DO_SWITCH, 0, 0);
            }
            return 1; /* suppress CapsLock */
        }
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}
```

- `LLKHF_INJECTED` check → ไม่ react กับ synthetic keystrokes (ป้องกัน loop)
- `return 1` → block CapsLock ไม่ให้ไปถึงแอปอื่น
- `PostThreadMessage` → async dispatch (hook ช้าอย่างมากได้)

### วิธีที่ SendInput ทำงาน

```c
static void switchLanguage(void) {
    INPUT in[4] = {0};
    in[0].ki.wVk = VK_CONTROL;           // Ctrl down
    in[1].ki.wVk = VK_SHIFT;             // Shift down
    in[2].ki = (KEYBDINPUT){ VK_SHIFT,  0, KEYEVENTF_KEYUP, 0, 0 }; // Shift up
    in[3].ki = (KEYBDINPUT){ VK_CONTROL, 0, KEYEVENTF_KEYUP, 0, 0 }; // Ctrl up
    SendInput(4, in, sizeof(INPUT));      // atomic: all 4 sent as one block
}
```

`SendInput` ส่ง key events ทั้งหมดในคำสั่งเดียว — ไม่มีทางที่ keystroke อื่นจะแทรกกลางได้

### Registry Hives ที่เกี่ยวข้อง

| Key | ชื่อเต็ม | ใช้ทำอะไร |
|-----|----------|-----------|
| `HKCU` | HKEY_CURRENT_USER | Auto-start เฉพาะ user ปัจจุบัน |
| `HKLM` | HKEY_LOCAL_MACHINE | Auto-start สำหรับทุก user (ต้องการ Admin) |

โปรแกรมนี้ใช้ `HKCU` — ไม่ต้องการ admin สำหรับการติดตั้ง

---

## การเพิ่มภาษา / เปลี่ยน Hotkey

ปัจจุบัน CapsLock ถูก hardcode ใน source code:

```c
#define VK_CAPITAL  0x14   // CapsLock — ค่านี้กำหนดไว้ใน <windows.h>
```

ถ้าต้องการเปลี่ยน hotkey ให้แก้บรรทัด:

```c
if (p->vkCode == VK_CAPITAL && ...)
```

เปลี่ยน `VK_CAPITAL` เป็น Virtual Key Code อื่น เช่น:

| ปุ่ม | Virtual Key Code |
|------|-----------------|
| CapsLock | `VK_CAPITAL` (0x14) |
| Scroll Lock | `VK_SCROLL` (0x91) |
| Pause/Break | `VK_PAUSE` (0x13) |
| F13-F24 | `VK_F13`–`VK_F24` |

แล้ว build ใหม่:
```bash
gcc src/hotkey.c -o build/HotkeyLanguageSwitcher.exe -mwindows -O2
```

---

## สิทธิ์และความปลอดภัย

- โปรแกรมนี้ **ไม่บันทึก** keystrokes ใดๆ
- ทำงานใน user space เท่านั้น (ไม่ใช่ kernel driver)
- Source code เปิดเผย — ตรวจสอบได้ใน `src/hotkey.c`
- ใช้เฉพาะ Windows API มาตรฐาน: `SetWindowsHookEx`, `SendInput`, `PostThreadMessage`

---

## License

MIT
