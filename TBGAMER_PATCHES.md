# TBGamer SDK 2.3.38 项目补丁审计

审计日期：2026-11-14。工作分支：`codex/sdk-upgrade-20261114`。

## 基线与升级来源

- 原项目 SDK：`tbgamer_dev` / `f8dad96a4612fe5c8d96c89d6698e150c5af1f97`。
- 原官方基线：`5cd17516d`，版本字段为 2.3.32。
- 最新官方 release tag：`v2.3.38` / `946f92a07af31a3fb66da75675b539289037fcc0`。
- 本次采用官方 `master` / `eba87edced5a03ce8bcad8cf6002494982f83a5f`，包含上述 release，及之后的失效 Kconfig 引用清理和 CI 凭据方式更新。
- 官方在 2.3.37 重整了历史，新旧分支没有共同祖先。本次以 `5cd17516d` 为显式三方合并基线移植项目差异，合并提交保留原项目和新官方两个父提交。

“项目补丁”是项目 SDK 相对官方基线增加的修改，不代表都应永久保留。以下逐项判断是否已由新版官方提供。

## 已移除的重复补丁

旧 `ee2cfb32e` 从官方 2.3.33 回移的经典蓝牙 sniff/SCO 修复已包含在 2.3.38：

- SCO 的 mSBC 编解码条件判断；
- 默认链路 sniff policy；
- HCI Mode Change 事件订阅；
- sniff 进入、退出与连接状态同步。

本次采用官方 `hci_core.c` 和 `hci_host.h`，两文件相对 `upstream/master` **零差异**。移除的是本地重复实现，功能由官方代码继续提供。

## 仍需保留的项目补丁

| 范围 | 保留内容 | 未移除原因 |
| --- | --- | --- |
| BL616 Flash | 80 MHz 初始化失败后回退 60/48 MHz，复位 PLL | 官方没有等效回退 |
| USB Audio | Linux UAC 的 SET_MIN / SET_MAX / SET_RES 请求确认 | 官方处理仍不等效 |
| USB 标准请求 | MS OS v1 空指针与长度保护、属性数量、正常 clear-halt 降至 debug | 官方没有相同边界处理与日志调整 |
| 蓝牙 buf | 注册应用 HID 收发缓冲池 | 项目自定义缓冲池接口 |
| HCI | 发送失败保留缓冲区所有权，避免二次释放 | 官方未提供等效修复 |
| BR L2CAP | 有界信令解析、pending identifier 保留、连接清理完成通知、HCI 完成回调 | 官方新增 LE 修复不等同于这些 BR 修改 |
| BR 超时 | INFO/CFG 6 秒，CONN 15 秒 | 项目连接策略 |
| USB VDMA | 按实际传输方向分发、共享 EP3 仲裁 | 项目 DS 音频与 HID 共用端点所需 |
| USB 初始化 | 运行时 FS 选择并保留默认行为、初始化时保持断开挂接 | 项目枚举/启动恢复顺序所需 |

上述代码及原有 Bluetooth 回归测试一并保留。不是仅按提交标题判断：已比较旧项目差异和新版对应函数。

## 本次新增的构建适配

- `cmake/bflb_flash.cmake`：优先采用 `PYTHON_EXE`，否则查找真实 Python 解释器。Windows CMake 3.25 的 `execute_process` 不能直接运行项目的 `python3.cmd` 包装器。
- `cmake/toolchain.cmake`：`CCACHE_DISABLE=1` 时不添加 ccache 启动器。此次 Windows 环境中旧 ccache/超长参数组合会出现头文件查找失败。
- 固件侧另有 Kconfig、Windows include response file、E907 DSP 汇编兼容适配，详见固件仓库 `docs/SDK-2.3.38-upgrade.md`。

## 验证与限制

实际合并后的 SDK 通过 HCI 所有权、BR L2CAP、USB EP0 描述符、共享 EP3、延迟挂接及 RF 启动恢复回归。官方 LittleFS/KV 的 emulated-flash 测试通过（提交错误、容量耗尽、覆盖与重挂载；Windows 主机版本未启用 sanitizers）。

这是供实机验证的升级候选。主机回归与成功链接不能代替真实蓝牙控制器、USB 主机、射频启动及音频时序验证。
