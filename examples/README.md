# Examples

This directory contains standalone MoonBit example programs for `justjavac/tray`.

Run them from this directory:

```bash
moon run check_support
moon run basic_lifecycle
moon run application_loop
moon run custom_icon
moon run custom_icon -- path/to/icon.png "My tray tooltip"
```

Included programs:

- `check_support`: probe the current platform and print the support result
- `basic_lifecycle`: create a tray, show it, hide it, and destroy it cleanly
- `application_loop`: demonstrate how to pump the tray backend from an app loop
- `custom_icon`: pass a custom icon path and optional tooltip from the command line
