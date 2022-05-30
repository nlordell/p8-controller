use std::{
    io::{BufRead as _, BufReader, BufWriter, Write as _},
    process::{ChildStdin, ChildStdout, Command, Stdio},
    sync::{
        atomic::{AtomicU32, Ordering},
        mpsc, Arc,
    },
    thread,
    time::Duration,
};

type Result<T = (), E = Box<dyn std::error::Error + Send + Sync + 'static>> =
    std::result::Result<T, E>;

fn main() -> Result {
    let state = Arc::new(AtomicU32::default());

    let pico = thread::spawn({
        let state = state.clone();
        move || pico(state)
    });

    while !pico.is_finished() {
        state.fetch_add(0x1000, Ordering::SeqCst);
        thread::sleep(Duration::from_secs_f64(1. / 16.));
    }

    Ok(())
}

fn pico(state: Arc<AtomicU32>) -> Result {
    let mut pico8 = Command::new("/Applications/PICO-8.app/Contents/MacOS/pico8")
        .args(&["cart/controller.p8"])
        .stdout(Stdio::piped())
        .stdin(Stdio::piped())
        .spawn()?;

    let (sender, receiver) = mpsc::channel();
    let stdout = pico8.stdout.take().unwrap();
    let stdin = pico8.stdin.take().unwrap();

    let _ = thread::spawn(move || read_output(stdout, sender));
    let _ = thread::spawn(move || write_input(stdin, receiver, state));

    pico8.wait()?;

    Ok(())
}

fn read_output(stdout: ChildStdout, sender: mpsc::Sender<()>) -> Result {
    let mut reader = BufReader::new(stdout);
    let mut buffer = String::new();
    while reader.read_line(&mut buffer)? > 0 {
        sender.send(())?;
    }

    Ok(())
}

fn write_input(stdin: ChildStdin, receiver: mpsc::Receiver<()>, state: Arc<AtomicU32>) -> Result {
    let mut writer = BufWriter::new(stdin);
    loop {
        let state = state.load(Ordering::SeqCst);
        writer.write_all(&state.to_le_bytes())?;
        writer.write_all(b"\n")?;
        writer.flush()?;

        receiver.recv()?;
    }
}
