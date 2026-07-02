///|
/// Logs one tray event.
fn log_event(event : @tray.TrayEvent) -> Unit {
  match event {
    Click => println("left click")
    RightClick => println("right click")
    DoubleClick => println("double click")
    MenuItemClick(item_id) => println("menu item click: \{item_id}")
  }
}

///|
/// Runs the tray click-events example.
fn main {
  match
    @tray.create(
      identifier="justjavac.tray.examples.click_events",
      tooltip="Click the tray icon",
    ) {
    Ok(tray) => {
      match
        tray.set_menu([
          @tray.TrayMenuItem::normal(id="open", label="Open"),
          @tray.TrayMenuItem::submenu(label="More", items=[
            @tray.TrayMenuItem::normal(id="settings", label="Settings"),
          ]),
          @tray.TrayMenuItem::separator(),
          @tray.TrayMenuItem::normal(id="quit", label="Quit"),
        ]) {
        Ok(_) => ()
        Err(error) => {
          println("set_menu failed: \{error}")
          tray.destroy()
          return
        }
      }
      match tray.show() {
        Ok(_) => ()
        Err(error) => {
          println("show failed: \{error}")
          tray.destroy()
          return
        }
      }
      let mut running = true
      while running {
        match tray.pump(blocking=true) {
          Ok(true) => ()
          Ok(false) => running = false
          Err(error) => {
            println("pump failed: \{error}")
            running = false
          }
        }
        for event in tray.drain_events() {
          log_event(event)
          if event.item_id() == Some("quit") {
            running = false
          }
        }
      }
      ignore(tray.hide())
      tray.destroy()
    }
    Err(error) => println("create failed: \{error}")
  }
}
