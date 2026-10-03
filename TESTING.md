# Production Test Strategy

โปรแกรมนี้เป็น low-level keyboard hook จึงไม่ควรทดสอบทุกอย่างด้วยการส่ง key จริงเข้า Windows session ของผู้ใช้โดยตรง เพราะอาจไปรบกวน input ปัจจุบันได้ ระบบ test จึงแยกเป็น 2 ชั้น:

1. Automated gate ที่ deterministic และรันได้ทุกครั้งก่อน release
2. Runtime/VM gate สำหรับยืนยัน behavior จริงหลัง login หรือก่อนปล่อย production

## One-command automated gate

```powershell
powershell -ExecutionPolicy Bypass -File scripts\test.ps1
```

สิ่งที่ gate นี้ตรวจ:

- build `HotkeyLanguageSwitcher.exe` ด้วย `-Wall -Wextra -Werror`
- build และรัน unit tests ของ hotkey core
- sync binary จาก `build/` ไป `installer/`
- verify SHA256 ของ binary ทั้งสองตำแหน่งว่าตรงกัน
- static check ว่า setup.ps1 สร้าง task แบบ at-logon/highest/no time limit/battery-safe, cleanup startup เก่า และไม่มี legacy `startup.bat`/`config.bat`
- `git diff --check` เพื่อจับ whitespace errors

## Automated coverage matrix

`tests/test_hotkey_core.c` ครอบคลุม behavior สำคัญที่เคยทำให้เกิด loop หรือ input ค้าง:

- non-CapsLock key ต้อง pass-through
- physical CapsLock keydown ต้อง suppress และ trigger language switch แค่ครั้งเดียว
- CapsLock auto-repeat ตอนกดค้างต้องไม่ switch ซ้ำ
- keyup ต้อง reset physical key state
- rapid repress ภายใน debounce window ต้องไม่ switch ซ้ำ
- missing keyup/stuck key ต้อง reset หลัง timeout
- third-party injected CapsLock ต้องถูก block และต้องไม่ trigger switch
- own tagged synthetic CapsLock สำหรับ normalize ต้อง pass-through เท่านั้น
- `WM_SYSKEYDOWN` / `WM_SYSKEYUP` ต้อง behave เหมือน key path ปกติ
- unknown CapsLock messages ต้อง suppress แบบไม่ side effect
- DWORD tick wrap-around ต้องไม่ทำ debounce พัง
- modifier injection ต้องกด/ปล่อยเฉพาะ Ctrl/Shift ที่โปรแกรมเป็นคนกดเอง
- CapsLock normalize input ต้องมี marker เฉพาะของโปรแกรม

## Runtime diagnostics after login

หลังติดตั้งหรือหลัง reboot ให้รัน:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\diagnose.ps1
```

สคริปต์นี้ตรวจสถานะจริงของ user session:

- process รันอยู่ และไม่เกิน 1 ตัว
- scheduled task `HotkeyLanguageSwitcher` มีอยู่, มี logon trigger, run level Highest, ไม่มี time limit, ไม่ถูกบล็อกตอนใช้แบต
- task ชี้ไปที่ `C:\Program Files\HotkeyLanguageSwitcher\HotkeyLanguageSwitcher.exe`
- binary ที่ติดตั้งมี hash ตรงกับ `installer\HotkeyLanguageSwitcher.exe` หรือไม่
- ไม่มี startup แบบเก่า (Registry Run / Startup folder) ค้างอยู่

## VM/manual production gate

ก่อน release ที่แตะ keyboard hook หรือ startup behavior ให้ทดสอบใน Windows VM หรือเครื่องทดสอบ:

- clean install ด้วย `installer\reset.bat` แล้ว `installer\install.bat`
- reboot แล้วรัน `scripts\diagnose.ps1` และกด CapsLock ทันทีที่เห็น desktop ต้องสลับได้ภายในไม่กี่วินาที
- กด CapsLock ครั้งเดียว ต้องสลับภาษา 1 ครั้ง และ CapsLock state ต้องไม่ ON
- กด CapsLock ค้าง 3 วินาที ต้องไม่สลับภาษารัว
- กด CapsLock เร็วหลายครั้ง ต้องไม่มี loop หรือ stuck modifier
- ทดสอบตอน Ctrl หรือ Shift ถูกกดค้าง ต้องไม่เกิด sticky modifier หลังปล่อย
- เปิดแอปปกติและแอป elevated ต้องสลับภาษาได้ทั้งคู่
- double-click exe ซ้ำขณะ task รันอยู่ ต้องไม่มี process ที่สอง
- ทดสอบ login ที่ CapsLock เปิด ON อยู่ก่อน ต้องถูก normalize เป็น OFF
- ถ้ามี keyboard vendor software/IME tool ให้เปิดพร้อม startup แล้ว reboot ซ้ำอย่างน้อย 5 รอบ

## Release rule

ห้ามปล่อย binary ใหม่ถ้า `scripts\test.ps1` ไม่ผ่าน หรือถ้า VM/manual gate พบ process ซ้ำ, startup path ซ้ำ, CapsLock state ค้าง, หรือ language switch เกิดมากกว่า 1 ครั้งต่อ physical press.
