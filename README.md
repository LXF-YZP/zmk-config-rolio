# Q20：ZMK Studio / DYA Studio 基础支持

此分支将 XIAO BLE + ADBS-A320 触控板键盘迁移到官方 ZMK v0.3 系列，
通过标准 Studio 协议兼容 [ZMK Studio](https://zmk.studio/) 和
[DYA Studio](https://studio.dya.cormoran.works/)。ZMK 版本固定在 `config/west.yml`。

## 支持范围

- 三个机械键和触控板按压均可在线改键；默认依次为 RALT、空格、Enter、鼠标左键。
- 默认层之外预留三个空层，可在 Studio 中启用和配置。
- 基础适配不包含 DYA 的动态宏、动态组合键、连接管理或鼠标调参 RPC。
  DYA 的 PMW3610 驱动设置不适用于 A320。
- `RALT + Enter` 保留为清除当前蓝牙配对的组合键；组合键仍在固件中定义。
- 新增 `空格 + 触控板按压` 解锁 Studio。两键需在 150 ms 内按下，然后松开。
  这个固定入口不依赖 Enter，也不依赖 Studio 中修改后的单键绑定。
  空格与触控板按压会因此最多延迟 150 ms，短按会在松开时送出。
- 布局是四个位置的示意图，不是按实物尺寸测绘；位置顺序与扫描和键位表一致。

## 连接与使用

1. 下载 Actions 的 `firmware` 附件，刷入 `q20-seeeduino_xiao_ble-studio.uf2`。
   `q20-seeeduino_xiao_ble-debug.uf2` 仅用于串口日志，不支持 Studio。
2. 用 USB 数据线连接，在桌面 Chrome/Edge 打开上面的任一 Studio 网站。
   macOS 网页端请用 USB，不要选择网页 BLE 连接。
3. 选择此键盘的 USB 串口。USB 产品名仍为 `bbcase`，蓝牙显示名为 `q20`；
   注意不要选到同时连接的其他 ZMK 键盘。
4. 同时按空格和触控板，再松开，在界面中改键并保存到键盘。
   初次测试建议先把正常的空格临时改为 Enter，验证电脑是否能收到回车，再恢复。
5. Studio 保存的键位会优先于后续刷入的 `.keymap` 默认值；要恢复固件默认键位，
   使用 Studio 的 Restore Stock Settings。先备份自己修改的键位。

USB 和 BLE 同时连接时，让键盘输出与 Studio 使用相同的连接通道；
如曾手动选过蓝牙输出，可通过 `&out OUT_USB` 切回 USB。

## A320 迁移说明

旧分支把鼠标循环写在 `main.c` 中，并通过键盘名称选择坐标方向。
现在 `drivers/input/input_a320.c` 在独立线程中读取 A320，交给官方 ZMK input listener
发送鼠标报告，避免替换官方主循环。

- I2C 地址、GPIO、背光及四个键位的扫描顺序沿用现有硬件定义。
- `swap-xy` 明确保留 `x = Delta_Y, y = Delta_X`；名称不再决定鼠标方向。
- 100 Hz 轮询、原有 1.5 倍移动倍率和锁定键控制的滚动模式保留。
- Motion 寄存器每次采样只读一次；I2C 失败和传感器溢出时不发送伪位移。
- 位移按有符号值解码，放大后使用 16 位值，消除旧代码 8 位溢出造成的反向跳动风险。
  滚动的小位移分支也修正了横向符号与 2、20 边界漏判。

寄存器地址与旧滚动行为参考
[ZitaoTech/zmk bbkeyboard_tp](https://github.com/ZitaoTech/zmk/tree/6bb7b0244fa644368cbce7a4790c5edff7ecb4f5)。
这次移植不能证明此前 Enter 无响应或鼠标跳动的实机根因。
构建通过后仍需验证移动方向、鼠标点击、键位保存和重启后的持久化。

## 开发验证

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined tests/test_a320_motion.c -o /tmp/test-a320-motion
/tmp/test-a320-motion
```

GitHub Actions 同时运行该检查，并构建 Studio 与独立 USB 日志固件。

官方文档：[Studio](https://zmk.dev/docs/features/studio)；
DYA 文档：[基础兼容](https://studio.dya.cormoran.works/developer-guide/level-1)。
