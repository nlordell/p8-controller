(function p8_controller() {
  const gamepads = [...navigator.getGamepads() ?? []].slice(0, 4);

  let ptr = 0;
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
    pico8_gpio[ptr++] = value & 0xff;
  }

  putc(0x9a);
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

  for (let i = ptr; i < gamepads.length; i++) {
    pico8_gpio[i] = 0;
  }

  requestAnimationFrame(p8_controller);
})();
