# OSSM M5 Remote Redux

OSSM M5 Remote Redux is an unofficial fork/rewrite of the user interface and
firmware from the [original OSSM M5 Remote project](https://github.com/ortlof/OSSM-M5-Remote).
It supports the same M5Stack Core2 and CoreS3 remote hardware, but replaces significant parts of
the firmware and user interface.

The goal of this project is to provide a clean, reliable remote that is easy to use and actively
maintained for compatibility with the official OSSM firmware. Reliability and predictable behavior
come before adding more features.

**Redux supports the official OSSM BLE firmware, so you no longer need to flash
custom firmware to the machine.**

> [!WARNING]
> **Work in progress:** This is pre-v1.0 code and may still change significantly. Carefully
> test all controls and functionality with your specific remote and machine in a safe manner
> before relying on a new version.

This project is not affiliated with, endorsed by, or maintained by the original
OSSM M5 Remote project, the OSSM project, or their authors/maintainers. It remains licensed
under CC BY-SA 4.0; see [LICENCE](LICENCE.txt).

## Building the hardware

Instructions for building the hardware are identical to the original M5 Remote and can be found
further down on this page or in the [original repo](https://github.com/ortlof/OSSM-M5-Remote).

## Flashing the firmware

The easiest way to install or update the firmware is with the
[web flasher](https://lowbaud.github.io/ossm-m5-remote-redux/web-flasher/). Connect the remote via
USB, open the flasher in a browser that supports Web Serial (such as Chrome or Edge), select the
battery charge current, and click **Install firmware**. The flasher detects Core2 and CoreS3
hardware automatically.

Use the recommended 500 mA charge current unless the installed battery is explicitly rated for
charging at 1000 mA. Flashing resets the remote's saved settings.

To upload the firmware from source instead:

1. Install Visual Studio Code and the PlatformIO extension.
2. Open this repository in VS Code.
3. Adjust the charge current if necessary for your battery (see development notes below).
4. Connect the remote via USB.
5. Select the correct PlatformIO environment for your device.
6. Click **Upload** in PlatformIO.

Alternatively, from the command line:

For CoreS3:

`pio run -e m5stack-cores3 -t upload`

For Core2:

`pio run -e m5stack-core2 -t upload`

## Using the remote

1. Power on the OSSM and let it home.
2. Power on the M5 Remote.
3. Click **Scan** on the Welcome screen.
4. Select the discovered OSSM and click **Connect**.
5. Set the speed to a non-zero value using the left encoder.

The last used connection is stored as the default. Auto-connect is enabled by default and can be
disabled in Settings. Press the left encoder to cancel connecting, or hold the left encoder while
turning on the remote to skip the automatic attempt.

The left encoder adjusts speed, the right encoder adjusts sensation, and the two middle encoders
adjust stroke and depth.

At the bottom of the screen, three soft buttons show the available click actions for the left
encoder, the right encoder, and the center MX button.

To change the active pattern, click the right encoder, turn it to select a pattern, then click it
again to confirm.

Settings can be reached from the welcome screen or the control screen by clicking the left
encoder.

The **Stop** button remains active at all times while connected, including in menus.

## Development notes

Direct PlatformIO builds use the recommended 500 mA charge current by default. This is set via
`default_charge_current_ma` in platformio.ini. The standard remote build uses an external
battery; retaining a small stock M5 battery is not a supported configuration.

The 500 mA setting is compatible with the external batteries documented by this project when
charged at room temperature. Use 1000 mA fast charging only when the installed battery is
explicitly rated for a charge current of at least 1000 mA, such as the EEMB LP103454. Always
verify the battery's polarity and charging specification before connecting it.

For development, you can override the charge current locally with the
`OSSM_CHARGE_CURRENT_MA` environment variable without changing tracked project configuration:

```bash
export OSSM_CHARGE_CURRENT_MA=1000
pio run -e m5stack-cores3
```

Release builds produce 500 mA and 1000 mA variants explicitly and do not rely on this local
default.

## Original project documentation

> The hardware and assembly information below still applies. References to the
> original firmware, its setup, or its behavior are preserved for context and
> may not apply to Redux.

## Overview of the OSSM-M5-Remote

A Remote Control Platform for all ESP Controlled Sex Toys,with a focus on the [OSSM](https://github.com/KinkyMakers/OSSM-hardware) and related toys.

![Final Addon](images/remote.png?raw=true "Remote" )

Intially developed for the OSSM Project:
https://github.com/KinkyMakers/OSSM-hardware

To help with development and design join the KinkyMakers Discord: https://discord.gg/MmpT9xE . Be sure to say hello in the #m5-remote channel.

## ~~!!! It does not Work with the Official OSSM Firmware !!!~~
~~This Branch is needed: https://github.com/ortlof/OSSM-Stroke~~

~~The Cable remote is obsolete with this firmware. If you want to go back you need to flash the OSSM orginal firmware back.~~

~~You can Easy flash the OSSM Mainboard with this [Online Flasher](https://openlust.org/lust-remote/lustremote-firmware/).~~


## Supported in this version of the M5 remote:
| OSSM Machine | https://github.com/KinkyMakers/OSSM-hardware |

## Not Supported sex toys in this version

| EJECT | https://github.com/ortlof/EJECT-cum-tube-project | [A Work in Progress]



# Build the OSSM M5 Remote Yourself

### Assembly instructions: [Klick Here !](Assembly.md)

## Bill Of Materials for sourcing Electrical Components

All M5Stack Core2 and CoreS3 are supported Now.

BOM is on Octopart for Easy Sourcing: https://octopart.com/bom-tool/rURYMuwB

PCB Files are located in the [Hardware/PCB](Hardware/PCB/) folder if you want to make one yourself or use a different manufacturer other than PCBWay.

## Additional parts needed that are not PCB:

| Quantity | Part | Sourcing EU | Price € |
|----------|------|-------------|---------|
| 1x | M5Stack CoreS3 SE | https://www.digikey.de/de/products/detail/m5stack-technology-co-ltd/K128-SE/23628221?s=N4IgTCBcDaILIFYDKAXAhgYwNYAIDCA9gE4CmSAzDkgKIgC6AvkA | 43 € |
| 2x | M3x25mm Hex Head Cap Bolt | https://www.amazon.de/Edelstahl-Innensechskant-Bolzenset-Eisenrahmen-Mechanischer-Innensechskantschraube-Mutternset/dp/B07PPFT871/ | 12,97 € |
| 4x | M3x20mm Hex Head Cap Bolt | Comes as part of the set mentioned above | " |
| 4x | Heat Set inserts M3 | https://www.amazon.de/ruthex-Gewindeeinsatz-St%C3%BCck-Gewindebuchsen-Kunststoffteile/dp/B08BCRZZS3 | 8,99 € |
| 1x | 3,7v 2000mAh Lipo Batterie Size 34,5 mm x 10,6 mm x 56 mm | https://www.amazon.de/EEMB-103454-2AhLithium-Schutzplatine-Isolationsbeschichtung/dp/B08214DJLJ/ | 14,89 € |
| 4x | Encoder Knob Bought or 3D Printed | https://de.aliexpress.com/item/1005001394286414.html | 5 € |
| 1x | OSSM M5 Remote PCB | KinyMaker Discord #M5-Remote Channel or https://www.pcbway.com/project/shareproject/M5Stack_Core2_Remote_Plattform_2cb5bac0.html | 15 € |

--------------------------------------------

| Quantity | Part | Sourcing US | Price $ |
|----------|------|-------------|---------|
| 1x | M5Stack CoreS3 SE | https://www.digikey.de/de/products/detail/m5stack-technology-co-ltd/K128-SE/23628221?s=N4IgTCBcDaILIFYDKAXAhgYwNYAIDCA9gE4CmSAzDkgKIgC6AvkA | $47|
| 2x | M3x25mm Hex Head Cap Bolt | https://www.amazon.com/dp/B09NR8X2LV | $17.99 |
| 4x | M3x20mm Hex Head Cap Bolt | Comes as part of the set mentioned above | " |
| 4x | Heat Set inserts M3 | https://www.amazon.com/ruthex-M3-Threaded-Inserts-RX-M3x5-7/dp/B08BCRZZS3 | $10.99 |
| 1x | 3.7v 2000mAh Lipo Battery Size 34.5 X 56 X 10.6 mm (The wires will need to be reversed in the connector on this one! See Assembly instructions for more info.) | https://www.amazon.com/EEMB-2000mAh-Battery-Rechargeable-Connector/dp/B08214DJLJ/ | $14.99 |
| 4x | Encoder Knob Bought or 3D Printed | https://www.aliexpress.us/item/3256801207971662.html?gatewayAdapt=deu2usa4itemAdapt | $5.00 |
| 1x | M5 Remote PCB | KinyMaker Discord #M5-Remote Channel or https://www.pcbway.com/project/shareproject/M5Stack_Core2_Remote_Plattform_2cb5bac0.html | $30.00 |

## 3D Printed Parts Needed:

| Quantity | Part | Information |
|----------|------|-------------|
| 1x | M5_curved_w6mm+5.stl Thanks to "Hoodlatch" KM Discord | There is a specific version for the wider Adafruit LIPO battery. Print with the base side facing down, 6 walls 20% Infill |
| 1x | TOP-*-Keycap-Standoff.stl | Top Depends on your Keycap: Cherry or DSA (DSA is wider) |
| 4x | M5_Remote_Knob_Customizable.scad | If you go for the 3d Printed knobs |

Filament - A good quality PLA works well. While there are no threads it is recommended that your printer is well calibrated.

## Operation, or how do I use it?

1. Power on the OSSM and let it home.
2. Power on the OSSM M5 Remote.
3. Press the left encoder to select 'Connect'
4. You can verify it is connected by looking in the top left corner, it should say 'connected'.
5. You can now begin use. You'll need to set the speed, depth and stroke to more than 0 and press the middle key to start. Start the speed out slow.
