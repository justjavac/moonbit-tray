///|
/// Prints one tray event in a compact format.
fn print_event(event : @tray.TrayEvent) -> Unit {
  match event {
    Click => println("left click")
    RightClick => println("right click")
    DoubleClick => println("double click")
    MenuItemClick(item_id) => println("menu item click: \{item_id}")
  }
}

///|
/// Runs one non-blocking tray event-loop tick.
async fn poll_step(tray : @tray.Tray) -> Result[Bool, String] {
  match tray.pump(blocking=false) {
    Err(error) => return Err("pump failed: " + error)
    Ok(false) => return Ok(false)
    Ok(true) => ()
  }
  let mut running = true
  for event in tray.drain_events() {
    print_event(event)
    if event.item_id() == Some("quit") {
      running = false
    }
  }
  @async.sleep(16)
  Ok(running)
}

///|
/// Runs the non-blocking event drain example.
async fn main {
  match
    @tray.create(
      identifier="justjavac.tray.examples.nonblocking_events",
      tooltip="Non-blocking tray event loop",
    ) {
    Ok(tray) => {
      match
        tray.set_menu([
          @tray.TrayMenuItem::normal(id="open", label="Open"),
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
      let mut ticks = 0
      while running && ticks < 600 {
        match poll_step(tray) {
          Ok(value) => running = value
          Err(error) => {
            println(error)
            running = false
          }
        }
        ticks += 1
      }
      ignore(tray.hide())
      tray.destroy()
      println("non-blocking event loop finished")
    }
    Err(error) => println("create failed: \{error}")
  }
}
