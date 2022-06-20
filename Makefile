PICO8 ?= pico8

.PHONY: all
all: target/p8-controller target/demo.html

target/p8-controller: src/controller.c
	mkdir -p target
	$(CC) -std=c11 $< -o $@ -Wall `sdl2-config --cflags --libs`

target/p8-controller.js: src/controller.js
	mkdir -p target
	cp $< $@

target/demo.html: cart/demo.p8 target/p8-controller.js
	mkdir -p target
	$(PICO8) $< -export $@
	sed 's|</body>|<script src="p8-controller.js"></script></body>|' $@ > target/sed.html
	mv target/sed.html $@

clean:
	rm -rf target
