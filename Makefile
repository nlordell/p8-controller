target/pico8-controller: src/main.c
	mkdir -p target
	$(CC) -std=c11 $< -o $@ -Wall
