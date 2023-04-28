all:
	cc -c cs.c -o cs.o
	cc -c hwt.c -o hwt.o
	cc -lopencsd -lm -lc++ hwt.o cs.o -o hwt
