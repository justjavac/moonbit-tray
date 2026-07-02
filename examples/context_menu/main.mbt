///|
/// Prints one tray event in a compact format.
fn print_event(event : @tray.TrayEvent) -> Unit {
  match event {
    MenuItemClick(item_id) => println("menu item clicked: \{item_id}")
    _ => println("tray event: \{event.event_name()}")
  }
}

///|
/// Runs the context-menu example.
fn main {
  match
    @tray.create(
      identifier="justjavac.tray.examples.context_menu",
      tooltip="Right-click for menu",
    ) {
    Ok(tray) => {
      let items = [
        @tray.TrayMenuItem::normal(id="show", label="Show Window"),
        @tray.TrayMenuItem::separator(),
        @tray.TrayMenuItem::checkbox(
          id="launch",
          label="Launch at Login",
          checked=true,
        ),
        @tray.TrayMenuItem::submenu(label="More", items=[
          @tray.TrayMenuItem::normal(id="settings", label="Settings"),
          @tray.TrayMenuItem::normal(id="about", label="About"),
        ]),
        @tray.TrayMenuItem::separator(),
        @tray.TrayMenuItem::normal(id="quit", label="Quit"),
      ]
      match tray.set_menu(items) {
        Ok(_) => println("context menu installed")
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
          match event {
            MenuItemClick("quit") => {
              println("quit requested")
              running = false
            }
            _ => print_event(event)
          }
        }
      }
      ignore(tray.hide())
      tray.destroy()
    }
    Err(error) => println("create failed: \{error}")
  }
}
