target/pico8-ctrl: src/main.c
	mkdir -p target
	$(CC) -std=c11 $< -o $@ -Wall `sdl2-config --cflags --libs`
