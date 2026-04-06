///|
/// Prints command-line usage for the custom-icon example.
fn usage(program : String) -> Unit {
  println("usage: \{program} [icon-path] [tooltip]")
  println("when icon-path is omitted, the platform default tray icon is used")
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
      ignore(tray.show())
      ignore(tray.pump())
      ignore(tray.set_icon(None))
      ignore(tray.hide())
      tray.destroy()
      println("custom icon example finished")
    }
    Err(error) => println("create failed: \{error}")
  }
}
