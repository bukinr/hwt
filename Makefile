OBJS += libpmcstat_image.o
OBJS += libpmcstat_string.o
OBJS += libpmcstat_symbol.o
OBJS += hwt.o
OBJS += hwt_process.o
OBJS += hwt_record.o
OBJS += cs.o

LIBS += -lc++ -lc -lelf
LIBS += -lopencsd

all:
	cc -c ${.CURDIR}/libpmcstat/libpmcstat_image.c -o libpmcstat_image.o
	cc -c ${.CURDIR}/libpmcstat/libpmcstat_string.c -o libpmcstat_string.o
	cc -c ${.CURDIR}/libpmcstat/libpmcstat_symbol.c -o libpmcstat_symbol.o
	cc -c ${.CURDIR}/hwt_process.c -o hwt_process.o
	cc -c ${.CURDIR}/hwt_record.c -o hwt_record.o
	cc -c ${.CURDIR}/hwt.c -o hwt.o
	cc -c ${.CURDIR}/cs.c -o cs.o

	cc ${LIBS} ${OBJS} -o ${.CURDIR}/hwt

clean:
	rm -rf obj/*
