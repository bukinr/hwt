OBJECTS += libpmcstat/libpmcstat_image.o
OBJECTS += libpmcstat/libpmcstat_string.o
OBJECTS += libpmcstat/libpmcstat_symbol.o
OBJECTS += libopencsd.a
OBJECTS += hwt.o hwt_process.o cs.o

LIBS += -lc++ -lc -lelf

all:
	cc -c libpmcstat/libpmcstat_image.c -o libpmcstat/libpmcstat_image.o
	cc -c libpmcstat/libpmcstat_string.c -o libpmcstat/libpmcstat_string.o
	cc -c libpmcstat/libpmcstat_symbol.c -o libpmcstat/libpmcstat_symbol.o
	cc -c hwt_process.c -o hwt_process.o
	cc -c hwt.c -o hwt.o
	cc -c cs.c -o cs.o

	cc ${LIBS} ${OBJECTS} -o hwt

clean:
	rm -f cs.o hwt.o hwt
