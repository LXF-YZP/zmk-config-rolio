cd /workspaces/zmk/app/
west build -s /workspaces/zmk/app -p -d build/left -b nice_nano_v2 -S studio-rpc-usb-uart -- -DSHIELD=sofle_left -DZMK_CONFIG="/workspaces/zmk-config/zmk-config/config" -DCONFIG_ZMK_STUDIO=y
mv build/left/zephyr/zmk.uf2 /workspaces/zmk-config/zmk-config
mv /workspaces/zmk-config/zmk-config/zmk.uf2 /workspaces/zmk-config/zmk-config/sofle-left.uf2
west build -s /workspaces/zmk/app -p -d build/right -b nice_nano_v2 -- -DSHIELD=sofle_right -DZMK_CONFIG="/workspaces/zmk-config/zmk-config/config"
mv build/right/zephyr/zmk.uf2 /workspaces/zmk-config/zmk-config
mv /workspaces/zmk-config/zmk-config/zmk.uf2 /workspaces/zmk-config/zmk-config/sofle-right.uf2