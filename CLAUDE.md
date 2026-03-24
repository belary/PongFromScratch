# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

PongFromScratch 是一个 C++ 项目，从零开始构建经典的 Pong 游戏。这是一个学习/探索项目，专注于在不使用游戏框架的情况下理解游戏开发基础。

## 构建系统

此项目使用 Microsoft Visual C++ (MSVC) 编译器通过批处理脚本构建。

**构建命令：**
```bash
build.bat
```

此脚本会：
1. 设置 MSVC 环境 (vcvars64.bat)
2. 编译 `src/main.cpp` 生成工作区根目录下的 `main.exe`

**运行可执行文件：**
```bash
main.exe
```

## 编译器说明

### MinGW vs MSVC

| 特性 | MinGW | MSVC (本项目使用) |
|------|-------|------------------|
| 编译器 | GCC | cl.exe |
| 调试器 | GDB | Windows 调试器 |
| 开源 | ✅ 是 | ❌ 否 |
| 跨平台 | ✅ 代码可移植到 Linux/macOS | ❌ 主要 Windows |
| VS Code 调试类型 | `cppdbg` | `cppvsdbg` |

**MinGW** (Minimalist GNU for Windows) 是 Windows 上的开源 C/C++ 工具链，包含 GCC 编译器和 Windows API 头文件。

本项目使用 **MSVC**，因为 `build.bat` 调用的是 Visual Studio 的 `vcvars64.bat` 环境和 `cl.exe` 编译器。

## 开发配置

- VS Code 任务已配置用于构建（Ctrl+Shift+B 默认运行 `build.bat`）
- 调试配置 (`launch.json`) 使用 `cppvsdbg` 类型
  - `cppvsdbg` = Windows 调试器（适用于 MSVC 编译的程序）
  - 与 `cppdbg` 不同：后者使用 GDB/LLDB（适用于 MinGW/WSL/Linux/macOS）

## 项目结构

- `src/main.cpp` - 入口点和游戏逻辑
- `build.bat` - 使用 MSVC cl.exe 的构建脚本
- 输出文件（`main.exe`、`main.obj`）生成在工作区根目录
