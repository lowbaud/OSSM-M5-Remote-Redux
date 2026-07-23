---
layout: default
title: Docs
description: Documentation for M5 Remote Redux.
nav_key: docs
permalink: /docs/
---

# Documentation

OSSM M5 Remote Redux is an unofficial fork and rewrite of the user
interface/firmware from the
[original OSSM M5 Remote project](https://github.com/ortlof/OSSM-M5-Remote).
It supports both the Core2-based and CoreS3-based versions of the OSSM M5
Remote.

The goal is to provide a clean, reliable remote that is easy to use and
actively maintained for compatibility with the official OSSM firmware.
Reliability and predictable behavior come before adding more features.

Redux supports the official OSSM BLE firmware and
[OSSM Lite](https://github.com/fray-d), so no custom firmware needs to be
flashed to the machine. Other machine firmware may work but has not been
tested.

> **Work in progress:** Redux is pre-v1.0 software and may change significantly.
> Carefully test all controls with your specific remote and machine in a safe
> setup before relying on a new version.
{: .notice .notice--warning }

## Supported hardware

Redux supports both the M5Stack Core2-based and CoreS3-based versions of the
OSSM M5 Remote. The remote hardware is unchanged from the original project.

If you do not have an M5 remote yet, see the
[hardware build instructions]({{ site.repository_url }}#building-the-hardware)
for the required parts and assembly information.

## Install or update

The easiest installation method is the [Web Flasher]({{ '/flash/' | relative_url }}).
It requires a current desktop version of Firefox, Chrome, or Edge and a USB data
connection to the remote. The flasher detects the remote variant automatically.

Select the recommended 500 mA charging option unless the installed battery is
explicitly rated for a charge current of at least 1000 mA. Lower charge currents
can be set when building the firmware from source.

Flashing via the Web Flasher always resets all settings to their defaults, regardless
of whether **Erase device** is selected.


## Connect to an OSSM

1. Power on the OSSM and let it finish homing.
2. Power on the M5 remote.
3. Select **Scan** on the welcome screen.
4. Select the discovered OSSM and choose **Connect**.

## Controls

Once connected, increasing the speed above zero starts motion. Begin with a
low speed. Press the center MX button to stop motion.

| Control | Function |
| --- | --- |
| Left encoder | Adjusts speed; press to enter the Settings screen. |
| Left middle encoder | Adjusts stroke. |
| Right middle encoder | Adjusts depth. |
| Right encoder | Adjusts sensation; press to select a pattern. |
| Center MX button | Stops motion. |

The **Stroke direction** setting controls how the left middle encoder adjusts stroke. By default,
turning it right decreases the stroke, matching the slider motion and machine travel.

To change the active pattern, press the right encoder, turn it to select a
pattern, and press it again to confirm.

## Build from source

Developers can upload directly with PlatformIO instead of using the Web
Flasher. See the [repository build instructions]({{ site.repository_url }}#flashing-the-firmware)
for the supported environments and upload commands.
