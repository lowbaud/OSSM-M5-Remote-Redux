# Native Unity tests

These tests run on the local machine.
PlatformIO supplies Unity through `test_framework = unity`.

## Running

Install PlatformIO and a native C++ compiler available on PATH (GCC on Linux,
Clang on macOS, or MinGW-w64 GCC on Windows). PlatformIO's ESP32 compiler does
not replace the native compiler.

From the repository root:

```sh
pio test -e native
```

To run a single suite, pass its path relative to `test/`:

```sh
pio test -e native -f native/test_<component>
```

These commands compile test executables.
The first run can download the native PlatformIO platform and Unity.

## Scope and adding tests

Add native suites under `test/native/test_<component>/`, each with a Unity
`main()` and `setUp()` / `tearDown()` hooks. Include any additional production
sources explicitly in the native environment's `build_src_filter`.

Shared fakes live in `test/support/`, a local library loaded only by the native
environment. Its `Preferences.h` replaces the ESP32 storage API for native tests.

The native environment is independent of the firmware settings so it does not
load ESP32 libraries, the application entry point, generated UI, or firmware
packaging scripts. The firmware remains the default build environment.

For controller tests, provide small fakes at hardware boundaries instead of
compiling the complete BLE/UI stack. ArduinoFake can be added when a tested
component needs supported Arduino calls. Device integration tests are not yet
configured.

## Python tooling tests

Python tests live in `test/python/`. Run them from the repository root:

```sh
python -m unittest discover -s test/python -p "test_*.py"
```
