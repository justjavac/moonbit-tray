///|
fn main {
  println("platform: \{@tray.current_platform()}")
  println("default identifier: \{@tray.default_identifier()}")
  match @tray.ensure_supported() {
    Ok(_) => println("tray backend is available on this machine")
    Err(error) => println("tray backend is unavailable: \{error}")
  }
}
