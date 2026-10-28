# Q20 触控板键盘 · nice!nano v2 版（q20-nice-nano）

在 `q20` 分支基础上把主控从 Seeeduino XIAO nRF52840 换成 nice!nano v2。
触控板（ADBS-A320，I2C 地址 `0x3B`）经 trackpad_breakout 转接板连接，接线与
`trackpad_breakout/out/nice_nano_wiring.pdf` 一致。固件仍基于 ZitaoTech 的 ZMK 分支
`bbkeyboard_tp`（内置 A320 驱动），键盘名保持 `bbq20`。

## 接线

nice!nano 正面朝上、USB 在上，"第 N 脚"从 USB 一端数起，不算电池焊盘 B−/B+。

| 连接对象 | nice!nano v2 | 位置 | 说明 |
| --- | --- | --- | --- |
| 转接板 J2 GND | GND | 右列第 2 脚 | 任意 GND 均可 |
| 转接板 J2 3V3 | VCC (3.3V) | 右列第 4 脚 | **不能接 RAW / B+** |
| 转接板 J2 SCL | P0.20 (D3) | 左列第 6 脚 | i2c0 默认引脚 |
| 转接板 J2 SDA | P0.17 (D2) | 左列第 5 脚 | i2c0 默认引脚 |
| 转接板 J3 KEYA | GND | 左列第 4 脚 | 触控板按压开关一端 |
| 转接板 J3 KEYB | P0.06 (D1) | 左列第 1 脚 | 触控板按压，鼠标左键 |
| 转接板 J3 BLK | P0.08 (D0) | 左列第 2 脚 | 触控板背光 PWM |
| 转接板 J3 MOT | P0.22 (D4) | 左列第 7 脚 | 固件不使用，驱动轮询 I2C |
| Kailh 键 1（RALT） | P0.24 (D5) | 左列第 8 脚 | 另一端接 GND |
| Kailh 键 2（空格） | P1.00 (D6) | 左列第 9 脚 | 另一端接 GND |
| Kailh 键 3（回车） | P0.11 (D7) | 左列第 10 脚 | 另一端接 GND |
| 复位键 | RST | 右列第 3 脚 | 另一端接 GND |

与 XIAO 版的差别：

- 按键全部改成"一端接地 + 内部上拉"的 direct 扫描，不需要二极管和行线。
  只测触控板时 Kailh 键可以不接。
- 触控板 KEYA 直接接地，KEYB 读低电平，不再按 1×1 矩阵扫描。
- 触控板供电来自 nice!nano 的 VCC，由 P0.13（EXT_POWER）控制。不要在固件里关闭外部供电，
  否则触控板会断电。
- 新增触控板背光：上电默认开启，亮度 40%；同时按 Kailh 键 1 和键 3 开关背光。

## 编译产物

推送后 GitHub Actions 自动编译，在运行页面底部的 Artifacts 下载 `firmware.zip`：

| 文件 | 用途 |
| --- | --- |
| `q20-nice_nano_v2-zmk.uf2` | 正式固件 |
| `q20-nice_nano_v2-debug.uf2` | 同上，并通过 USB 串口输出日志，排查触控板 I2C 问题时用 |

## 烧写

快速按两下复位键（或把 RST 对 GND 短接两次），出现 `NICENANO` U 盘后把 `.uf2` 拖进去。
蓝牙设备名为 `bbq20`。

## 调整

- 指针方向不对：`boards/shields/q20/Kconfig.defconfig` 里的 `ZMK_KEYBOARD_NAME` 决定
  fork 的 `main.c` 用哪套坐标映射（同时也是蓝牙名）。`bbq20` 为默认；改成 `bbq10` 则 X、Y 都反向；
  改成 `bb9900` 则 X、Y 轴对调（x 取 Y 读数、y 取 X 读数）。
- 速度、滚动间隔：`config/q20.conf` 里的 `CONFIG_TRACKPAD_*`。
- 背光默认亮度：在 `config/q20.conf` 加 `CONFIG_ZMK_BACKLIGHT_BRT_START=<1~100>`。
