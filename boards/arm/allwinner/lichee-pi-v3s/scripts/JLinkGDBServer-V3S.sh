# 启动Jlink 并链接 全志v3s 处理器

# arm-none-eabi-objdump

JLinkGDBServer -select USB -device Cortex-A7 -endian little -if JTAG -speed 4000  -ir -LocalhostOnly