# 02-use-safe-scanf: Replace unsafe sscanf call

将 `PCANBasicExampleDlg.cpp` 中解析设备编号的 `sscanf` 调用替换为 Microsoft CRT 的安全版本 `sscanf_s`，保持十六进制解析、目标变量类型和现有数值处理逻辑不变。

## Research findings

- 受影响文件：`D:\Manual\USBCAN卡资料_PCAN\04-二次开发\PCAN-Basic_Windows-4.7\Samples\Samples\GUI\C++_MFC\PCANBasicExampleDlg.cpp`。
- 唯一命中位于 `CPCANBasicExampleDlg::OnEnKillfocusTxtparamdevnumber()` 的设备编号分支，当前为 `sscanf(txtParameterDevNumber.GetBuffer(), "%X", &newValue);`。
- 项目配置为 `CharacterSet=MultiByte`，因此该 CString 缓冲区与 `sscanf_s` 的窄字符签名匹配。
- 格式串只解析整数，不涉及字符串目标；`sscanf_s` 不需要额外的缓冲区长度参数，直接替换函数名即可。
- 保持后续 `MAXUINT` 限制、控件更新和十进制分支逻辑不变。

**Done when**: 设备编号解析调用使用 `sscanf_s`，增量构建不再报告 C4996，且没有新增警告或错误。
