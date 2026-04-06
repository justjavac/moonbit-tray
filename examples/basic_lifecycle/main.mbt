///|
/// Prints the visibility result of a tray operation in a compact demo format.
fn print_result(action : String, result : Result[Bool, String]) -> Unit {
  match result {
    Ok(visible) => println("\{action}: visible=\{visible}")
    Err(error) => println("\{action}: failed: \{error}")
  }
}

///|
/// Runs the basic tray lifecycle example from create to destroy.
fn main {
  match
    @tray.create(
      identifier="justjavac.tray.examples.basic",
      tooltip="basic lifecycle example",
    ) {
    Ok(tray) => {
      println("created tray on \{@tray.current_platform()}")
      print_result("set_tooltip", tray.set_tooltip("ready to show"))
      print_result("show", tray.show())
      for _ in 0..<5 {
        ignore(tray.pump())
      }
      print_result("hide", tray.hide())
      tray.destroy()
      println("destroyed tray handle")
    }
    Err(error) => println("create failed: \{error}")
  }
}
