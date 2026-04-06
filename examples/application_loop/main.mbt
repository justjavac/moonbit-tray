///|
fn update_step(tray : @tray.Tray, step : Int, tooltip : String) -> Unit {
  match tray.set_tooltip(tooltip) {
    Ok(visible) => println("step \{step}: tooltip updated, visible=\{visible}")
    Err(error) => println("step \{step}: tooltip update failed: \{error}")
  }
  match tray.pump() {
    Ok(true) => println("step \{step}: pump ok")
    Ok(false) => println("step \{step}: backend requested stop")
    Err(error) => println("step \{step}: pump failed: \{error}")
  }
}

///|
fn main {
  let tooltips = [
    "booting tray backend", "warming up application state", "ready for user interaction",
  ]
  match
    @tray.create(identifier="justjavac.tray.examples.loop", tooltip=tooltips[0]) {
    Ok(tray) => {
      ignore(tray.show())
      for step, tooltip in tooltips {
        update_step(tray, step, tooltip)
      }
      ignore(tray.hide())
      tray.destroy()
      println("application loop example finished")
    }
    Err(error) => println("create failed: \{error}")
  }
}
