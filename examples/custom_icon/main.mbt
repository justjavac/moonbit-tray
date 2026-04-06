///|
/// Prints command-line usage for the custom-icon example.
fn usage(program : String) -> Unit {
  println("usage: \{program} [icon-path] [tooltip]")
  println("when icon-path is omitted, the platform default tray icon is used")
}

///|
/// Prints the visibility result of a tray operation in a compact demo format.
fn print_result(action : String, result : Result[Bool, String]) -> Unit {
  match result {
    Ok(visible) => println("\{action}: visible=\{visible}")
    Err(error) => println("\{action}: failed: \{error}")
  }
}

///|
/// Runs the custom-icon example and briefly switches back to the default icon.
fn main {
  let args = @env.args()
  let program = args.get(0).unwrap_or("custom_icon")
  let icon = args.get(1)
  let tooltip = args.get(2).unwrap_or("custom icon example")

  usage(program)
  match
    @tray.create(
      identifier="justjavac.tray.examples.custom-icon",
      icon~,
      tooltip~,
    ) {
    Ok(tray) => {
      println("created tray with icon=\{icon}")
      print_result("show", tray.show())
      match tray.pump() {
        Ok(true) => println("pump: ok")
        Ok(false) => println("pump: backend requested stop")
        Err(error) => println("pump: failed: \{error}")
      }
      print_result("set_icon", tray.set_icon(None))
      print_result("hide", tray.hide())
      tray.destroy()
      println("custom icon example finished")
    }
    Err(error) => println("create failed: \{error}")
  }
}
