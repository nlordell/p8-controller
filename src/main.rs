//! Controller adapter for PICO-8.

mod sdl;

use sdl::{GameController, Init, Sdl};
use std::{
    env,
    error::Error,
    io::{BufRead as _, BufReader, Write as _},
    process::{Command, Stdio},
};

type Result<T, E = Box<dyn Error>> = std::result::Result<T, E>;

fn main() -> Result<()> {
    let sdl = Sdl::init(Init::GAMECONTROLLER)?;

    let mut pico8 = Command::new(env::var("PICO8_EXE").as_deref().unwrap_or("pico8"))
        .args(env::args().skip(1))
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .spawn()?;

    let mut clock = BufReader::new(pico8.stdout.take().unwrap());
    let mut data = pico8.stdin.take().unwrap();

    let mut line = String::new();
    let mut controllers = Controllers::new(&sdl);
    while {
        line.clear();
        clock.read_line(&mut line)? > 0
    } {
        let device_index = match request(&line) {
            Some(value) => value,
            None => {
                print!("{line}");
                continue;
            }
        };
        let controller = controllers.handle(device_index)?;

        data.write_all(&[0xff; 30])?;
        data.flush()?;
    }

    Ok(())
}

fn request(line: &str) -> Option<usize> {
    line.strip_prefix("☉")?.strip_suffix("☉\n")?.parse().ok()
}

struct Controllers<'a> {
    sdl: &'a Sdl,
    handles: Vec<Option<GameController>>,
}

impl<'a> Controllers<'a> {
    fn new(sdl: &'a Sdl) -> Self {
        Self {
            sdl,
            handles: Default::default(),
        }
    }

    fn handle(&mut self, device_index: usize) -> Result<Option<&GameController>> {
        let sdl = self.sdl;

        if !sdl.is_game_controller(device_index) {
            return Ok(None);
        }

        // SDL reuses game controller allocations - so take advantage of this
        // and just re-open all game controllers whenever we need one.
        self.handles.resize_with(device_index + 1, || None);
        self.handles[device_index] = Some(sdl.game_controller_open(device_index)?);

        Ok(self.handles[device_index].as_ref())
    }
}
