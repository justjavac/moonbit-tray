# justjavac/tray

[![CI](https://github.com/justjavac/moonbit-tray/actions/workflows/ci.yml/badge.svg)](https://github.com/justjavac/moonbit-tray/actions/workflows/ci.yml)
[![coverage](https://img.shields.io/codecov/c/github/justjavac/moonbit-tray/main?label=coverage)](https://codecov.io/gh/justjavac/moonbit-tray)
[![Docs](https://img.shields.io/badge/docs-mooncakes.io-green)](https://mooncakes.io/docs/justjavac/tray)

Cross-platform native tray helpers for MoonBit.

## Install

```bash
moon add justjavac/tray
```

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

## API

- `current_platform()`
- `is_supported()` / `ensure_supported()`
- `create(...)`
- `show()` / `hide()`
- `set_tooltip(...)`
- `set_icon(...)`
- `pump(...)`
- `destroy()`
