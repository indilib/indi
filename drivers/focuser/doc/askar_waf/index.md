# Askar WAF Focuser

[Askar](https://gb.sharpstar-optics.com/) (Jiaxing Ruixing Optical Instrument Co., Ltd.) manufactures the **Askar WAF**, a USB focuser for astronomy. The device presents a USB CDC virtual serial port and is controlled with logical step coordinates.

_Askar WAF_ is the INDI focuser driver for this device. It is part of the INDI core package and uses only the standard serial connection already available in INDI Core.

## Features

- Absolute and relative focus moves
- Abort / halt
- Sync logical position (no motor motion)
- Reverse motion direction
- Firmware backlash compensation
- Read and set maximum travel
- Firmware version and model name display

## Installation

The driver is included in the INDI core package and does not require extra dependencies.

Start the INDI server with the focuser driver:

```shell
indiserver -v indi_askar_waf_focus
```

## Configuration

1. Open the INDI Control Panel and select **Askar WAF**.
2. On the **Connection** tab, choose the USB CDC serial port (typically `/dev/ttyACM0` on Linux). Baud rate is required to open the port but is otherwise irrelevant for CDC.
3. Click **Connect**. On success, the driver reads firmware version, model name, max travel, current position, backlash, and reverse settings.

On the **Main Control** tab you can use:

| Control | Description |
| ------- | ----------- |
| Absolute Position | Move to a logical step position in `[0, Max Position]` |
| Relative Position | Move inward (−) or outward (+) by a number of steps |
| Abort | Stop motion immediately |
| Sync | Set the reported logical position without moving the motor |
| Reverse | Invert physical motor direction for all moves |
| Backlash | Firmware compensation steps (`0` disables compensation) |
| Max Position | Maximum logical travel (firmware range `100`–`1000000`) |
| Firmware | Read-only firmware version and model name |

## Usage & Tips

- All positions are **logical steps**. After power-up, the firmware restores the last saved logical position from NVS when available; otherwise it defaults near mid-travel. If the reported position does not match the mechanics, use **Sync** (for example sync to `0` when fully retracted).
- When changing **Max Position**, the current logical position must not exceed the new maximum, or the device rejects the command. A common setup is: fully retract → **Sync** to `0` → extend to the mechanical limit → set **Max Position**.
- **Reverse** only inverts motor rotation; logical coordinates stay the same. Use it when the focuser is mounted so that “outward” moves the tube the wrong way.
- Moves are asynchronous. The driver polls motion state and position until the focuser is idle.
- Enable the driver **Debug** control if you need to inspect the `F…#` command traffic on the serial port.
