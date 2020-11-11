all:
	sdcc -mmcs51 loader.c
	sdcc -mmcs51 hello.c
