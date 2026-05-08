# TouchDirve 用户使用指南

> Win7 虚拟触摸驱动 + 全局滚动/缩放热键工具

---

## 这是什么

把这套工具装到一台 **Win7 SP1 x86/x64** 电脑上之后，你就能：

1. 在系统里多出一个虚拟触摸屏设备（控制面板 → 笔和触摸 里能看到完整 Touch 选项）
2. 用一个**后台运行的小程序 + 全局热键**给任何窗口发"鼠标轮滚动"和"Ctrl+滚轮缩放"
3. 任何能用鼠标轮的程序都支持：Chrome、IE、Firefox、QQ 浏览器、记事本、画图、PotPlayer、QQ 音乐、网易云音乐、Word、Excel、PDF Reader 等等

**和真触摸屏的区别**：
- ✅ 滚动、缩放、翻页这些动作完全像真触摸
- ❌ 多指捏合、旋转手势这些做不出（Win7 系统级限制，不是这个工具的问题）

---

## 系统要求

- **Win7 SP1 x86 或 x64**（Win10/11 也能装但用不到）
- 管理员权限（首次安装驱动需要）
- 大约 5MB 磁盘空间

---

## 安装

### 第一步：拷贝文件到电脑

把整个 `payload` 文件夹（或者只 `payload/x64/` 子文件夹，看你的系统位数）拷到目标电脑的 `C:\hidtouch\`，结构应该是：

```
C:\hidtouch\
├── hidtouch.sys              # 驱动
├── hidtouch.inf              # 安装信息
├── hidtouch.cat              # 签名
├── WdfCoInstaller01009.dll   # KMDF 协同安装
├── devcon.exe                # 设备管理工具
├── app.exe                   # 全局热键控制器
├── HidTouchTestCA.cer        # 测试证书
├── HidTouchTestSigner.cer    # 签名者证书
├── setup_vm.bat              # 首次安装脚本
└── reinstall.bat             # 重新安装驱动脚本
```

### 第二步：首次安装驱动（需要重启）

1. 用**管理员权限**打开 cmd（开始 → cmd → 右键"以管理员身份运行"）
2. 执行：`cd C:\hidtouch && setup_vm.bat`
3. 脚本会做这些事：
   - 关 UAC（需要重启生效）
   - 装测试证书到 LocalMachine\Root + TrustedPublisher
   - 启用 testsigning 模式（`bcdedit /set testsigning on`）
   - 10 秒后自动重启电脑

### 第三步：重启后再装驱动

1. 重启完登录桌面
2. 用**管理员权限** cmd 执行：`cd C:\hidtouch && reinstall.bat`
3. 看到 `[reinstall] OK` + `mshidkmdf STATE: RUNNING` + `mtconfig STATE: RUNNING` = 装好了

### 第四步：验证设备就绪

在 cmd 跑 `C:\hidtouch\sysprobe.exe`，应该看到：
```
SM_DIGITIZER (94) = 0xC1
SM_MAXIMUMTOUCHES (95) = 10
  0x01 INTEGRATED_TOUCH  : 1
  0x40 MULTI_INPUT       : 1
  0x80 READY             : 1
```

打开 **控制面板 → 硬件和声音 → 笔和触摸 → 触摸** 标签，应该能看到完整的设置选项（"将手指用作输入设备" 复选框、双击距离、长按时间等）。

---

## 使用全局热键工具

### 启动 app.exe

桌面双击 `C:\hidtouch\app.exe`，会出现一个窗口标题为：
> TouchDirve 全局热键 (Win+Shift+J/K/I/O/0/Q)

窗口里会显示**热键说明**和**实时状态**。

**这个窗口可以最小化**，热键照样 work（这是后台监听的）。

### 热键列表

| 热键 | 功能 | 等价于 |
|---|---|---|
| `Win+Shift+J` | 向下滚动 | 鼠标轮向下滚 6 格 |
| `Win+Shift+K` | 向上滚动 | 鼠标轮向上滚 6 格 |
| `Win+Shift+I` | 放大 | Ctrl+滚轮上 4 次 |
| `Win+Shift+O` | 缩小 | Ctrl+滚轮下 4 次 |
| `Win+Shift+0` | 重置缩放 | Ctrl+0 |
| `Win+Shift+Q` | 退出程序 | 关闭 app.exe |

> 选用 `Win+Shift` 组合是为了避开输入法（搜狗 / QQ 拼音常抢 Ctrl+Alt 系列）和其他常用快捷键的冲突。

### 使用方法

**核心规则**：把鼠标光标移到要滚动的窗口上，然后按热键。

举例：
1. 打开 Chrome 浏览百度
2. **不要切窗口**，直接把鼠标放在 Chrome 网页区域
3. 按 `Win+Shift+J` → 网页向下滚动
4. 按 `Win+Shift+K` → 网页向上滚动
5. 按 `Win+Shift+I` → 网页放大
6. 按 `Win+Shift+O` → 网页缩小
7. 按 `Win+Shift+0` → 重置到 100%

**支持的程序**：任何能用鼠标轮的程序都支持，包括：
- 网页浏览器（Chrome、IE、Firefox、QQ 浏览器、360）
- 文档阅读（PDF、Word、Excel、PowerPoint、记事本）
- 文件管理（Windows 资源管理器）
- 图像/绘图（画图、Photoshop）
- 多媒体播放（QQ 音乐、网易云、PotPlayer、Windows Media Player）
- 编辑器（VS Code、Notepad++、Sublime Text）

---

## 常见问题

### 启动 app.exe 时显示 "X/6 热键已注册" 而 X < 6

说明 Win+Shift+J/K/I/O/0/Q 中某个热键被别的程序抢了。`C:\hidtouch\app.log` 里会显示具体哪个 ID 注册失败（gle=1409 = ERROR_HOTKEY_ALREADY_REGISTERED）。

**解决方案**：
- 暂时关掉抢占热键的程序（常见嫌疑：QQ、微信、TIM、AutoHotkey 脚本）
- 或者修改 app.cpp 换成不冲突的热键（需要重新 build）

### 窗口启动时显示"未响应"

旧版本（Ctrl+Alt 组合）在某些机器上 RegisterHotKey 阻塞导致窗口超 5 秒不响应。**最新版（Win+Shift 组合）已修复** —— 启动后立即显示窗口，热键注册在窗口可见后异步进行。

### 按热键没反应

1. 看 app.exe 窗口里的"实时状态"行 —— 按热键时应该闪现"向下滚动..."等文字。
   - 如果有闪现 = 热键收到了，但鼠标光标位置没在能滚动的窗口上
   - 如果没闪现 = 热键被其他程序抢占了
2. 看 `C:\hidtouch\app.log`：每按一次热键应该有一行 `scroll-down 6x at cursor` 之类
3. 确认你的**鼠标光标**在目标窗口上方（光标位置决定滚轮发给谁）

### Chrome 缩放后字体太大/太小

按 `Ctrl+Alt+0` 重置缩放到 100%。

### 开机自启

希望 app.exe 在每次登录时自动启动：
```
C:\hidtouch\install_autostart.bat
```
脚本会写入 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`，下次登录时 Windows 自动启动 app.exe。

取消自启：
```
C:\hidtouch\uninstall_autostart.bat
```

### 卸载

要卸载驱动：
```
cd C:\hidtouch
devcon.exe remove "ROOT\HIDCLASS\*"
sc delete hidtouch
```

要卸载 app.exe：直接删除 `C:\hidtouch\app.exe` 即可（如果在运行先关闭它）。

要彻底关闭 testsigning 模式（驱动不再加载）：
```
bcdedit /set testsigning off
```
然后重启。

---

## 文件清单

| 文件 | 作用 |
|---|---|
| `hidtouch.sys` | KMDF 内核虚拟触摸驱动 |
| `hidtouch.inf` | 驱动安装信息 |
| `hidtouch.cat` | 驱动签名目录 |
| `app.exe` | 全局热键控制器（用户主要用的程序） |
| `setup_vm.bat` | 首次部署脚本（关 UAC + 装证书 + 启 testsigning + 重启） |
| `reinstall.bat` | 重新装/升级驱动脚本 |
| `sysprobe.exe` | 诊断工具：dump 系统触摸状态 |
| `hidcaps.exe` | 诊断工具：dump HID 设备 capabilities |
| `stats.exe` | 诊断工具：dump 驱动统计信息 |
| `gesture.exe` | 诊断工具：headless 注入 1-finger swipe / pinch |
| `testinj.exe` | 诊断工具：注入指定数量帧 |
| `touchsink.exe` | 诊断工具：WM_TOUCH 接收窗口（同进程闭环验证） |

---

## 已知限制

1. **Win7 RDP session 收不到滚轮事件** — 必须在物理键盘前操作（console session）
2. **同时多个 app.exe 实例**会争抢热键，只启动一个
3. **管理员权限程序**（比如 cmd 管理员、TaskManager）默认不接收非管理员发的输入 —— 需要 app.exe 也以管理员权限启动
4. **不支持多指手势**（pinch zoom、rotate）—— 这是 Win7 系统级 + 模拟鼠标的限制，做不到真多指。但 Ctrl+滚轮缩放在 99% 程序里效果一致

---

## 技术原理简介

- **驱动层**：KMDF root-enumerated 虚拟 HID multi-touch device。系统识别为完整 multi-touch screen（INTEGRATED_TOUCH=1, MAXIMUMTOUCHES=10）
- **应用层**：`mouse_event(MOUSEEVENTF_WHEEL)` + `keybd_event(VK_CONTROL)` 模拟鼠标轮滚动和 Ctrl+滚轮缩放。Windows 把鼠标轮事件派发给当前光标位置下的窗口
- **热键**：用户态 `RegisterHotKey` + 全局监听 + 实时触发

驱动本身已经准备好了**真触摸输入路径**（IOCTL → HID 报告 → win32k）—— 但 Win7 上 root-enumerated 虚拟 HID 不能让外部进程收到 WM_TOUCH（OS 设计限制）。所以 app.exe 走"模拟鼠标"路线兜底。如果要在 Win10/11 上用真触摸，驱动可以直接复用（system-level 的 WM_TOUCH 派发在 Win10+ 改了，root-enumerated 也能 work）。

---

## 联系 / 反馈

如果你遇到本文档没覆盖的问题，请把以下信息反馈给开发者：

1. 系统：Win7 SP1 x86 / x64 / 别的
2. 步骤：什么操作触发了问题
3. `C:\hidtouch\app.log` 内容
4. `C:\hidtouch\sysprobe.exe` 输出
