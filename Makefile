SHELL = /bin/sh
CFLAGS = -pedantic -Wall `sdl2-config --cflags`
LDFLAGS = `sdl2-config --libs` -lSDL2_ttf
TARGET = tetris
OBJ = main.o

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

main.o: main.c

.PHONY: clean
clean:
	rm -f $(TARGET) $(OBJ)
