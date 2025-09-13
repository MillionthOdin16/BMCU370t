# BMCU370 USB-ESP Interface
**Complete USB Communication Foundation with ESP32 Web Interface and Advanced Features**

BMCU星尘修改版最新（BMCU-C 370霍尔版 V0.1-0020）源码，原项目链接：[Xing-C/BMCU370x](https://github.com/Xing-C/BMCU370x)。有一些个人小优化。

BMCU Xing-C modified version latest (BMCU-C Hall V0.1-0020) source code. Includes some minor personal optimizations.

## 🚀 USB-ESP Interface Features

This repository now includes a complete USB communication foundation with ESP32 web interface:

### 🔌 BMCU370 USB Communication Foundation
- **USB CDC-ACM Interface**: Dual-mode operation preserving DFU firmware updates
- **JSON Status API**: Complete system status export with all sensor data
- **Command Protocol**: GET_STATUS, SET_PARAM, DFU mode switching
- **Pin Conflict Resolution**: PA11 USB/LED automatic channel management
- **Memory Optimized**: Clean build at 68.3% flash usage

### 🌐 ESP32 Web Interface
- **USB Host Communication**: Automatic BMCU370 device enumeration
- **Modern Web Dashboard**: Real-time monitoring with responsive design and clear status indicators for both the web interface and the BMCU.
- **WiFi Management**: AP mode setup and network configuration.
- **System Control**: Remote configuration, diagnostics, and firmware updates.

### 📊 Advanced Features
- **Historical Data Logging**: 24+ hour sensor data with trend analysis
- **OTA Updates**: Web-based firmware updates with progress tracking
- **Professional Architecture**: Production-ready with comprehensive error handling

## 🏗️ System Architecture
```
[Bambu Printer] ←RS485→ [BMCU370] ←USB-C→ [ESP32-S3] ←WiFi→ [Web Browser]
                          ↓                    ↓
                    [USB CDC-ACM]        [Historical Data]
                    [JSON Status API]    [OTA Updates]  
                    [Command Protocol]   [Trend Analysis]
                    [DFU Preservation]   [Data Export]
```

## 🛠️ Build Status
- **BMCU370**: 68.3% Flash, 63.6% RAM
- **ESP32**: 71.0% Flash, 15.9% RAM

## 📦 Firmware Builds
Firmware binaries are automatically built for both targets on every PR and push via GitHub Actions. Download artifacts from the Actions tab or releases.

## 🚀 Quick Start

### 1. Flash BMCU370 Firmware
```bash
# Enter DFU mode (hold BOOT button during power-on)
dfu-util -a 0 -D bmcu370_firmware.bin
```

### 2. Flash ESP32 Firmware
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash -z \
  0x0000 esp32_bootloader.bin \
  0x8000 esp32_partitions.bin \
  0x10000 esp32_firmware.bin \
  0x110000 esp32_littlefs.bin
```

### 3. Access Web Interface
1. Connect BMCU370 to ESP32 via USB-C
2. Power ESP32 → creates AP "BMCU370-Setup"
3. Connect to AP (password: bmcu370setup)
4. Configure WiFi at http://192.168.4.1
5. Access interface at http://esp32-bmcu370.local

## 📚 Documentation
- [ESP32 Implementation Guide](ESP32_WEB_INTERFACE_IMPLEMENTATION.md)
- [Integration Test Results](INTEGRATION_TEST_RESULTS.md)

# 链接
- english wiki: https://wiki.yuekai.fr/
- 中文wiki：https://bmcu.wanzii.cn/

# 更新日志
从群文件里拷贝来的

### 25-7月17日-0020；
修复灯光逻辑错误，导致一些状态不亮灯。
修复通道意外上线
修正防掉线，之前并未生效
重写灯光系统，修复了闪烁问题，降低了刷新频率。
当通道错误时，每隔3秒尝试更新一次红色，避免BMCU进入工作状态后插入的通道不亮灯。

### 25-7月6日-0019修改版；
双微动霍尔版本也可用。

首先 是0019原版对于0013原版的改变；
根据刷入固件不同可以让P1X1支持16色了
修复了P1X1打印机固件升级后（目前最新00.01.06.62），或切片软件最新版(目前2.1.1.52)下，无法保存耗材丝信息的问题。
修改了在线逻辑判断，防止某些状态下出现错误的通道在线。
修改了电机控制逻辑，在高低电压位使用不同调用。

然后 对于上个版本0013修改灯效防过热版本的变动；
主板灯光，未连接打印机时红色呼吸，正常工作时白色呼吸。
进一步降低缓冲灯光和主板灯光的亮度。
退料部分，抛弃对A1进行控制。