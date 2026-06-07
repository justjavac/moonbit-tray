///|
/// Builds a simulated tray value for white-box state transition tests.
fn create_simulated_tray(
  identifier? : String = default_identifier(),
  icon? : String? = None,
  tooltip? : String = "",
  platform? : Platform = current_platform(),
) -> Tray {
  {
    handle: zero_handle(),
    native: false,
    platform,
    identifier: normalize_identifier(identifier),
    icon,
    tooltip,
    visible: false,
    destroyed: false,
  }
}

///|
test "identifier normalization falls back to the default" {
  inspect(normalize_identifier("   "), content="moonbit-tray")
  inspect(normalize_identifier(" app.tray "), content="app.tray")
}

///|
test "simulated tray lifecycle updates visible state" {
  let tray = create_simulated_tray(
    identifier="demo",
    icon=Some("demo.ico"),
    tooltip="initial",
    platform=Windows,
  )
  debug_inspect((tray.visible, tray.tooltip), content="(false, \"initial\")")
  debug_inspect(tray.show(), content="Ok(true)")
  debug_inspect((tray.visible, tray.tooltip), content="(true, \"initial\")")
  debug_inspect(tray.set_tooltip("updated"), content="Ok(true)")
  debug_inspect((tray.visible, tray.tooltip), content="(true, \"updated\")")
  debug_inspect(tray.set_icon(None), content="Ok(true)")
  debug_inspect((tray.visible, tray.icon), content="(true, None)")
  debug_inspect(tray.hide(), content="Ok(false)")
  inspect(tray.visible, content="false")
}

///|
test "show can replace the tooltip inline" {
  let tray = create_simulated_tray(tooltip="before", platform=MacOS)
  debug_inspect(tray.show(tooltip=Some("after")), content="Ok(true)")
  debug_inspect((tray.tooltip, tray.visible), content="(\"after\", true)")
}

///|
test "native hidden operations stay usable when backend is available" {
  if is_supported() {
    let tray = match
      create(identifier="justjavac.tray.test", tooltip="hidden") {
      Ok(tray) => tray
      Err(error) => fail(error)
    }
    debug_inspect(
      (tray.native, tray.visible, tray.destroyed),
      content="(true, false, false)",
    )
    debug_inspect(tray.set_tooltip("hidden updated"), content="Ok(false)")
    debug_inspect(tray.set_icon(None), content="Ok(false)")
    debug_inspect(tray.hide(), content="Ok(false)")
    debug_inspect(tray.pump(), content="Ok(true)")
    tray.destroy()
    debug_inspect((tray.native, tray.destroyed), content="(false, true)")
  } else {
    inspect("unsupported", content="unsupported")
  }
}

///|
test "destroy prevents later operations" {
  let tray = create_simulated_tray(platform=Linux)
  tray.destroy()
  debug_inspect((tray.visible, tray.destroyed), content="(false, true)")
  debug_inspect(tray.show(), content="Err(\"tray handle has been destroyed\")")
  debug_inspect(tray.hide(), content="Err(\"tray handle has been destroyed\")")
  debug_inspect(
    tray.set_tooltip("ignored"),
    content="Err(\"tray handle has been destroyed\")",
  )
  debug_inspect(
    tray.set_icon(Some("icon.png")),
    content="Err(\"tray handle has been destroyed\")",
  )
  debug_inspect(tray.pump(), content="Err(\"tray handle has been destroyed\")")
}
