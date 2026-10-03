# Hotkey Language Switcher

โปรแกรม Windows ขนาดเล็กสำหรับ remap ปุ่ม `CapsLock` ให้เป็นปุ่มสลับภาษา input ด้วย `Ctrl+Shift`

ออกแบบให้ทำงานเงียบ ๆ ตอนเปิดเครื่อง กดแล้วตอบสนองเร็ว และป้องกันปัญหา CapsLock เปิดเอง, modifier ค้าง, auto-repeat, และ startup loop ที่อาจเกิดจาก keyboard driver หรือ IME tool อื่น ๆ

## ใช้งานอย่างไร

| ปุ่ม | ผลลัพธ์ |
|------|---------|
| `CapsLock` | สลับภาษา input |
| ปุ่มอื่น | ทำงานปกติ |

หลังติดตั้งแล้วโปรแกรมจะทำงานอยู่เบื้องหลัง ไม่มีหน้าต่าง และเริ่มเองทันทีหลัง login ผ่าน Task Scheduler (ไม่ต้องรอคิว Startup folder)

หมายเหตุ: `CapsLock` จะไม่ใช้เปิด/ปิดตัวพิมพ์ใหญ่อีกต่อไป โปรแกรมจะ suppress CapsLock จริงทั้งหมด

## ติดตั้งหรืออัปเกรด

วิธีแนะนำ:

```powershell
cd D:\Agents\Claude\hotkey-language-switcher
.\installer\install.bat
```

`install.bat` จะขอสิทธิ์ Administrator (UAC) หนึ่งครั้ง แล้วรัน `installer\setup.ps1` ซึ่ง:

- ปิด `HotkeyLanguageSwitcher.exe` ตัวเก่าถ้ามี
- copy binary ไปที่ `C:\Program Files\HotkeyLanguageSwitcher\` (admin-only writable กัน binary ถูกสลับ)
- ลบ startup แบบเก่า (Registry Run key และ Startup folder) เพื่อไม่ให้รันซ้ำ
- สร้าง scheduled task `HotkeyLanguageSwitcher`:
  - trigger: At log on ของ user ปัจจุบัน ไม่มี delay
  - Run with highest privileges เพื่อให้ใช้ได้ในแอปที่รันแบบ Administrator
  - ไม่มี time limit (ค่า default ของ Windows จะ kill หลัง 72 ชั่วโมง)
  - ทำงานตอนใช้แบตเตอรี่
  - restart เองทุก 1 นาที (สูงสุด 10 ครั้ง) ถ้า process ล้ม
- start task ทันทีหลังติดตั้ง

รันซ้ำเพื่ออัปเกรดได้เลย

หลังติดตั้ง ให้ตรวจสถานะจริง:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\diagnose.ps1
```

ถ้าขึ้นแบบนี้ถือว่าพร้อมใช้:

```text
[PASS] runtime diagnostics
```

### หลังติดตั้งต้องทำอะไรต่อ

ไม่ต้องทำอะไร และไม่ต้อง restart:

- `install.bat` start task ให้ทันที กด CapsLock ใช้ได้เลย
- login ครั้งต่อไป Task Scheduler จะเปิดโปรแกรมให้เองภายในไม่กี่วินาที

### อัปเกรดหลังแก้โค้ด

แก้ `src/` แล้วต้อง build และติดตั้งใหม่ เพราะ task รัน binary ใน `C:\Program Files\HotkeyLanguageSwitcher\` ไม่ใช่ใน repo:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\test.ps1   # build + test + sync ไป installer\
.\installer\install.bat                                        # copy ไป Program Files + restart task
powershell -ExecutionPolicy Bypass -File .\scripts\diagnose.ps1
```

ถ้าลืมขั้นที่สอง `diagnose.ps1` จะแจ้งว่า installed binary hash ไม่ตรง

## ทำไมใช้ Task Scheduler

เวอร์ชันก่อนใช้ Startup folder ซึ่ง Windows จงใจหน่วง และเปิดหลังโปรแกรมใน Registry Run key ทั้งหมด บนเครื่องที่มีโปรแกรม startup เยอะ (Docker, Steam, Discord, OneDrive ฯลฯ) วัดได้ว่าโปรแกรมเริ่มช้ากว่า Explorer ถึง 3 นาที 42 วินาที

```text
เดิม:  Login → Explorer → [หน่วง] → Run key ทุกตัวแย่งกันโหลด → Startup folder → switcher   (10 วินาที – หลายนาที)
ใหม่:  Login → Task Scheduler → switcher   (ทำงานคู่กับ Explorer, ~1-3 วินาที)
```

| | Startup folder (เดิม) | Task Scheduler (ปัจจุบัน) |
|---|---|---|
| ใช้ได้หลัง login | 10 วินาที – หลายนาที | ~1-3 วินาที |
| ต่อคิวหลังโปรแกรมอื่น | ใช่ | ไม่ |
| ใช้ในแอปที่รันแบบ Administrator | ไม่ได้ | ได้ (highest privileges) |
| process ล้มแล้ว restart เอง | ไม่ | ได้ |
| ใช้ที่หน้า login / lock screen | ไม่ได้ | ไม่ได้ |
| ติดตั้ง | copy ไฟล์ | ต้องกด UAC หนึ่งครั้ง |

### ทางเลือกอื่นที่พิจารณาแล้ว

ถ้าต้องการให้ใช้ได้ตั้งแต่ก่อน login (BIOS, หน้า login, lock screen) ต้องทำที่ระดับ driver หรือ hardware ไม่ใช่โปรแกรม user-space:

| ทางเลือก | ใช้ได้ตั้งแต่ boot | ข้อจำกัด |
|---|---|---|
| Registry `Scancode Map` + ตั้ง hotkey สลับภาษาเป็น Grave (`` ` ``) | ได้ | Scancode Map map ได้แค่ 1 ปุ่ม → 1 ปุ่ม ทำ Ctrl+Shift ไม่ได้ จึงต้องใช้ Grave แทน และจะพิมพ์ `` ` `` / `~` ไม่ได้ |
| Remap ใน firmware คีย์บอร์ด (QMK/VIA) | ได้ | Keychron K2 รุ่นปกติไม่รองรับ ต้องเป็น K2 Pro / Max / HE |
| ตัวแปลง USB (RP2040 + [hid-remapper](https://github.com/jfedor2/hid-remapper)) | ได้ | ต้องต่อคีย์บอร์ดแบบสาย |
| ตัวแปลง Bluetooth (Pico W + firmware เขียนเอง) | ได้ | K2 ใช้ Bluetooth Classic ต้องเขียน firmware เอง (BTstack + TinyUSB) |
| Kernel filter driver เขียนเอง | ได้ | Windows 11 + Secure Boot ต้องให้ Microsoft sign driver; test-signing ลดความปลอดภัยและ anti-cheat ไม่ยอม; bug = BSOD |

สำหรับการใช้งานหลัง login Task Scheduler เป็นจุดที่คุ้มที่สุด: ไม่ต้องซื้อของ, ยังใช้ Ctrl+Shift และพิมพ์ `` ` `` ได้ปกติ, ใช้ได้ทั้งสายและ Bluetooth

## ถอนการติดตั้ง

```powershell
cd D:\Agents\Claude\hotkey-language-switcher
.\installer\reset.bat
```

`reset.bat` จะขอสิทธิ์ Administrator แล้วปิด process, ลบ scheduled task, ลบ `C:\Program Files\HotkeyLanguageSwitcher\` และลบ startup แบบเก่าที่อาจค้างอยู่

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
- ย้ายจาก Startup folder ไป Task Scheduler: Startup folder ถูก Windows หน่วงและต้องต่อคิวหลังโปรแกรมใน Run key ทำให้บางครั้งใช้ได้ช้ากว่า login หลายนาที
- instance ที่สองที่เปิดแบบไม่ elevate จะออกทันที แทนที่จะรันซ้อนแล้วสลับภาษาสองรอบ

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
- ตรวจว่า setup สร้าง task แบบ at-logon, highest privileges, ไม่มี time limit, ไม่หยุดตอนใช้แบต และ cleanup startup แบบเก่า
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
    ├── install.bat           # ติดตั้ง/อัปเกรด (เรียก setup.ps1)
    ├── reset.bat             # ถอนการติดตั้ง (setup.ps1 -Uninstall)
    └── setup.ps1             # สร้าง scheduled task + cleanup startup เก่า
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
- ใช้ได้กับแอปที่รันแบบ Administrator เพราะ scheduled task รันด้วย highest privileges
- ยังใช้ไม่ได้ที่หน้า login / lock screen เพราะเป็น user-space hook
- Global keyboard hooks ไม่ควรทดสอบด้วยการยิง key จริงบนเครื่องใช้งานหลัก จึงมี unit test core logic และ runtime diagnostics แยกกัน

## Troubleshooting

ถ้ากดแล้วไม่สลับภาษา:

- ตรวจว่ามี input language มากกว่า 1 ภาษาใน Windows Settings
- รัน `scripts\diagnose.ps1`
- เปิด Task Manager แล้วเช็คว่ามี `HotkeyLanguageSwitcher.exe` แค่ตัวเดียว
- เปิด Task Scheduler (`taskschd.msc`) แล้วดูว่า task `HotkeyLanguageSwitcher` อยู่ในสถานะ Running

ถ้า diagnostics แจ้งว่า binary hash ไม่ตรงหรือ task หาย:

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
