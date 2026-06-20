.PHONY: all firmware userspace clean

all: firmware

firmware:
	./scripts/build_firmware.sh

userspace:
	$(MAKE) -C userspace

clean:
	rm -rf build
	$(MAKE) -C userspace clean
