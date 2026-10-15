# CppBuildToolsUpgrade Plan

## Overview

**Target**: 修复 MSVC v145 升级后 PCANBasicExample 项目中的弃用/不安全 C++ 构建警告。
**Scope**: 单个 MFC C++ 项目，涉及一个项目设置和一个源文件调用点；当前没有编译错误。

## Tasks

### 01-remove-minimal-rebuild: Remove deprecated minimal rebuild setting

移除 Debug 配置中通过 `<MinimalRebuild>true</MinimalRebuild>` 生成的 `/Gm` 选项。该选项已被 v145 工具集弃用；仅调整项目设置，不改变调试运行库、预编译头或其他编译选项。编辑 `.vcxproj` 前必须卸载项目，编辑后验证 XML，再重新加载项目。

**Done when**: `.vcxproj` 不再为任何配置生成 `/Gm`，项目文件验证通过，增量构建不再报告 D9035，且没有新增警告或错误。

---

### 02-use-safe-scanf: Replace unsafe sscanf call

将 `PCANBasicExampleDlg.cpp` 中解析设备编号的 `sscanf` 调用替换为 Microsoft CRT 的安全版本 `sscanf_s`，保持十六进制解析、目标变量类型和现有数值处理逻辑不变。

**Done when**: 设备编号解析调用使用 `sscanf_s`，增量构建不再报告 C4996，且没有新增警告或错误。

---

### 03-final-rebuild-validation: Validate complete solution

对解决方案执行完整重建，确认升级评估中的两个警告均已消失，并确认没有引入新的错误或警告。

**Done when**: 最终完整重建成功，结果为 0 errors、0 warnings；评估中记录的 D9035 和 C4996 均不再出现。
