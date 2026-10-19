zephyr_library()
zephyr_library_sources(custom_driver/a320.c)
zephyr_library_sources(custom_driver/trackpad_led.c)
zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
