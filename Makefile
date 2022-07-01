PICO8 ?= pico8

POOM_SRC := $(wildcard cart/poom/*.lua)
POOM_HTML_DAT := $(addprefix cart/poom/,poom_1.p8 poom_2.p8 poom_3.p8 poom_4.p8 poom_5.p8 poom_6.p8 poom_7.p8 poom_8.p8 poom_9.p8 poom_10.p8 poom_11.p8 poom_12.p8 poom_13.p8 poom_e1.p8 poom_e2.p8)
POOM_HTML_CARTS := cart/poom/poom_0.p8 $(POOM_HTML_DAT)

.PHONY: all
all: target/p8-controller target/demo.html target/poom.html cart/poom/poom.patch

.PHONY: poom
poom: target/p8-controller
	target/p8-controller -root_path cart/poom -run cart/poom/poom_0.p8

target/p8-controller: src/controller.c
	mkdir -p target
	$(CC) -std=c11 $< -o $@ -Wall `sdl2-config --cflags --libs`

target/p8-controller.js: src/controller.js
	mkdir -p target
	cp $< $@

target/demo.html: cart/demo.p8 target/p8-controller.js
	mkdir -p target
	$(PICO8) $< -export $@
	sed 's|</body>|<script src="p8-controller.js"></script></body>|' $@ > $@.sed
	mv $@.sed $@

target/poom.html: $(POOM_HTML_CARTS) $(POOM_SRC) target/p8-controller.js
	mkdir -p target
	$(PICO8) $< -export "$@ $(POOM_HTML_DAT)"
	sed 's|</body>|<script src="p8-controller.js"></script></body>|' $@ > $@.sed
	mv $@.sed $@

cart/poom/poom.patch: $(POOM_SRC)
	git diff 4283c235701998d9fdb8c290b512b59004f4143e -- cart/poom ':(exclude)cart/poom/poom.patch' > cart/poom/poom.patch

clean:
	rm -rf target
