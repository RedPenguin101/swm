all:
	cc swm.c -lX11 -lXft -I/usr/include/uuid -I/usr/include/freetype2 -I/usr/include/libpng16 -o swm