///|
/// Represents a system tray handle created by this package.
///
/// A `Tray` tracks the user-visible state that the MoonBit layer believes is
/// active, including whether the tray is currently visible, which tooltip is
/// being shown, and which icon path was last requested. The underlying native
/// resources are released by calling `destroy()`.
pub struct Tray {
  mut handle : Int64
  mut native : Bool
  platform : Platform
  identifier : String
  mut icon : String?
  mut tooltip : String
  mut visible : Bool
  mut destroyed : Bool
}

///|
/// Returns whether the native backend can create tray instances on the current
/// machine.
///
/// On Windows this is expected to be `true`. On other platforms, or when the
/// required desktop runtime is unavailable, this returns `false`.
pub fn is_supported() -> Bool {
  is_supported_ffi()
}

///|
/// Validates that the native backend is available before any tray is created.
///
/// Use this when an application wants to show an actionable startup error
/// instead of deferring the failure until `create()`.
pub fn ensure_supported() -> Result[Unit, String] {
  if is_supported() {
    Ok(())
  } else {
    Err(support_error_message())
  }
}

///|
/// Creates a tray handle with an optional icon path and initial tooltip.
///
/// - `identifier` should be a stable, non-empty id for the tray instance.
/// - `icon` may be `None` to request the platform default tray icon.
/// - `tooltip` becomes the initial hover text when the platform supports it.
///
/// The returned handle starts hidden, so callers can finish any last setup and
/// then call `show()`.
pub fn create(
  identifier? : String = default_identifier(),
  icon? : String? = None,
  tooltip? : String = "",
) -> Result[Tray, String] {
  match ensure_supported() {
    Err(error) => Err(error)
    Ok(_) => {
      let normalized_identifier = normalize_identifier(identifier)
      let handle = create_state_ffi(
        encode_text(normalized_identifier),
        encode_optional_text(icon),
        encode_text(tooltip),
      )
      if handle == zero_handle() {
        Err(
          decode_bytes(last_create_error_ffi()).unwrap_or("tray.create failed"),
        )
      } else {
        Ok(Tray::{
          handle,
          native: true,
          platform: current_platform(),
          identifier: normalized_identifier,
          icon,
          tooltip,
          visible: false,
          destroyed: false,
        })
      }
    }
  }
}

///|
/// Shows the tray icon and optionally replaces the tooltip in the same call.
///
/// Passing `tooltip=Some(...)` is the most efficient way to update the tooltip
/// immediately before the tray becomes visible.
pub fn Tray::show(
  self : Tray,
  tooltip? : String? = None,
) -> Result[Bool, String] {
  match self.ensure_alive() {
    Err(error) => Err(error)
    Ok(_) => {
      let next_tooltip = tooltip.unwrap_or(self.tooltip)
      if !self.native {
        self.tooltip = next_tooltip
        self.visible = true
        Ok(true)
      } else if show_state_ffi(self.handle, encode_text(next_tooltip)) {
        self.tooltip = next_tooltip
        self.visible = true
        Ok(true)
      } else {
        Err(self.state_error(self.handle, "tray.show failed"))
      }
    }
  }
}

///|
/// Hides the tray icon while keeping the handle valid for later `show()` calls.
pub fn Tray::hide(self : Tray) -> Result[Bool, String] {
  match self.ensure_alive() {
    Err(error) => Err(error)
    Ok(_) =>
      if !self.native {
        self.visible = false
        Ok(false)
      } else if hide_state_ffi(self.handle) {
        self.visible = false
        Ok(false)
      } else {
        Err(self.state_error(self.handle, "tray.hide failed"))
      }
  }
}

///|
/// Replaces the current tooltip text without changing visibility.
///
/// Platforms that cannot show a real tooltip may map this value to the nearest
/// native concept available to the host desktop environment.
pub fn Tray::set_tooltip(self : Tray, tooltip : String) -> Result[Bool, String] {
  match self.ensure_alive() {
    Err(error) => Err(error)
    Ok(_) =>
      if !self.native {
        self.tooltip = tooltip
        Ok(self.visible)
      } else if set_tooltip_ffi(self.handle, encode_text(tooltip)) {
        self.tooltip = tooltip
        Ok(self.visible)
      } else {
        Err(self.state_error(self.handle, "tray.set_tooltip failed"))
      }
  }
}

///|
/// Changes the tray icon path or resets it to the platform default when `None`
/// is passed.
pub fn Tray::set_icon(self : Tray, icon : String?) -> Result[Bool, String] {
  match self.ensure_alive() {
    Err(error) => Err(error)
    Ok(_) =>
      if !self.native {
        self.icon = icon
        Ok(self.visible)
      } else if set_icon_ffi(self.handle, encode_optional_text(icon)) {
        self.icon = icon
        Ok(self.visible)
      } else {
        Err(self.state_error(self.handle, "tray.set_icon failed"))
      }
  }
}

///|
/// Pumps one native tray loop iteration.
///
/// Call this from long-running native applications when the host platform needs
/// event-loop progress from the tray backend. A return value of `Ok(false)`
/// means the backend asked to stop processing.
pub fn Tray::pump(
  self : Tray,
  blocking? : Bool = false,
) -> Result[Bool, String] {
  match self.ensure_alive() {
    Err(error) => Err(error)
    Ok(_) =>
      if !self.native {
        Ok(true)
      } else {
        let status = pump_state_ffi(self.handle, if blocking { 1 } else { 0 })
        if status > 0 {
          Ok(true)
        } else if status == 0 {
          Ok(false)
        } else {
          Err(self.state_error(self.handle, "tray.pump failed"))
        }
      }
  }
}

///|
/// Releases the underlying native resources and turns the handle into a no-op
/// object that rejects later operations.
pub fn Tray::destroy(self : Tray) -> Unit {
  if self.destroyed {
    return ()
  }
  if self.native {
    destroy_state_ffi(self.handle)
  }
  self.handle = zero_handle()
  self.native = false
  self.visible = false
  self.destroyed = true
}

///|
fn Tray::ensure_alive(self : Tray) -> Result[Unit, String] {
  if self.destroyed {
    Err("tray handle has been destroyed")
  } else {
    Ok(())
  }
}

///|
fn Tray::state_error(_self : Tray, handle : Int64, fallback : String) -> String {
  decode_bytes(last_state_error_ffi(handle)).unwrap_or(fallback)
}

///|
fn encode_text(value : String) -> Bytes {
  @utf8.encode(value)
}

///|
fn encode_optional_text(value : String?) -> Bytes {
  @utf8.encode(value.unwrap_or(""))
}

///|
fn decode_bytes(bytes : Bytes) -> String? {
  if bytes.is_empty() {
    None
  } else {
    Some(@utf8.decode_lossy(bytes))
  }
}

///|
fn zero_handle() -> Int64 {
  Int64::from_int(0)
}
