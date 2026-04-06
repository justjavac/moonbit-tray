///|
/// Prints the visibility result of a tray operation in a compact demo format.
fn print_result(action : String, result : Result[Bool, String]) -> Unit {
  match result {
    Ok(visible) => println("\{action}: visible=\{visible}")
    Err(error) => println("\{action}: failed: \{error}")
  }
}

///|
/// Updates the tooltip and pumps the backend once for a single demo step.
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
/// Runs the application-loop example that alternates tooltip updates and pumps.
fn main {
  let tooltips = [
    "booting tray backend", "warming up application state", "ready for user interaction",
  ]
  match
    @tray.create(identifier="justjavac.tray.examples.loop", tooltip=tooltips[0]) {
    Ok(tray) => {
      print_result("show", tray.show())
      for step, tooltip in tooltips {
        update_step(tray, step, tooltip)
      }
      print_result("hide", tray.hide())
      tray.destroy()
      println("application loop example finished")
    }
    Err(error) => println("create failed: \{error}")
  }
}
