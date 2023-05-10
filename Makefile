all:
	cc -c cs.c -o cs.o
	cc -c hwt.c -o hwt.o
	cc -static /usr/lib/libc++.a libopencsd.a hwt.o cs.o -o hwt

clean:
	rm -f cs.o hwt.o hwt
