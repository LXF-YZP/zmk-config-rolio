# BBQ10 Keyboard Pro ZMK 配置（zitaotech_q10）

本分支把 ZitaoTech [BBQ10_Keyboard_Pro](https://github.com/ZitaoTech/BBQ10_Keyboard_Pro)
仓库中 `ZMK source code/zitaotech_q10` 的板级代码整理成 zmk-config 结构，
推送后由 GitHub Actions 直接编译出 UF2 固件。上游代码采用 MIT 协议，
原始许可证见 `boards/arm/zitaotech_q10/LICENSE`。

## 目录

```
.github/workflows/build.yml          调用 ZMK v0.3 官方编译流程
build.yaml                           编译矩阵：默认固件 + 清除配对固件
config/west.yml                      固定 ZMK v0.3（Zephyr 3.5）
config/zitaotech_q10.keymap          按键映射，改键只改这里
boards/arm/zitaotech_q10/            板级定义（dts、Kconfig、defconfig）
boards/arm/zitaotech_q10/custom_driver/
                                     A320 触控板、键盘背光、触控板指示灯驱动
dts/bindings/input/avago,a320.yaml   A320 触控板的 devicetree binding
zephyr/module.yml                    把本仓库注册为 board_root / dts_root
```

## 编译

1. 推送到 GitHub 后，Actions 会自动运行 `Build ZMK firmware`，也可以在 Actions 页面手动触发。
2. 运行成功后，在该次运行页面底部的 Artifacts 下载 `firmware.zip`，里面有：
   - `bbq10_pro_default_firmware.uf2`：日常使用的固件
   - `bbq10_pro_reset_firmware.uf2`：清除配对和设置用的固件

ZMK 版本固定为 `v0.3`。ZMK `main` 已升级到 Zephyr 4.1 的新板级格式（HWMv2），
本板级代码是 Zephyr 3.5 格式，不能直接切到 `main`。

## 烧写

1. 键盘用 USB 连接电脑。
2. 切到第 2 层（UPPER），单击 `$` 键进入 bootloader（双击是切换 USB/蓝牙输出）。
3. 电脑上出现名为 `NICENANO` 的 U 盘后，把 `.uf2` 文件拖进去，键盘会自动重启。

固件代码区从 `0x1000` 开始，与 ZitaoTech 官方发布的 UF2 一致；
不要刷入为普通 nice!nano（代码区 `0x26000`）编译的固件。

清除配对：先刷 `bbq10_pro_reset_firmware.uf2`，再进入 bootloader 刷回
`bbq10_pro_default_firmware.uf2`。

## 相对上游的改动

上游代码原样导入在第一个提交里，之后只做了能让它在 zmk-config 中编译所需的最小改动：

- `Kconfig.board` 只保留板子符号。Zephyr 3.5 在 "Board Selection" choice 中加载该文件，
  其中的触控板、指示灯配置项移到 `Kconfig`。
- `Kconfig` 补充 `a320.c` 用到但上游未定义的 `CONFIG_A320_LOG_LEVEL` 和
  `CONFIG_INPUT_A320_INIT_PRIORITY`。
- `Kconfig.defconfig` 补充 `config BOARD` 默认值。
- 新增 `avago,a320` 的 devicetree binding。
- `zitaotech_q10-layouts.dtsi` 中不存在的 `&matrix_transform0` 改为 `&default_transform`。
- `keyboard_backlight.c` 中 `case 0:` 后紧跟变量声明，补上花括号，避免旧版 C 标准报错。
- 默认 keymap 移到 `config/`，其余按键行为、引脚、分区、驱动逻辑保持不变。
