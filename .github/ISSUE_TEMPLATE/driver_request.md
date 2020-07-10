---
name: Driver request
about: Request Support for a new INDI driver
title: ''
labels: driver development
assignees: ''

---

**Did you make sure there is no existing driver for your device? Please check [supported devices portal](https://indilib.org/devices].**
Please confirm you verified no drivers exist for your device.

**Provide link to product page**
Provide URL to the product description and details.

**Describe device family**
All INDI devices fall under one or more of the following categories:
* Mounts
* Cameras (CCDs + CMOS)
* Focusers
* Filter Wheels
* Domes
* Weather Stations
* Adaptive Optics
* Receivers
* Spectrographs
* Auxiliary (Anything else that does not fit above).

**Provide device communication protocol**
All devices use some sort of communication language between them and the host system. This could be called *Command Set* or *SDK*. It is best to provide a link to a document or PDF with such details from the manufacutrer. It needs to include the following information:
* Communication Method: Serial, USB, TCP...etc. along with the connection parameters.
* Communication Protocol: Detailed description of the language used to communicate with the device along with examples if available.

**Hardware Sample**
Would it be possible to send the developer a sample hardware unit to develop the driver? If that's not possible, would it be possible to setup a remote environment with the devie attached?
