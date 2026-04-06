# justjavac/tray

[![CI](https://github.com/justjavac/moonbit-tray/actions/workflows/ci.yml/badge.svg)](https://github.com/justjavac/moonbit-tray/actions/workflows/ci.yml)
[![coverage](https://img.shields.io/codecov/c/github/justjavac/moonbit-tray/main?label=coverage)](https://codecov.io/gh/justjavac/moonbit-tray)
[![linux](https://img.shields.io/codecov/c/github/justjavac/moonbit-tray/main?flag=linux&label=linux)](https://codecov.io/gh/justjavac/moonbit-tray)
[![macos](https://img.shields.io/codecov/c/github/justjavac/moonbit-tray/main?flag=macos&label=macos)](https://codecov.io/gh/justjavac/moonbit-tray)
[![windows](https://img.shields.io/codecov/c/github/justjavac/moonbit-tray/main?flag=windows&label=windows)](https://codecov.io/gh/justjavac/moonbit-tray)

Cross-platform native tray helpers for MoonBit.

## Example

```mbt nocheck
guard @tray.is_supported() else {
  return
}

let tray = @tray.create(
  identifier="com.example.demo",
  tooltip="MoonBit tray demo",
).unwrap()

ignore(tray.show())
ignore(tray.pump())
tray.destroy()
```
