#!/bin/bash
# ESP-IDF 4.3 Environment Setup for ESP32 Doom
# Source this file before building: source setup-env.sh

export IDF_PATH=/opt/esp-idf
export PATH="$HOME/.espressif/tools/xtensa-esp32-elf/esp-2020r3-8.4.0/xtensa-esp32-elf/bin:$HOME/.espressif/python_env/idf4.3_py3.11_env/bin:/opt/esp-idf/tools:$PATH"
export IDF_PYTHON_ENV_PATH="$HOME/.espressif/python_env/idf4.3_py3.11_env"

echo "ESP-IDF 4.3 environment configured:"
echo "  IDF_PATH: $IDF_PATH"
echo "  Toolchain: xtensa-esp32-elf-gcc 8.4.0 (esp-2020r3)"
echo "  Python: $IDF_PYTHON_ENV_PATH"
echo ""
echo "Common commands:"
echo "  make -j4          # Build firmware"
echo "  make flash        # Flash to device"
echo "  make monitor      # Serial monitor"
echo "  make menuconfig   # Configuration menu"
