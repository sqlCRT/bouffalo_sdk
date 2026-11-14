# TBGamer SDK 2.3.38 项目补丁审计

审计日期：2026-11-14。工作分支：`codex/sdk-upgrade-20261114`。

## 二次对照：蓝牙持久化删除（2026-11-14）

配套 `3.27t4-sdk238`。官方 `eba87edce` 将蓝牙存储从 EasyFlash 接口迁移到
LittleFS/KV 时，在 `settings_delete` 和 `bt_keys_clear` 中把
`lfs_kv_get_blob(key, NULL, 0, &len)` 的返回值当作存在性判断。
该返回值是复制的字节数，长度查询总是返回 0，实际长度写入 `len`，导致存在的键也不删除。
应用清空黑名单调用前者；后者影响 BLE 密钥池，不能据此声称所有 BR 配对删除路径均有问题。

修复为直接调用本来就支持“缺失键删除成功”的 `lfs_kv_del`，保留写入错误返回。
真实 LittleFS/KV 与闪存模拟器的测试已在修复前分别复现两个路径失败，修复后验证
50 次保存/删除/重挂载、缺失键、其他键不受影响、IRK 回调、内存清零及删除提交失败。
测试入口：`tests/bluetooth/run_settings_persistence_tests.py --cc gcc`。

这是一项本次采用的官方版本尚未包含修复的新项目补丁。其余旧项目补丁再次逐文件核对：
14 个代码文件中 10 个与旧项目完全相同，2 个采用官方实现，2 个叠加官方变更；5 个旧测试文件保留。
旧版真实 LittleFS 镜像还完成了“旧版写入 → 新版读取/更新 → 旧版读取”验证，
覆盖配置、二进制密钥、含转义字符的键名与删除结果。完整结论见配套固件的二次审计记录。

## FreeRTOS 内部 WRAM 回退（2026-11-14）

本分支移植诊断 SDK 提交 `397072143edd94797bc64af0981419fedba5b989` 的修复及测试，
配套普通升级固件为 `3.27t2-sdk238`。以下诊断日志是定位依据，未引入诊断分支的其他功能。

`codex/disconnect-snapshot-20261113` 的 d9 实机日志在蓝牙初始化期间两次
进入 `vApplicationMallocFailedHook`，诊断 arena 尚未启动；`mepc=a004879a`
对应最终 ELF 中 `pvPortMalloc` 的失败断点。转储被启动看门狗打断，未包含
失败请求大小、调用者及各堆剩余量，不能据此指定某一次具体分配。

升级漏查了 `heap_3.c` 的策略变化：旧版 `kmalloc(size, 0)` 可使用注册的
OCRAM 和 WRAM，新版仅指定 OCRAM。板级初始化仍注册 32 KiB 内部 WRAM
（0x23028000，位于蓝牙 EM 之外），但 RTOS 分配无法使用它。

增加默认关闭、仅 BL616/BL618 可选的 `FREERTOS_INTERNAL_WRAM_FALLBACK`；
配套固件开启后，保持 OCRAM 优先，失败再尝试内部 WRAM。不会回退到 PSRAM
或蓝牙 EM；全部内部内存不可用时仍保留原失败钩子，释放仍按指针寻找所属堆。
不削减栈、池、功能或诊断记录，不增加运行期日志和预留缓冲。

`tests/run-freertos-internal-heap-tests.py` 编译真实 heap_3/MM/TLSF，覆盖新旧
策略、OCRAM 碎片但 WRAM 可用、OCRAM 优先、10000 次复用、释放归属、WRAM
未注册、两池耗尽、PSRAM 不被使用及失败钩子在恢复调度后恰好执行一次。
这些是主机策略回归，不能替代接收器实机启动验收。

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
