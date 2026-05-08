@echo off
REM reinstall.bat -- runs ON THE VM. Safe re-install of hidtouch.
REM
REM Why this is non-trivial:
REM   1. KMDF driver service hidtouch.sys cannot be sc-stopped while the
REM      control device singleton is alive (sc returns 1052). New sys files
REM      take effect only after a reboot. So we don't try to unload here --
REM      a freshly-pushed sys is picked up on the *next* boot.
REM   2. Repeated devcon install accumulates ROOT\HIDCLASS\NNNN entries.
REM      Each remove leaves a phantom with ConfigFlags=0x40 ("needs
REM      reinstall"); next install picks the next free NNNN. After enough
REM      cycles PnP gets confused and the device stops actually starting
REM      (mshidkmdf/mtconfig stay STOPPED). We purge phantoms before install.
REM   3. The hardware ID in the INF is "root\hidtouch", but devcon under
REM      HIDClass installs the device as ROOT\HIDCLASS\NNNN, not ROOT\HIDTOUCH.
REM      Match against ROOT\HIDCLASS in cleanup, not ROOT\HIDTOUCH.
setlocal EnableExtensions
set HERE=%~dp0
cd /d "%HERE%"

echo [reinstall] removing live HID children...
"%HERE%devcon.exe" remove "HID\HIDTOUCH*"        >nul 2>&1
"%HERE%devcon.exe" remove "HID\HIDTOUCH&COL01*"  >nul 2>&1
"%HERE%devcon.exe" remove "HID\HIDTOUCH&COL02*"  >nul 2>&1

echo [reinstall] removing live ROOT\HIDCLASS\* parents...
"%HERE%devcon.exe" remove "ROOT\HIDCLASS*"       >nul 2>&1

echo [reinstall] purging phantom Enum\ROOT\HIDCLASS subkeys...
REM This VM is single-purpose -- no other HIDClass root device should ever
REM live here. Wipe the whole branch; PnP will rebuild it on install.
for /f "tokens=*" %%K in ('reg query "HKLM\SYSTEM\CurrentControlSet\Enum\ROOT\HIDCLASS" 2^>nul ^| findstr /I "HIDCLASS\\"') do (
    reg delete "%%K" /f >nul 2>&1
)

echo [reinstall] purging phantom Enum\HID\HidTouch* subkeys...
REM mshidkmdf creates one HID PDO per top-level collection (Col01, Col02);
REM each install adds a new instance under those, accumulating phantoms.
REM Wipe the whole HidTouch* subtree -- mshidkmdf rebuilds on next start.
reg delete "HKLM\SYSTEM\CurrentControlSet\Enum\HID\HidTouch"        /f >nul 2>&1
reg delete "HKLM\SYSTEM\CurrentControlSet\Enum\HID\HidTouch&Col01"  /f >nul 2>&1
reg delete "HKLM\SYSTEM\CurrentControlSet\Enum\HID\HidTouch&Col02"  /f >nul 2>&1

echo [reinstall] devcon install hidtouch.inf root\hidtouch
"%HERE%devcon.exe" install "%HERE%hidtouch.inf" "root\hidtouch"
set RC=%ERRORLEVEL%
echo [reinstall] devcon exit=%RC% (0 ok, 1 needs reboot)

echo [reinstall] device tree:
"%HERE%devcon.exe" findall =HIDClass * | findstr /I "HIDCLASS HIDTOUCH"

echo [reinstall] services:
sc query mshidkmdf | findstr /C:"STATE"
sc query mtconfig  | findstr /C:"STATE"

if %RC% NEQ 0 if %RC% NEQ 1 (
  echo [reinstall] devcon install FAILED
  exit /b 1
)
echo [reinstall] OK
exit /b 0
