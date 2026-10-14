# 02-use-safe-scanf: Replace unsafe sscanf call

将 `PCANBasicExampleDlg.cpp` 中解析设备编号的 `sscanf` 调用替换为 Microsoft CRT 的安全版本 `sscanf_s`，保持十六进制解析、目标变量类型和现有数值处理逻辑不变。

**Done when**: 设备编号解析调用使用 `sscanf_s`，增量构建不再报告 C4996，且没有新增警告或错误。
