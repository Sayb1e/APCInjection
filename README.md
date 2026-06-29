## 📖 概述

**APCInjection_2** 是一个基于 **APC (Asynchronous Procedure Call)** 机制的 DLL 注入工具，使用纯 C 语言编写。它通
过向目标进程的所有线程排队 APC，诱使目标进程加载指定的 DLL，从而实现代码注入。

> ⚠️ **免责声明：** 本工具仅用于**授权的安全测试、教育培训和逆向工程研究**。未经授权使用此工具攻击他人系统是违法
的。

## 🔧 工作原理

注入过程分为以下步骤：

```
┌─────────────────────────────────────────────────────────┐
│  ① 打开目标进程（PROCESS_VM_OPERATION | VM_WRITE）        │
├─────────────────────────────────────────────────────────┤
│  ② 获取 kernel32!LoadLibraryA 的函数地址                  │
├─────────────────────────────────────────────────────────┤
│  ③ 在目标进程中分配内存，写入 DLL 路径                      │
├─────────────────────────────────────────────────────────┤
│  ④ 枚举目标进程的所有线程                                  │
│     └─ 对每个线程调用 QueueUserAPC(LoadLibraryA, ...)      │
├─────────────────────────────────────────────────────────┤
│  ⑤ 等待目标线程进入可告警等待状态 → APC 被执行               │
│     └─ LoadLibraryA 在目标进程中加载 DLL                    │
└─────────────────────────────────────────────────────────┘
```

## 🚀 使用方法

### 编译

使用 Visual Studio 2022 打开解决方案文件 `APCInjection.sln`，选择 **Release** / **x64** 配置进行编译。

### 运行

```shell
APCInjection_2.exe <PID> <DLL_PATH>
```

| 参数 | 说明 |
|------|------|
| `<PID>` | 目标进程的进程 ID |
| `<DLL_PATH>` | 要注入的 DLL 的完整路径（如 `C:\Path\To\mydll.dll`） |

### 示例

```shell
# 将 myhook.dll 注入到 PID 为 1234 的进程
APCInjection_2.exe 1234 C:\Tools\myhook.dll
```

注入成功后，控制台会显示：
- 每个成功排队的线程 `[ APC successfully queued to thread NNNN ]`
- 排队总数与失败数 `[ Queued: N, Failed: M ]`

按 **Enter** 键清理远程内存并退出。

## 📁 项目结构

```
APCInjection_2/
├── APCInjection.c             # 主程序源代码
├── APCInjection.sln           # Visual Studio 解决方案文件
├── APCInjection.vcxproj       # Visual Studio 项目文件
├── APCInjection.vcxproj.filters # 项目筛选器文件
├── .gitignore                 # Git 忽略规则
└── README.md                  # 本文件
```

## ⚙️ 构建要求

- **操作系统：** Windows 10+
- **工具集：** Visual Studio 2022 (v143)
- **平台：** x64（也支持 Win32）
- **配置：** Debug / Release

## 📝 注意事项

1. **APC 触发条件：** 目标线程必须进入**可告警等待状态**（如调用 `SleepEx`、`WaitForSingleObjectEx`、`MsgWaitFor
MultipleObjectsEx` 等）APC 才会被实际执行。如果目标进程的所有线程都处于繁忙状态，注入不会生效。
2. **权限要求：** 需要以足够权限运行（通常需要管理员权限）才能打开目标进程。
3. **稳定性：** 如果目标进程在 APC 触发前退出，分配在目标进程中的远程内存会被自动释放。
4. **64/32 位兼容：** 注入器与目标进程的位数需要匹配（x64 注入器注入 x64 进程，x86 同理）。

## 📜 许可证

本项目仅供学习研究使用。
