# INDI Alpaca Server

The INDI Alpaca Server is a bridge that allows ASCOM Alpaca clients to connect to INDI devices. It provides an Alpaca REST API server that translates Alpaca requests to INDI commands and vice versa.

## Features

- Automatic discovery of INDI devices
- Support for telescope/mount devices (with more device types planned)
- Full implementation of the Alpaca REST API
- ASCOM Alpaca Discovery Protocol support for network device discovery
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

The Alpaca server will start automatically when the driver is connected, after a configurable startup delay (default: 3 seconds). This delay allows time for the INDI server to start. Each INDI device will be made available through Alpaca as soon as it is fully initialized and its driver interface is defined. By default, the server listens on port 11111, and the discovery service listens on port 32227. You can configure these settings in the INDI control panel under the Options tab.

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

## Alpaca Discovery Protocol

The server implements the ASCOM Alpaca Discovery Protocol as defined at https://github.com/DanielVanNoord/AlpacaDiscoveryTests. This allows Alpaca clients to discover the server on the local network without knowing its IP address or port.

The discovery protocol works as follows:

1. Clients broadcast a UDP message containing "alpacadiscovery" to port 32227
2. The server responds with a JSON message containing the Alpaca API port: `{"AlpacaPort": 11111}`
3. Clients can then connect to the server using the IP address from the response and the provided port

The discovery protocol supports both IPv4 broadcast and IPv6 multicast for maximum compatibility.

### Management API

- `/management/apiversions` - Return supported API versions
- `/management/v1/description` - Return server description
- `/management/v1/configureddevices` - Return list of configured devices

### Device API

- `/api/v1/{device_type}/{device_number}/{method}` - Device-specific methods

## License

This project is licensed under the GNU General Public License v2.0.
