# nice!nano v2 + BlackBerry Q10 触控板测试固件（bbq10_trackpad）

用 nice!nano v2 单独验证 BBQ10 Pro 的 ADBS-A320 触控板。触控板电路与 ZitaotechQ10 Pro
原理图一致，按《nice!nano v2 触控板接线图》连接。驱动来自 ZitaoTech
[BBQ10_Keyboard_Pro](https://github.com/ZitaoTech/BBQ10_Keyboard_Pro) 的
`ZMK source code/zitaotech_q10/custom_driver`（MIT 协议，见 `boards/shields/bbq10_trackpad/LICENSE`）。

## 引脚

| nice!nano | GPIO | 网络 | 说明 |
| --- | --- | --- | --- |
| 2 (D1) | P0.06 | TP_SDA_3V3 | I2C SDA。原图为 P1.09，nice!nano 未引出，改到这里 |
| 3 (D0) | P0.08 | TP_SCL_3V3 | I2C SCL，与原图相同 |
| 中间通孔 | P1.01 | TP_MOTION_1V8 | 与原图相同，`a320.c` 中写死 |
| 10 (D15) | P1.13 | TPBLK | 触控板背光 PWM，与原图相同 |
| 8 (D4) | P0.22 | ROW8 / KEYA | 触控板按压开关 |
| 6 (D2) | P0.17 | COL3 / KEYB | 触控板按压开关 |

I2C 地址 `0x3B`，400 kHz，上拉由 RN1 提供，固件不开内部上拉。
触控板电源来自 nice!nano 的 VCC（P0.13 控制的 EXT_POWER），由板子定义自动管理。

## 编译产物

推送后 GitHub Actions 自动编译，在运行页面底部的 Artifacts 下载 `firmware.zip`：

| 文件 | 用途 |
| --- | --- |
| `nice_nano_v2_bbq10_trackpad.uf2` | 测试固件 |
| `nice_nano_v2_bbq10_trackpad_usb_logging.uf2` | 同上，并通过 USB 串口输出日志，排查问题时用 |
| `settings_reset-nice_nano_v2-zmk.uf2` | 清除蓝牙配对 |

## 烧写

快速按两下 nice!nano 的复位键（或把 RST 对 GND 短接两次），出现 `NICENANO` U 盘后把 `.uf2` 拖进去。

`bbq10-pro` 分支编出的 `zitaotech_q10` 固件只能用于 ZitaoTech 原装主板，代码区从 `0x1000` 开始，
不要刷进 nice!nano。

## 测试方法

先用 USB 线连电脑测试，不需要配对：

1. 手指在触控板上滑动，鼠标指针应跟着移动。
2. 按下触控板，应产生鼠标左键单击。
3. 打开电脑的大写锁定（Caps Lock）后，滑动改为滚轮。
4. USB 连接时，触控板背光约每秒短闪一次，这是原驱动的 USB 模式提示。

蓝牙测试：设备名为 `BBQ10 Trackpad`。蓝牙模式下手指触摸时背光点亮，松开约 5 秒后熄灭；
打开 Caps Lock 时背光呼吸。

## 排查

刷入 `usb_logging` 固件后，用串口工具打开 nice!nano 的 USB 串口（如
`screen /dev/tty.usbmodem* 115200`），日志延迟约 4 秒开始输出：

- `A320 init OK addr=0x3B`：驱动已加载。该日志不代表传感器应答，驱动只检查 I2C 总线是否就绪。
- 滑动时出现 I2C 错误：传感器没有应答，检查 SDA/SCL、Q6 电平转换、RN1 上拉、1V8/2V8 供电。
- 滑动时既不动也没有任何日志：MOTION 没有拉低，驱动不会去读 I2C。检查 P1.01、
  TP_RESET_1V8（R2/C15）、TP_SHUTDOWN_1V8（R1 下拉）和触控板供电。

接线图"注意事项"第 3 条提到的两处电平余量（R2 上拉到 3V3、I2C 传感器侧 VIH），
在读不到 `0x3B` 时优先检查。

## 相对上游驱动的改动

- `a320.c` 未改动。它用到但上游未定义的 `CONFIG_A320_LOG_LEVEL`、`CONFIG_INPUT_A320_INIT_PRIORITY`
  在仓库根目录 `Kconfig` 中定义，`avago,a320` binding 在 `dts/bindings/input/`。
- `trackpad_led.c`：测试板没有键盘背光，未启用 ZMK backlight 时按原机默认背光亮度 100 处理，
  其余逻辑不变。
- 原主板的键盘矩阵、键盘背光（`keyboard_backlight.c`）、RGB、pwm0（P0.06）、EXT_POWER（P1.11）
  和电池分压节点均不保留；EXT_POWER 和电池检测使用 nice_nano_v2 板子自带定义。

完整的 ZitaoTech 原主板固件见 `bbq10-pro` 分支。
