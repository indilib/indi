# INDI Alpaca Server

The INDI Alpaca Server is a bridge that allows ASCOM Alpaca clients to connect to INDI devices. It provides an Alpaca REST API server that translates Alpaca requests to INDI commands and vice versa.

## Features

- Automatic discovery of INDI devices
- Support for telescope/mount devices (with more device types planned)
- Full implementation of the Alpaca REST API
- Configurable server settings

## Installation

### Dependencies

- INDI Library
- C++11 compatible compiler
- CMake
- httplib (included)

### Building

```bash
mkdir build
cd build
cmake ..
make
sudo make install
```

## Usage

Start the INDI server with your devices:

```bash
indiserver indi_simulator_telescope
```

Then start the Alpaca server:

```bash
indi_alpaca_server
```

By default, the Alpaca server listens on port 11111. You can configure this in the INDI control panel.

## Supported Devices

Currently, the following INDI device types are supported:

- Telescopes/Mounts

Support for the following device types is planned:

- Cameras
- Filter Wheels
- Focusers
- Domes

## Alpaca API

The server implements the ASCOM Alpaca API as defined at https://ascom-standards.org/api/

### Management API

- `/management/apiversions` - Return supported API versions
- `/management/v1/description` - Return server description
- `/management/v1/configureddevices` - Return list of configured devices

### Device API

- `/api/v1/{device_type}/{device_number}/{method}` - Device-specific methods

## License

This project is licensed under the GNU General Public License v2.0.
