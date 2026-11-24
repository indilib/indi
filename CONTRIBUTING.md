# 🧩 INDI Driver Contribution Guide

Thank you for your interest in contributing a driver to the INDI Library!  
To maintain a high-quality and user-friendly experience, we ask all driver authors —especially device manufacturers— to follow the structured process below.

---

## ✅ Contribution Checklist

Before submitting your driver, please ensure:

- ✅ No existing driver already supports the device (see below)
- ✅ The driver builds and runs successfully
- ✅ It was tested on the real hardware, and the pull request states the device model, firmware version and what was tested
- ✅ Automated tests cover the logic that can run without the device
- ✅ The driver is placed in the correct directory structure
- ✅ The driver version is set in the driver (`setVersion`) and matches its `drivers.xml` entry
- ✅ Documentation is submitted to the [drivers documentation repository](https://github.com/indilib/drivers-docs)
- ✅ Licensing and coding guidelines are followed

The testing and submission requirements are described in detail in [AGENTS.md](AGENTS.md) (sections 5 to 7). They apply to every submission, whether or not it was written with an AI coding agent.

---

## 🔎 Is a New Driver Needed?

Before writing a new driver, check that the device is not already supported. Search [`drivers.xml`](drivers.xml), the [indi-3rdparty](https://github.com/indilib/indi-3rdparty/) repository and [drivers.indilib.org](https://drivers.indilib.org) by manufacturer, model and protocol. Many devices are supported under another name, for example by the LX200, OnStep or MoonLite drivers.

If an existing driver supports the device, or could with a fix or an extension, please improve that driver instead of writing a new one. Two drivers for the same hardware confuse users and split bug fixes, documentation and maintenance. A second driver for an already supported device is only accepted for a very good reason, such as an incompatible protocol, and the pull request must explain why the existing driver could not be used or extended. If in doubt, ask first on the [INDI Forum](https://indilib.org/forum.html) or in a GitHub issue.

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
            └── yourdevice.xml

    indi/
    └── drivers/
        └── category/
            ├── CMakeLists.txt
            ├── yourdevice.cpp
            ├── yourdevice.h
            └── ...

---

## 📄 Documentation Requirements

Your driver **must** come with user documentation to help users install and use the driver effectively.

Documentation for all drivers, Core and Third-Party, lives in a separate repository, [indilib/drivers-docs](https://github.com/indilib/drivers-docs), which is published at [drivers.indilib.org](https://drivers.indilib.org). Submit a pull request there and link it in your driver pull request. Do not add documentation files to the driver directory.

Each driver has its own directory there, `src/content/docs/<category>/<manufacturer>/<driver>/`, with a Markdown page (`<driver>.md`), a metadata file (`<driver>.yaml`) and a 300x300 thumbnail (`<driver>.webp`). The [README of that repository](https://github.com/indilib/drivers-docs#readme) explains the layout, the image rules and how to preview the page locally.

A documentation update is also required when a change to an existing driver makes its page wrong or incomplete, for example a new or renamed property, a changed default, a new connection type or a newly supported model.

### Suggested Structure for the page:

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

📚 **Example**: [EQMod Mount](https://drivers.indilib.org/mounts/sky-watcher/eqmod)

---

## 🛠 CMake Integration

Your `CMakeLists.txt` should:

- Include the driver source files
- Respect standard INDI CMake variables (`CMAKE_INSTALL_PREFIX`, etc.)

---

## 🧾 Licensing

Drivers must be released under a compatible open-source license, typically **GPLv2 or later**.  
Please add a license header to all source files.

---

## 🚀 How to Submit

1. Fork the appropriate INDI repository
2. Create a new branch (e.g. `driver-yourdevice`)
3. Add your driver code and tests
4. Test the driver on the real hardware
5. Submit the documentation as a Pull Request to [drivers-docs](https://github.com/indilib/drivers-docs)
6. Submit a Pull Request with a clear title and description, the hardware test results and a link to the documentation Pull Request

---

## 💬 Support

Need help?

- 🗨 [INDI Forum](https://indilib.org/forum.html)
- 🐛 Open a GitHub Issue
- 📫 Contact a maintainer

---

## 💡 For Manufacturers

High-quality documentation makes your hardware easier to adopt and reduces your support burden.  
Consider adding:

- Diagrams
- Connection examples
- Firmware version notes
- FAQs

---

Thank you for contributing to the INDI community!
