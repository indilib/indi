# 🧩 INDI Driver Contribution Guide

Thank you for your interest in contributing a driver to the INDI Library!  
To maintain a high-quality and user-friendly experience, we ask all driver authors —especially device manufacturers— to follow the structured process below.

---

## ✅ Contribution Checklist

Before submitting your driver, please ensure:

- ✅ The driver builds and runs successfully
- ✅ It works with real hardware (or includes a simulation mode)
- ✅ The driver is placed in the correct directory structure
- ✅ Documentation is included in a `/doc` directory
- ✅ Licensing and coding guidelines are followed

---

## 🔀 Driver Submission Guidelines: INDI Core vs. INDI Third-Party

When deciding where to submit a driver (INDI Core or INDI Third-Party), use the following rule:

- INDI Core: If the driver can be built and run using only the dependencies already available in INDI Core, it should be included in the INDI Core repository.

- INDI Third-Party: If the driver requires any external dependencies that are not part of INDI Core, it must be submitted as a third-party driver.

This ensures that INDI Core remains lightweight and easy to build, while still allowing flexibility for more complex drivers through the third-party ecosystem.

## 📁 Directory Structure

Choose the appropriate repository and structure for your driver:

### 📦 Core Driver
Add under: [`indi/drivers/`](https://github.com/indilib/indi/tree/master/drivers)

### 🔌 Third-Party Driver
Add under: [`indi-3rdparty/`](https://github.com/indilib/indi-3rdparty/)

**Example structure:**

    indi-3rdparty/
    └── drivers/
        └── yourdevice/
            ├── CMakeLists.txt
            ├── yourdevice.cpp
            ├── yourdevice.h
            ├── yourdevice.xml
            └── doc/
                └── index.md

    indi/
    └── drivers/
        └── category/
            ├── CMakeLists.txt
            ├── yourdevice.cpp
            ├── yourdevice.h
            ├── ...
            └── doc/
                └── yourdevice
                    └── photo.webp
                    └── index.md
---

## 📄 Documentation Requirements

Your driver **must** include a `/doc/index.md` file to help users install and use the driver effectively.

### Suggested Structure for `index.md`:

#### 1. 📌 Device Overview
- Brief description of the device
- Manufacturer, model, and supported functions

#### 2. ✨ Features
- List of implemented features (e.g., autofocus, temperature reporting)

#### 3. ⚙️ Installation
- Dependencies and build steps (if not standard)
- UDEV rules (if applicable)

#### 4. 🔧 Configuration
- Connection types (USB, Serial, TCP/IP)
- INDI Control Panel setup

#### 5. 🧪 Usage & Tips
- Common pitfalls
- Testing procedures
- Logging tips

#### 6. 🖼 Screenshots (Optional but Recommended)
- Driver interface
- Logs or sample output

📚 **Example**: [indi-duino/doc](https://github.com/indilib/indi-3rdparty/tree/master/indi-duino/doc)

---

## 🛠 CMake Integration

Your `CMakeLists.txt` should:

- Include the driver source files
- Respect standard INDI CMake variables (`CMAKE_INSTALL_PREFIX`, etc.)
- Optionally install docs (if applicable)

---

## 🧾 Licensing

Drivers must be released under a compatible open-source license, typically **GPLv2 or later**.  
Please add a license header to all source files.

---

## 🚀 How to Submit

1. Fork the appropriate INDI repository
2. Create a new branch (e.g. `driver-yourdevice`)
3. Add your driver code and documentation
4. Submit a Pull Request with a clear title and description

---

## 💬 Support

Need help?

- 🗨 [INDI Forum](https://indilib.org/forum.html)
- 🐛 Open a GitHub Issue
- 📫 Contact a maintainer

---

## 💡 For Manufacturers

High-quality documentation in `/doc/index.md` makes your hardware easier to adopt and reduces your support burden.  
Consider adding:

- Diagrams
- Connection examples
- Firmware version notes
- FAQs

---

Thank you for contributing to the INDI community!
