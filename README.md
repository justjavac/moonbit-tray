# justjavac/tray

[![CI](https://github.com/justjavac/moonbit-tray/actions/workflows/ci.yml/badge.svg)](https://github.com/justjavac/moonbit-tray/actions/workflows/ci.yml)
[![coverage](https://img.shields.io/codecov/c/github/justjavac/moonbit-tray/main?label=coverage)](https://codecov.io/gh/justjavac/moonbit-tray)
[![linux](https://img.shields.io/codecov/c/github/justjavac/moonbit-tray/main?flag=linux&label=linux)](https://codecov.io/gh/justjavac/moonbit-tray)
[![macos](https://img.shields.io/codecov/c/github/justjavac/moonbit-tray/main?flag=macos&label=macos)](https://codecov.io/gh/justjavac/moonbit-tray)
[![windows](https://img.shields.io/codecov/c/github/justjavac/moonbit-tray/main?flag=windows&label=windows)](https://codecov.io/gh/justjavac/moonbit-tray)
[![Docs](https://img.shields.io/badge/docs-mooncakes.io-green)](https://mooncakes.io/docs/justjavac/tray)

Cross-platform native tray helpers for MoonBit.

This package focuses on a small, readable API for tray lifecycle management:
detect support, create a tray handle, update the icon or tooltip, show or hide
it, pump native events when needed, and destroy it cleanly.

## Install

```bash
moon add justjavac/tray
```

This package supports the `native` target only.

## What You Get

- A lightweight tray icon lifecycle API for MoonBit native apps
- Runtime support checks before you create a tray
- Cross-platform backends for Windows, Linux, and macOS
- No compile-time Linux or macOS GUI dependency in the package itself
- Coverage-aware CI and Codecov badges for Windows, Linux, and macOS

## Quick Start

```mbt nocheck
fn run_tray_demo() -> Unit {
  guard @tray.is_supported() else {
    println("tray backend unavailable: \{@tray.ensure_supported()}")
    return
  }

  let tray = match @tray.create(
    identifier="com.example.demo",
    tooltip="MoonBit tray demo",
  ) {
    Ok(tray) => tray
    Err(error) => {
      println("create failed: \{error}")
      return
    }
  }

  ignore(tray.set_icon(Some("assets/tray.png")))
  ignore(tray.show())

  loop {
    match tray.pump() {
      Ok(true) => ()
      Ok(false) => break
      Err(error) => {
        println("pump failed: \{error}")
        break
      }
    }
  }

  tray.destroy()
}
```

## API Summary

### Platform and capability helpers

- `current_platform() -> Platform`
- `default_identifier() -> String`
- `is_supported() -> Bool`
- `ensure_supported() -> Result[Unit, String]`

### Tray lifecycle

- `create(identifier? : String, icon? : String?, tooltip? : String) -> Result[Tray, String]`
- `Tray::show(tooltip? : String?) -> Result[Bool, String]`
- `Tray::hide() -> Result[Bool, String]`
- `Tray::set_tooltip(String) -> Result[Bool, String]`
- `Tray::set_icon(String?) -> Result[Bool, String]`
- `Tray::pump(blocking? : Bool) -> Result[Bool, String]`
- `Tray::destroy() -> Unit`

### Return conventions

- `show()` returns `Ok(true)` when the tray is visible after the call.
- `hide()` returns `Ok(false)` when the tray is hidden after the call.
- `set_tooltip()` and `set_icon()` return the current visible state.
- `pump()` returns `Ok(false)` only when the native loop asks the caller to stop.

## Platform Notes

| Platform | Backend | Notes |
| --- | --- | --- |
| Windows | Win32 notification area | Uses a hidden message window plus `Shell_NotifyIconW`. |
| Linux | GTK 3 + AppIndicator | GUI runtime is loaded dynamically at runtime. |
| macOS | AppKit `NSStatusItem` | AppKit is loaded through the Objective-C runtime. |

### Windows

- Works with the normal shell notification area.
- Uses UTF-8 to UTF-16 conversion internally for tooltips and icon paths.
- `pump()` processes the Win32 message queue and is safe to call in a regular loop.

### Linux

- Requires a desktop session with GTK 3 available.
- Requires either `libayatana-appindicator3` or `libappindicator3` at runtime.
- The package does not hard-link those libraries at build time; it probes them at runtime.
- Tooltip updates are mapped to the AppIndicator title because Linux tray APIs do not expose one consistent tooltip concept.

### macOS

- Uses `NSStatusBar` / `NSStatusItem`.
- Tray creation must happen on the main thread.
- If no icon path is supplied, the backend falls back to a simple text title.

## Event Loop Guidance

`pump()` exists so native tray backends that need loop progress can keep moving.

- On Windows, it advances the Win32 message queue.
- On Linux, it runs one GTK main-loop iteration.
- On macOS, it advances one AppKit event iteration.

For simple apps, a non-blocking loop is often enough:

```mbt nocheck
loop {
  match tray.pump() {
    Ok(true) => ()
    Ok(false) => break
    Err(error) => {
      println(error)
      break
    }
  }
}
```

If your app already owns a native GUI loop, call `pump()` only where it makes
sense for your host application.

## Current Scope

This package currently covers tray icon lifecycle management only:

- support detection
- creation
- icon updates
- tooltip updates
- visibility changes
- event pumping
- destruction

It does not yet expose menu items or click callbacks as part of the public
MoonBit API.

## Testing

```bash
moon test --target native
moon test --target native --enable-coverage
moon coverage analyze -p justjavac/tray -- -f summary
```

Optional native integration checks can be enabled locally:

```bash
$env:MOONBIT_TRAY_RUN_NATIVE_TESTS = "1"
moon test --target native --filter "native*"
```

## Coverage and Badges

CI uploads one coverage report per operating system:

- `linux`
- `macos`
- `windows`

Codecov then aggregates those uploads into the total badge shown at the top of
this README. Badge values update only after the relevant GitHub Actions run
finishes and Codecov processes the reports.

## Development

The repository follows the usual MoonBit flow:

```bash
moon check
moon test
moon info
moon fmt
```

`moon info` updates the generated public interface in
[`src/pkg.generated.mbti`](src/pkg.generated.mbti), which is useful for
reviewing intentional API changes.

## License

MIT. See [LICENSE](LICENSE).
