///|
pub enum Platform {
  Windows
  Linux
  MacOS
  Unknown
} derive(Show, Eq, ToJson)

///|
/// Returns the desktop platform detected by the native backend for the current
/// process.
///
/// The value is computed by the native stub so it matches the operating system
/// that actually builds and runs the package.
pub fn current_platform() -> Platform {
  platform_from_tag(current_platform_ffi())
}

///|
/// Returns the default identifier used by `create()` when callers do not
/// provide one.
///
/// The identifier is used as the native tray instance id on platforms that
/// require one, and keeping it stable makes logs and diagnostics easier to
/// follow.
pub fn default_identifier() -> String {
  "moonbit-tray"
}

///|
fn platform_from_tag(tag : Int) -> Platform {
  match tag {
    1 => Windows
    2 => Linux
    3 => MacOS
    _ => Unknown
  }
}

///|
fn support_error_message() -> String {
  decode_bytes(support_error_ffi()).unwrap_or(
    "tray is not supported on this platform",
  )
}

///|
fn normalize_identifier(identifier : String) -> String {
  let trimmed = identifier.trim().to_string()
  if trimmed.is_empty() {
    default_identifier()
  } else {
    trimmed
  }
}
