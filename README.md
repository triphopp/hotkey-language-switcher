# Hotkey Language Switcher

โปรแกรม Windows ขนาดเล็กสำหรับ remap ปุ่ม `CapsLock` ให้เป็นปุ่มสลับภาษา input ด้วย `Ctrl+Shift`

ออกแบบให้ทำงานเงียบ ๆ ตอนเปิดเครื่อง กดแล้วตอบสนองเร็ว และป้องกันปัญหา CapsLock เปิดเอง, modifier ค้าง, auto-repeat, และ startup loop ที่อาจเกิดจาก keyboard driver หรือ IME tool อื่น ๆ

## ใช้งานอย่างไร

| ปุ่ม | ผลลัพธ์ |
|------|---------|
| `CapsLock` | สลับภาษา input |
| ปุ่มอื่น | ทำงานปกติ |

หลังติดตั้งแล้วโปรแกรมจะทำงานอยู่เบื้องหลัง ไม่มีหน้าต่าง และจะเริ่มเองทุกครั้งหลัง login

หมายเหตุ: `CapsLock` จะไม่ใช้เปิด/ปิดตัวพิมพ์ใหญ่อีกต่อไป โปรแกรมจะ suppress CapsLock จริงทั้งหมด

## ติดตั้งหรืออัปเกรด

วิธีแนะนำ:

```powershell
cd D:\Agents\Claude\hotkey-language-switcher
.\installer\install.bat
```

สิ่งที่ `install.bat` ทำ:

- ปิด `HotkeyLanguageSwitcher.exe` ตัวเก่าถ้ามี
- อัปเกรด binary ในตำแหน่งติดตั้งเดิม
- ถ้ายังไม่เคยติดตั้ง จะ copy ไปที่ Startup folder ของ user ปัจจุบัน
- ลบ startup entry ซ้ำอีกทาง เพื่อไม่ให้มีทั้ง Registry Run และ Startup folder พร้อมกัน
- เปิดโปรแกรมทันทีหลังติดตั้ง

หลังติดตั้ง ให้ตรวจสถานะจริง:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\diagnose.ps1
```

ถ้าขึ้นแบบนี้ถือว่าพร้อมใช้:

```text
[PASS] runtime diagnostics
```

## ถอนการติดตั้ง

```powershell
cd D:\Agents\Claude\hotkey-language-switcher
.\installer\reset.bat
```

`reset.bat` จะปิด process, ลบ Registry Run key และลบ Startup folder copy ให้ครบ

## สาเหตุปัญหาที่เคยพบ

อาการเดิมคือเปิด Windows แล้วบางครั้งใช้งานได้ แต่บางครั้งเหมือน `CapsLock` ทำงานเอง ทำให้สลับภาษาไม่ได้หรือเกิด loop

สาเหตุที่พบจากโค้ดเดิม:

- hook เดิมปล่อย `CapsLock` ที่มี flag `LLKHF_INJECTED` ผ่านระบบทั้งหมด
- event แบบ injected อาจมาจาก Windows, IME, keyboard vendor software หรือ logic normalize CapsLock state
- เมื่อ event เหล่านี้ทะลุไปถึง Windows จึงมีโอกาสเปลี่ยน CapsLock toggle state จริง
- installer/reset เดิมมีโอกาสเหลือ startup entry คนละที่ ทำให้หลัง login รัน binary เก่าหรือคนละสำเนาได้

สิ่งที่แก้แล้ว:

- block third-party injected CapsLock ทั้งหมด
- อนุญาตเฉพาะ synthetic CapsLock ที่โปรแกรม tag เองด้วย marker `HKLS`
- เพิ่ม debounce เพื่อกัน key bounce และ rapid loop
- เพิ่ม stuck-key timeout เพื่อ recover ถ้า Windows ไม่ส่ง keyup
- normalize CapsLock ให้ OFF ตอนเริ่มโปรแกรมและหลังสลับภาษา
- installer/reset cleanup duplicate startup paths

## ระบบทดสอบ

รัน production test gate:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\test.ps1
```

test gate ตรวจสิ่งเหล่านี้:

- build โปรแกรมด้วย `-Wall -Wextra -Werror`
- build และรัน unit tests ของ hotkey core
- ตรวจว่า CapsLock จริง trigger switch แค่หนึ่งครั้งต่อ physical press
- ตรวจว่า CapsLock auto-repeat ตอนกดค้างไม่สลับภาษารัว
- ตรวจ debounce สำหรับการกดซ้ำเร็วผิดปกติ
- ตรวจ recovery เมื่อ keyup หายหรือ key state ค้าง
- ตรวจว่า third-party injected CapsLock ถูก block
- ตรวจว่า own tagged normalize event เท่านั้นที่ pass-through
- ตรวจว่า Ctrl/Shift injection ไม่ทำ modifier ค้าง
- ตรวจ DWORD tick wrap-around
- sync binary จาก `build/` ไป `installer/`
- ตรวจ SHA256 ของ binary ทั้งสองตำแหน่งว่าตรงกัน
- ตรวจว่า installer/reset มี logic cleanup startup duplicate
- ตรวจ whitespace ด้วย `git diff --check`

รายละเอียด coverage และ release checklist อยู่ใน [TESTING.md](TESTING.md)

## โครงสร้างโปรเจกต์

```text
.
├── src/
│   ├── hotkey.c              # Windows hook wrapper และ runtime entry point
│   └── hotkey_core.h         # deterministic core logic ที่ unit test ได้
├── tests/
│   └── test_hotkey_core.c    # unit tests สำหรับ CapsLock state machine
├── scripts/
│   ├── test.ps1              # production test gate
│   └── diagnose.ps1          # runtime diagnostics หลัง install/login
├── build/
│   └── HotkeyLanguageSwitcher.exe
└── installer/
    ├── HotkeyLanguageSwitcher.exe
    ├── install.bat
    ├── reset.bat
    ├── config.bat
    └── startup.bat
```

## Build จาก source

ต้องมี MinGW-w64 หรือ MSYS2 ที่เรียก `gcc` ได้จาก PATH

```powershell
gcc src\hotkey.c -o build\HotkeyLanguageSwitcher.exe -mwindows -O2 -Wall -Wextra -Werror
```

สำหรับ release ให้ใช้:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\test.ps1
```

เพราะคำสั่งนี้จะ build, test, sync installer binary และตรวจ hash ให้ครบ

## รายละเอียดทางเทคนิค

โปรแกรมใช้ `WH_KEYBOARD_LL` เพื่อ intercept CapsLock ก่อนถึงแอปอื่น แล้วส่งงานจริงไปที่ message loop ด้วย `PostThreadMessage` เพื่อให้ hook callback คืนค่าทันที

flow หลัก:

```text
Physical CapsLock
    -> WH_KEYBOARD_LL
    -> hotkey core decides pass/suppress/switch
    -> PostThreadMessage(WM_DO_SWITCH)
    -> SendInput(Ctrl down, Shift down, Shift up, Ctrl up)
    -> Windows switches input language
```

กฎของ CapsLock event:

| Event | ผลลัพธ์ |
|-------|---------|
| Physical CapsLock down | suppress + request language switch |
| Physical CapsLock repeat | suppress only |
| Physical CapsLock up | suppress + reset state |
| Third-party injected CapsLock | suppress only |
| Own tagged injected CapsLock | pass-through เพื่อ normalize OFF |
| Non-CapsLock key | pass-through |

## สิทธิ์และข้อจำกัด

- โปรแกรมไม่บันทึก keystrokes
- ใช้ Windows API มาตรฐานเท่านั้น
- ทำงานใน user space ไม่ใช่ kernel driver
- ถ้าต้องใช้กับแอปที่รันแบบ Administrator โปรแกรมนี้ต้องรันแบบ Administrator ด้วย
- Global keyboard hooks ไม่ควรทดสอบด้วยการยิง key จริงบนเครื่องใช้งานหลัก จึงมี unit test core logic และ runtime diagnostics แยกกัน

## Troubleshooting

ถ้ากดแล้วไม่สลับภาษา:

- ตรวจว่ามี input language มากกว่า 1 ภาษาใน Windows Settings
- รัน `scripts\diagnose.ps1`
- เปิด Task Manager แล้วเช็คว่ามี `HotkeyLanguageSwitcher.exe` แค่ตัวเดียว

ถ้า diagnostics แจ้งว่า startup binary hash ไม่ตรง:

```powershell
cd D:\Agents\Claude\hotkey-language-switcher
.\installer\install.bat
powershell -ExecutionPolicy Bypass -File .\scripts\diagnose.ps1
```

ถ้าต้องการ reset สะอาด:

```powershell
.\installer\reset.bat
.\installer\install.bat
```

## License

MIT
