# EEZ Studio UI project

Open `OSSM-M5-Remote-Redux.eez-project` in EEZ Studio to edit the UI. The project uses LVGL 9.5 at 320 x 240 with 16-bit color and Flow disabled.

Use EEZ Studio's build command to regenerate the UI. Its output directory is configured as `../src/ui/generated`.

The generated C sources are committed so normal firmware builds do not require EEZ Studio. Do not edit files under `src/ui/generated` manually; make changes in the EEZ project and regenerate them instead.

Define event bindings and stable object names in the EEZ project. Native action implementations and their state belong in the handwritten code under `src/ui`, outside the generated directory.
