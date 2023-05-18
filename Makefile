LIBS += libpmcstat/libpmcstat_image.o
LIBS += libpmcstat/libpmcstat_string.o
LIBS += libpmcstat/libpmcstat_symbol.o
LIBS += libpmcstat/libpmcstat_process.o
LIBS += /usr/lib/libc++.a
LIBS += /usr/lib/libelf.a
LIBS += libopencsd.a

all:
	cc -c cs.c -o cs.o
	cc -c hwt.c -o hwt.o
	cc -c hwt_process.c -o hwt_process.o
	cc -static ${LIBS} hwt.o hwt_process.o cs.o -o hwt

clean:
	rm -f cs.o hwt.o hwt
