name = "justjavac/tray"

version = "0.1.5"

readme = "README.mbt.md"

repository = "https://github.com/justjavac/moonbit-tray"

license = "MIT"

keywords = [ "tray", "desktop", "native", "windows", "macos", "linux" ]

description = "Cross-platform native tray helpers for MoonBit."

preferred_target = "native"

options(
  source: "src",
  supported_targets: "+native",
)
