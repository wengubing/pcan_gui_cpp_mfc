# 01-remove-minimal-rebuild: Remove deprecated minimal rebuild setting

移除 Debug 配置中通过 `<MinimalRebuild>true</MinimalRebuild>` 生成的 `/Gm` 选项。该选项已被 v145 工具集弃用；仅调整项目设置，不改变调试运行库、预编译头或其他编译选项。编辑 `.vcxproj` 前必须卸载项目，编辑后验证 XML，再重新加载项目。

## Research findings

- 受影响项目：`D:\Manual\USBCAN卡资料_PCAN\04-二次开发\PCAN-Basic_Windows-4.7\Samples\Samples\GUI\C++_MFC\PCANBasicExample.vcxproj`。
- `Debug|Win32` 和 `Debug|x64` 的 `<ClCompile>` 节点各包含 `<MinimalRebuild>true</MinimalRebuild>`，对应评估中的 D9035 `/Gm` 警告。
- `Release|Win32` 和 `Release|x64` 已为 `MinimalRebuild=false`，无需修改。
- 项目使用 v145 工具集、MFC 和预编译头；本任务不涉及源代码、运行库、语言标准或其他编译设置。
- 验证顺序：卸载项目，删除两个 Debug 配置中的 MinimalRebuild 节点，验证 `.vcxproj`，重新加载项目，然后执行增量解决方案构建。

**Done when**: `.vcxproj` 不再为任何配置生成 `/Gm`，项目文件验证通过，增量构建不再报告 D9035，且没有新增警告或错误。
