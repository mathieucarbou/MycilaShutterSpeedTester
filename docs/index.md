# MycilaShutterSpeedTester

ESP32 firmware to mesure camera shutter speeds

[![](https://mathieu.carbou.me/MycilaShutterSpeedTester/screenshot.png)](https://mathieu.carbou.me/MycilaShutterSpeedTester/screenshot.png)

[![Latest Release](https://img.shields.io/github/release/mathieucarbou/MycilaShutterSpeedTester.svg)](https://GitHub.com/mathieucarbou/MycilaShutterSpeedTester/releases/)
[![GPLv3 license](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0.txt)
[![Contributor Covenant](https://img.shields.io/badge/Contributor%20Covenant-2.1-4baaaa.svg)](code_of_conduct.md)
[![Build](https://github.com/mathieucarbou/MycilaShutterSpeedTester/actions/workflows/ci.yml/badge.svg)](https://github.com/mathieucarbou/MycilaShutterSpeedTester/actions/workflows/ci.yml)
[![GitHub latest commit](https://badgen.net/github/last-commit/mathieucarbou/MycilaShutterSpeedTester)](https://GitHub.com/mathieucarbou/MycilaShutterSpeedTester/commit/)

Table of contents

- 📥 [Downloads](#downloads)
- ⚙️ [Installation](#installation)
- 🔌 [Wiring](#wiring)
- 👀 [Usage](#usage)

## Downloads

Releases (firmware binaries) are available in the GitHub releases page:

[https://github.com/mathieucarbou/MycilaShutterSpeedTester/releases](https://github.com/mathieucarbou/MycilaShutterSpeedTester/releases)

Naming convention:

- `MycilaShutterSpeedTester-BOARD.FACTORY.bin` — first-time / factory flash image
- `MycilaShutterSpeedTester-BOARD.OTA.bin` — OTA update image

## Installation

First-time flash (factory image):

```bash
esptool.py erase_flash
esptool.py write_flash 0x0 MycilaShutterSpeedTester-BOARD.FACTORY.bin
```

## Wiring

## Usage

## License

This project is licensed under the GNU General Public License v3.0. See the [LICENSE](LICENSE) file for details.
