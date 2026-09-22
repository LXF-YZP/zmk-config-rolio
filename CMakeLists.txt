cmake_minimum_required(VERSION 3.20.0)

# ads1220-zephyr-module uses DEVICE_API, which was added after the Zephyr
# release used by ZMK v0.2. Keep the compatibility definition local to this
# configuration module so the dependency can still build on Zephyr 3.5.
if (NOT DEFINED ZEPHYR_VERSION_CODE OR ZEPHYR_VERSION_CODE LESS 262144)
  zephyr_compile_options(-include "${CMAKE_CURRENT_LIST_DIR}/include/rolio/zephyr_compat.h")
endif()
