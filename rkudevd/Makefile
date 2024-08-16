TARGET=bin/rkudevd
SRC=src/main.cpp

CFLAGS = -g -DDEBUG
#CFLAGS = -s -O2

all: $(TARGET)

clean:
	@rm -rf $(TARGET)

$(TARGET): $(SRC)
	@mkdir -p bin
	@g++ $(CFLAGS) $(SRC) -lpthread -static -o $@

