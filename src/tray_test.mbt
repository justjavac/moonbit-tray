///|
test "default identifier is stable" {
  inspect(@tray.default_identifier(), content="moonbit-tray")
}

///|
test "support probe stays consistent" {
  let supported = @tray.is_supported()
  let ensured = @tray.ensure_supported()
  if supported {
    inspect(ensured, content="Ok(())")
  } else {
    guard ensured is Err(_) else {
      fail("ensure_supported should return an error when support probe fails")
    }
  }
}

///|
test "native integration stays opt in" {
  guard @env.get_env_var("MOONBIT_TRAY_RUN_NATIVE_TESTS") is Some("1") else {
    inspect("skipped", content="skipped")
    return ()
  }
  let tray = match @tray.create(tooltip="MoonBit tray integration") {
    Ok(tray) => tray
    Err(error) => fail(error)
  }
  inspect(tray.show(), content="Ok(true)")
  inspect(
    tray.set_tooltip("MoonBit tray integration updated"),
    content="Ok(true)",
  )
  inspect(tray.hide(), content="Ok(false)")
  tray.destroy()
}
