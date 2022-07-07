# PICO-8 Extended Controller Input Support

A proof-of-concept PICO-8 controller adapter over the GPIO `SERIAL` interface, allowing for extended controller input support including analogue sticks and more buttons.

![demo](cart/demo.gif)

## Building

Building the `p8-controller` process requires:
1. A C11 compiler
2. GNU `make`
3. SDL2 (make sure `sdl2-config` is correctly configured in your path)

Then, once you have all of the required dependencies, run:

```sh
make
```

## Running

### Desktop

Run the `p8-controller` binary.
This will create `controller.data` and `controller.clock` "serial" lines that can be "connected" to a PICO-8 console:

```sh
pico8 -i controller.data > controller.clock
```

For example, you can:
1. `make run` to start the controller handler process and create the "serial" lines
2. `make demo` to start the demo cart attached to the controller "serial" lines
    * Be sure to also try out `make poom` :smiling_imp:

### Web

Just make sure to load the controller JS file in your page hosting the exported PICO-8 game.

```html
<script src="p8-controller.js"></script>
```

Take a look at the `Makefile` to see how we do this automatically for the exported demo and POOM cartridges.

It may be confusing that there is a `src/controller.js` and a `target/p8-controller.js`.
This was done to allow the `Makefile` to potentially bundle multiple JavaScript files in the future.
Currently it just `cp`s the file over.

## How It Works

GPIO works very differently on various PICO-8 targets.
Because of this, the way we support Desktop (Running PICO-8 locally on your computer) and Web (so HTML/JS exported PICO-8 games) are very different.

### Desktop

For desktop targets, additional controller data is sent over a named pipe attached to PICO-8 process's `-i` input file (serial channel `0x806`).
PICO-8 `SERIAL` command allows scheduling reading a certain amount from the named pipe with:

```lua
serial(0x806, target_address, data_length)
```

The basic concept is to periodically send controller data over the named pipe for it to be read with `SERIAL` commands.
The tricky bit is synchronizing the reading and writing of controller data.
For this, we use the PICO-8 process's standard output.
Specifically the controller application will wait for some well-formed message indicating that the game is requesting controller data.
Once this message is received, controller state is read, encoded and sent over PICO-8's standard input.
We use a single digit numerical value alone on a line representing the controller index being polled to request controller data.
For example, to get the controller data for controllers index 2 and 5, you could:

```lua
printh"2\n5" -- request new controller data for controllers 2 and 5 over stdout
serial(0x806,0x9a00,60) -- read 60 bytes from input file, each controller state is 30 bytes long
```

#### Why Not `-o`?

In theory, it should be possible to use serial channels for both input and output lines:

```sh
pico8 -i controller.data -o controller.clock
```

This would have a minor advantage over piping PICO-8's standard output into the `controller.clock` serial line of not losing the PICO-8 cart's standard output.
This allows, for example, for `PRINTH` to be used for debugging.

Unfortunately, this also has a small issue and introduces a frame of controller input delay.
This additional frame delay is interesting as it appears, in part, to be a PICO-8 issue.
Specifically, PICO-8 will always wait for `SERIAL` reads before writes, even if the write was queued first.
For example:

```lua
serial(0x807, ...) -- write to file specified in `-o` parameter
serial(0x806, ...) -- read from file specified in `-i` parameter
```

In this case, unintuitively, PICO-8 will first read from the file spcified by the `-i` parameter **before** writing to the file specified by the `-o` parameter.
Therefore, we need to request controller data one frame early.
This leads to a total of 2 frames of delay for receiving controller input:
- Frame 0: request controller data
    - `FLIP` causes the request to get flushed over the `SERIAL` interface
- Frame 1: read controller data
    - The controller data gets written to memory between frames
- Frame 2: controller data is in memory and can be used in the `_UPDATE` function

```lua
-- Frame 0
serial(0x807, ...) -- signal that we want controller data
flip() -- flush the serial output

-- Frame 1
serial(0x807, ...) -- already request controller data for next frame
serial(0x806, ...) -- queue read of the controller data
flip() -- flush the serial output and read serial data to memory

-- Frame 2
peek(...) -- we can now read controller data from memory
```

By using the fact that `PRINTH` flushes immediately and piping PICO-8's standard output to the `controller.clock` serial line, we only have 1 frame of delay.

#### Why Not Standard Input/Output?

Another alternative solution would have been to use PICO-8's standard input and output instead of named pipes for sending and synchronizing controller data.
In fact, this project used to implement a wrapper binary that started PICO-8 and connected to its standard input and output ([`9765128`](https://github.com/nlordell/p8-controller/commit/976512890f2f1213a14ee5b04aa7d1d91fca5593)).

This method has one major issue.
If you were to start a game with extended controller support without properly attaching a controller to the PICO-8 process's standard input and output will cause PICO-8 to freeze.
This is because the `SERIAL` command would get stuck waiting for 30 bytes of controller data over PICO-8's standard input that will never come.
On the other hand, if you read from serial line `0x806` without specifying a file with the `-i` command line option, PICO-8 will just not read any bytes.
This means that you can even detect if no controller is attached:

```lua
poke(0x9a0c, 0x9a) -- write marker value to memory location of first button
printh"0" -- request controller 0 data
serial(0x806, 0x9a00, 30) -- queue read of the controller data
flip() -- read serial data to memory
if @0x9a0c==0x9a then
  -- controller not connected
end
```

#### What About Windows?

I don't have a Windows machine, so I can't develop a Windows version.
In theory, the same support can be implemented using Win32 named pipes (using `CreateNamedPipe` and `ConnectNamedPipe`).
PRs are welcome!

### Web

For web targets, it is very straight forward.
On each animation frame, we first set the `pico8_gpio[0]` pin to a marker value, so that the PICO-8 cart can detect that it is in web mode.
Then we read controller state and write it directly to `pico8_gpio`.
Easy-peasy-lemon-squeezy.
