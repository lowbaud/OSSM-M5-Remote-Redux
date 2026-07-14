# Repository Instructions

## Builds

Do not automatically run `pio run` or any other build or compilation command.
Only attempt a build when the user explicitly asks for one.

## EEZ Studio UI

`eez-ui/` and `src/ui/generated/` are user-owned. Do not modify, move, delete,
or regenerate files in either directory.

Read-only inspection is allowed for integrating handwritten code. If changes
are required, describe them; the user will update the EEZ Studio project and
regenerate the UI.

## C/C++ Formatting

All changed handwritten C/C++ files must be clang-format 22 compliant using
the repository-root `.clang-format`. Format only the changed handwritten files
before completing the task. If no compatible formatter is available, report
that instead of installing tooling without permission.

Do not format or modify `eez-ui/`, `src/ui/generated/`, or `src/lv_conf.h`.
