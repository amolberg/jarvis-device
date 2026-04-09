# Contributing to JARVIS Device

Thank you for contributing to the JARVIS device project! 🎉

## How to Contribute

### Bug Reports

Found a bug? Please open an issue with:
- Steps to reproduce
- Expected vs actual behavior
- Serial logs (for firmware issues)
- Your hardware variant and firmware version

### Feature Requests

Have an idea? Open a feature request and describe:
- The problem you're solving
- Your proposed solution
- Any alternatives you've considered

### Pull Requests

1. **Fork** the repository
2. **Create a branch**: `git checkout -b feature/your-feature-name`
3. **Make your changes** — add tests if applicable
4. **Test** your changes:
   ```bash
   cd firmware
   idf.py build          # Build firmware
   idf.py flash monitor  # Flash and monitor
   ```
5. **Commit**: `git commit -m "feat: add your feature description"`
6. **Push**: `git push origin feature/your-feature-name`
7. **Open a Pull Request** against `main`

### Code Style

- **C/C++**: Follow [ESP-IDF style guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/style.html)
  - 4-space indentation
  - Braces on same line
  - Max line length: 120
- **Python**: PEP 8
- **Markdown**: Wrap at 100 characters

### Firmware Development

```bash
# Install ESP-IDF
git clone --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf
./install.sh esp32s3
source ~/esp/esp-idf/export.sh

# Build
cd firmware
idf.py set-target esp32s3
idf.py build

# Flash
idf.py -p /dev/ttyUSB0 flash monitor
```

### Hardware Design

- **3D Case**: OpenSCAD parametric design in `hardware/case/`
- **PCB**: KiCad project in `hardware/pcb/`
- **BOM**: Supplier links and pricing in `bom/bom.csv`

### Documentation

- Assembly guide: `docs/ASSEMBLY.md`
- Wiring diagrams: `hardware/wiring-diagram.svg`
- Firmware API: see `firmware/README.md`

## Project Structure

```
jarvis-device/
├── firmware/          # ESP32-S3 C++ firmware (ESP-IDF)
│   ├── src/          # Source files
│   ├── include/      # Header files
│   ├── platformio.ini # PlatformIO build config
│   └── CMakeLists.txt # ESP-IDF build config
├── hardware/         # Hardware designs
│   ├── case/         # 3D printable OpenSCAD case
│   ├── pcb/          # KiCad PCB designs
│   └── wiring-diagram.svg # Connection diagram
├── bom/             # Bill of materials
├── docs/            # Documentation
│   └── ASSEMBLY.md  # Step-by-step build guide
├── .github/         # GitHub config (issue templates)
└── CONTRIBUTING.md  # This file
```

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
