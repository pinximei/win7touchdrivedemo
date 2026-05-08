# TouchDirve — Win7 虚拟 HID 多点触摸驱动

[![Platform](https://img.shields.io/badge/platform-Win7%20SP1-blue)]()
[![Arch](https://img.shields.io/badge/arch-x86%20%7C%20x64-green)]()
[![Driver](https://img.shields.io/badge/driver-KMDF%201.9-orange)]()

一套可在 **Win7 SP1 x86 / x64** 上工作的虚拟 HID 多点触摸屏驱动 + 注入工具 + 接收验证程序 + 全局热键兜底。

驱动把自己暴露成一台**完整的 10 点多点触摸屏**（INTEGRATED_TOUCH=1, MAXIMUMTOUCHES=10）。配套的命令行工具 `gesture.exe` 可以把任意触摸轨迹注入到任何注册了 WM_TOUCH 的窗口，实测可驱动 **画图（mspaint）/ Chrome / IE / 自带 touchsink** 等程序响应触摸。

---

## 特性

- ✅ KMDF 内核虚拟 HID 触摸屏，10 contacts，HID Digitizer / Touch Screen TLC（标准 Microsoft 规范）
- ✅ Win7 SP1 **x86 + x64** 双架构，同一份源码，一条命令出双产物
- ✅ 自签测试证书 + 一键 first-run 部署（关 UAC + 装根证书 + testsigning + 重启）
- ✅ `gesture.exe` 命令行注入工具：swipe / pinch / scroll / zoom，支持 `FindWindow` 抢前台精准注入
- ✅ `touchsink.exe` WM_TOUCH 接收窗口（带可视化轨迹绘制）
- ✅ `app.exe` 全局热键兜底（鼠标轮 / Ctrl+滚轮模拟，给不接 WM_TOUCH 的程序用）
- ✅ 完整诊断工具链：`stats / sysprobe / hidcaps / testinj / autotest`
- ✅ 全链路实测通过：driver → hidclass → win32k → **跨进程 WM_TOUCH** 派发到画图 / Chrome / touchsink

---

## 系统要求

**目标机（Win7）**：

- Windows 7 SP1（x86 或 x64），**Console session 必须**（RDP 不传 WM_TOUCH，Win7 协议级限制）
- 管理员权限（首次部署需要关 UAC + 装证书 + 启 testsigning）

**构建机（Windows 7/10/11）**：

- Visual Studio 2015 Update 3（v140 toolset）
- WDK 8.1（KMDF 1.9）
- PuTTY（plink + pscp，远程部署用，可选）

---

## 快速上手

### 1. 构建

```cmd
cd touchdirve
tools\build.bat
tools\sign.bat
```

输出：

```
payload\x86\hidtouch.sys / .inf / .cat   ← Win7 x86
payload\x64\hidtouch.sys / .inf / .cat   ← Win7 x64
payload\app.exe / touchsink.exe / gesture.exe / ...
payload\HidTouchTestCA.cer               ← 公钥证书（要装到目标机）
payload\HidTouchTestSigner.cer
```

### 2. 部署到目标机

**自动部署**（host 上有 PuTTY + 目标机开了 SSH）：

```cmd
tools\deploy.bat <user> <password> <ip> [first|update]
```

- `first`：首次部署 —— 推文件 + 关 UAC + 装证书 + 启 testsigning + **重启目标机**
- `update`：升级 —— 只推文件，不动证书 / testsigning

**手动部署**（拷文件 + 在目标机本地执行）：

1. 把整个 `payload\` 目录拷到目标机 `C:\hidtouch\`
2. 用**管理员身份** cmd 执行 `C:\hidtouch\setup_vm.bat`（首次）
3. 重启
4. 重启后再执行 `C:\hidtouch\reinstall.bat` 装驱动

### 3. 验证安装

在目标机 cmd 执行：

```cmd
C:\hidtouch\sysprobe.exe
```

应当看到：

```
SM_DIGITIZER (94) = 0xC1
  0x01 INTEGRATED_TOUCH  : 1
  0x40 MULTI_INPUT       : 1
  0x80 READY             : 1
SM_MAXIMUMTOUCHES (95) = 10
```

打开 **控制面板 → 笔和触摸 → 触摸**，能看到完整 multi-touch 选项。

### 4. 端到端测试

**A. 同进程闭环（最快验证）**：

```cmd
C:\hidtouch\touchsink.exe selfinject
```

窗口里出现彩色圆点轨迹，左上角 `frames=` > 0 = 通了。

**B. 跨进程驱动真实应用**：

```cmd
:: 启动 touchsink 等接收
start C:\hidtouch\touchsink.exe

:: 注入到 touchsink 窗口（通过 class name 定位）
C:\hidtouch\gesture.exe scroll TouchSink

:: 或注入到画图
start mspaint.exe
C:\hidtouch\gesture.exe scroll MSPaintApp

:: 或注入到 Chrome
C:\hidtouch\gesture.exe scroll Chrome_WidgetWin_1
```

`gesture.exe` 第二个参数是窗口的 class name（不是程序名），常见值：

| 程序 | class name |
|---|---|
| touchsink（自带） | `TouchSink` |
| 画图 | `MSPaintApp` |
| Chrome | `Chrome_WidgetWin_1` |
| Internet Explorer | `IEFrame` |
| 记事本 | `Notepad` |

---

## 命令行工具

### gesture.exe

```
gesture.exe <gesture> <ClassName>

gesture: swipe | pinch | scroll | zoom
```

工作流：`FindWindowA(class_name)` → `GetWindowRect` 算中心 → `SetForegroundWindow` 抢 z-order → 沿轨迹注入 30 帧 HID multi-touch report 到驱动 → 驱动 complete pending READ → hidclass → win32k → WM_TOUCH 派发给目标窗口。

### touchsink.exe

WM_TOUCH 接收窗口。最大化启动，左上角显示 `frames / evts / lastN / lastMsg`，窗口里把每个 contact 画成彩色圆点。

```cmd
touchsink.exe              :: 普通接收模式
touchsink.exe selfinject   :: 自启 worker 注入到自身窗口（同进程闭环）
```

### app.exe（全局热键兜底）

给不接 WM_TOUCH 的应用使用，走 `mouse_event(WHEEL)` + `keybd_event(VK_CONTROL)` 路线。

| 热键 | 功能 |
|---|---|
| `Win+Shift+J` / `K` | 向下 / 向上滚动 |
| `Win+Shift+I` / `O` | Ctrl+滚轮放大 / 缩小 |
| `Win+Shift+0` | Ctrl+0 重置缩放 |
| `Win+Shift+Q` | 退出 |

操作方法：把鼠标光标移到要操作的窗口上，按热键。

### 诊断工具

```cmd
stats.exe       :: dump driver 内部计数器（InjectCalls/ReadCompletions/...）
sysprobe.exe    :: dump 系统触摸状态（SM_DIGITIZER / TabletInputService / ...）
hidcaps.exe     :: dump 当前 HID 设备的 TLC + 独占状态
testinj.exe     :: headless 注入 N 帧用于压力测试
autotest.exe    :: 端到端自动化测试
```

---

## 已知限制

1. **Win7 RDP session 不传 WM_TOUCH** —— 协议级限制（RDP 7.x），必须 console session 测。Win8+ 才支持 RDP touch。
2. **Console session 才能收触摸** —— `qwinsta` 看 `console` 状态必须是 "活动" 且是当前登录用户
3. **不支持完整 multi-finger gesture（如 pinch zoom 真实双指）的应用层接收** —— `gesture.exe` 注入是真多指，但 Chrome / Win7 GestureRecognizer 在某些应用里只识别滚动/缩放，不识别旋转
4. **管理员权限程序拒绝来自非管理员的输入** —— 注入到管理员窗口时 `gesture.exe` 也要管理员身份运行
5. **同时多个 hidtouch / vmulti instance 会干扰** —— 卸载旧版本要彻底清 phantom device

---

## 项目结构

```
touchdirve/
├── driver/                 KMDF 虚拟 HID 触摸驱动源码
│   ├── driver.c            DriverEntry / EvtDeviceAdd / Power Policy
│   ├── hid.c               HID query routines + GetFeature
│   ├── inject.c            IOCTL_HIDTOUCH_INJECT 处理
│   ├── queue.c             Default queue + manual queue (pending READ)
│   ├── descriptor.h        HID Report Descriptor (10 contacts)
│   └── hidtouch.inx        INF 模板（NTamd64 + NT$ARCH$）
├── app/                    用户态工具源码
│   ├── app.cpp             全局热键兜底程序
│   ├── touchsink.cpp       WM_TOUCH 接收 + 可视化
│   ├── gesture.cpp         命令行触摸注入工具
│   ├── testinj.cpp         批量帧注入测试
│   ├── autotest.cpp        端到端自动化测试
│   ├── stats.cpp           driver 计数器读取
│   ├── sysprobe.cpp        系统触摸状态诊断
│   └── hidcaps.cpp         HID 设备 capability 诊断
├── tools/
│   ├── build.bat           一键构建（x86 + x64 driver + 所有 user-mode 工具）
│   ├── sign.bat            自签测试证书 + 签 cat（x86 + x64）
│   ├── deploy.bat          SSH 自动部署
│   └── cert/               测试证书（公钥 .cer 入库，私钥 .pvk/.pfx 不入库）
└── payload/                构建产物（gitignore）
    ├── x86/                Win7 x86 driver 包
    ├── x64/                Win7 x64 driver 包
    └── *.exe / *.cer       通用工具 + 公钥证书
```

---

## 关键技术细节

### KMDF Power Policy 让出（避免 BSOD 0x10D）

HID minidriver 必须**显式让出** Power Policy Ownership 给 hidclass，否则第一个 system power IRP 来时会触发 `WDF_VIOLATION` (0x10D 0xD `WDF_POWER_MULTIPLE_PPO`) 蓝屏：

```c
// driver.c
WdfFdoInitSetFilter(DeviceInit);
WdfDeviceInitSetPowerPolicyOwnership(DeviceInit, FALSE);  // ← 关键
WdfDeviceCreate(&DeviceInit, ...);
```

### Win7 testsigning 证书链

- CA 装到 `LocalMachine\Root` + `LocalMachine\TrustedPublisher`
- **Signer 也必须装到 `LocalMachine\TrustedPublisher`**（不只是 CA！）
- `bcdedit /set testsigning on` 后重启

### 跨进程 WM_TOUCH 派发规则

win32k 收到 HID input report → 用 X,Y 坐标找到那个点上 z-order 最高的、注册过 `RegisterTouchWindow` 的窗口 → 给它发 WM_TOUCH。所以注入坐标必须落在目标窗口客户区，且目标窗口必须是前台。`gesture.exe` 自动 `SetForegroundWindow + GetWindowRect` 算中心解决这两点。

---

## 致谢

- HID Report Descriptor 形状参考 djpnewton/vmulti
- 项目过程中所有踩坑（BSOD bug、证书链、RDP 限制、phantom device、PnP cache）都已经在 commit history 里完整可见

---

## License

MIT
