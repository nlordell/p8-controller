pico8_gpio[0] = 0x9a;

function p8_controller() {
  const gamepads = [...navigator.getGamepads() ?? []].slice(0, 4);

  let i = 1;
  function putd(value) {
    const int = Math.max(Math.min(~~(value * 0x8000), 0x7fff), -0x8000);
    putc(int);
    putc(int >> 8);
  }
  function putb(value) {
    if (value) {
      putc(0xff);
    } else {
      putc(0);
    }
  }
  function putz(n) {
    for (let i = 0; i < n; i++) {
      putc(0);
    }
  }
  function putc(value) {
    pico8_gpio[i++] = value & 0xff;
  }

  putc(gamepads.length);
  for (const gamepad of gamepads) {
    if (gamepad === null || gamepad.mapping !== "standard") {
      putz(30);
    }

    for (let i = 0; i < 4; i++) {
      putd(gamepad.axes[i]);
    }
    putd(gamepad.buttons[6].value);
    putd(gamepad.buttons[7].value);
    for (let i = 0; i < 18; i++) {
      const button = gamepad.buttons[i];
      putb(button && button.pressed);
    }
  }

  requestAnimationFrame(p8_controller);
}
requestAnimationFrame(p8_controller);
