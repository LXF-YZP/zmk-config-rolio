# Q20 触控板键盘 · nice!nano v2 矩阵版（q20-nice-nano-matrix）

在 `q20-nice-nano` 分支基础上，触控板按压改回 `q20` 分支的 1×1 矩阵扫描（KEYA 接 GPIO 而不是 GND），
并去掉三枚 Kailh 按键。
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
| 转接板 J3 KEYA | P0.24 (D5) | 左列第 8 脚 | 触控板按压开关一端，扫描时输出高电平 |
| 转接板 J3 KEYB | P0.06 (D1) | 左列第 1 脚 | 触控板按压，下拉输入，鼠标左键 |
| 转接板 J3 BLK | P0.08 (D0) | 左列第 2 脚 | 触控板背光 PWM |
| 转接板 J3 MOT | P0.22 (D4) | 左列第 7 脚 | 固件不使用，驱动轮询 I2C |
| 复位键 | RST | 右列第 3 脚 | 另一端接 GND |

与 `q20-nice-nano` 分支的差别：

- 触控板 KEYA 接 P0.24 而不是 GND，按 1×1 矩阵扫描：KEYA 输出高电平，KEYB 下拉输入，
  按下时读到高电平（与 `q20` 分支相同）。
- 不接 Kailh 按键，键位表只有触控板按压一个键（鼠标左键）。
- 触控板供电来自 nice!nano 的 VCC，由 P0.13（EXT_POWER）控制。不要在固件里关闭外部供电，
  否则触控板会断电。
- 触控板背光：上电默认开启，亮度 100%。没有 Kailh 键，所以去掉了背光开关组合键。

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
- 背光默认亮度：`config/q20.conf` 里的 `CONFIG_ZMK_BACKLIGHT_BRT_START=<1~100>`。
