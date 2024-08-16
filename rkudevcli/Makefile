SRC = src/main.cpp
DST = bin/rkudevcli

CFLAGS += -g3
CFLAGS += -I../../common/libusb/libusb
LFLAGS += -L../../common/libusb/libusb/.libs
LFLAGS += -lusb-1.0 -ludev

.PHONY: all clean prepare

all: prepare $(DST)

clean:
	@rm -rf $(DST)

prepare:
	@mkdir -p bin

$(DST): $(SRC)
	@g++ $(CFLAGS) $^ $(LFLAGS) -o $@
