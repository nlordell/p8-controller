# PICO-8 Extended Controller Input Support

A proof-of-concept PICO-8 controller adapter over the GPIO `SERIAL` interface, allowing for extended controller input support including analogue sticks and more buttons.

![demo](cart/demo.gif)

## Future Work

Currently, this is very much a proof of concept.
It currently only works on computers and not for any web targets as it requires processing PICO-8's standard output
(as controller updates are requested with `PRINTH"☉☉"`)
as well as writing to its standard input (as controller updates are read with `SERIAL(0x805, ...)`).
This also means that we require a launcher to setup the required pipes for reading and writing to standard input and output.

In the future, it might be possible to experiment with `SERIAL` channels `0x806` and `0x807` with named pipes.
Also, injecting some JS code is needed using `var pico8_gpio = Array(128);` to achieve a similar features on web-exported carts.
