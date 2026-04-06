///|
extern "C" fn current_platform_ffi() -> Int = "moonbit_tray_current_platform"

///|
extern "C" fn is_supported_ffi() -> Bool = "moonbit_tray_is_supported"

///|
extern "C" fn support_error_ffi() -> Bytes = "moonbit_tray_support_error"

///|
#borrow(identifier, icon, tooltip)
extern "C" fn create_state_ffi(
  identifier : Bytes,
  icon : Bytes,
  tooltip : Bytes,
) -> Int64 = "moonbit_tray_create"

///|
extern "C" fn last_create_error_ffi() -> Bytes = "moonbit_tray_last_create_error"

///|
extern "C" fn destroy_state_ffi(state : Int64) -> Unit = "moonbit_tray_destroy"

///|
#borrow(tooltip)
extern "C" fn show_state_ffi(state : Int64, tooltip : Bytes) -> Bool = "moonbit_tray_show"

///|
extern "C" fn hide_state_ffi(state : Int64) -> Bool = "moonbit_tray_hide"

///|
#borrow(tooltip)
extern "C" fn set_tooltip_ffi(state : Int64, tooltip : Bytes) -> Bool = "moonbit_tray_set_tooltip"

///|
#borrow(icon)
extern "C" fn set_icon_ffi(state : Int64, icon : Bytes) -> Bool = "moonbit_tray_set_icon"

///|
extern "C" fn pump_state_ffi(state : Int64, blocking : Int) -> Int = "moonbit_tray_pump"

///|
extern "C" fn last_state_error_ffi(state : Int64) -> Bytes = "moonbit_tray_last_error"
