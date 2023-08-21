OBJS += hwt_coresight.o
OBJS += hwt_elf.o
OBJS += hwt_process.o
OBJS += hwt_record.o
OBJS += hwt.o
OBJS += libpmcstat_stubs.o

LIBS += -lc++ -lc -lelf -lopencsd -lpmcstat -lxo -lutil

all:
	cc -c ${.CURDIR}/hwt_coresight.c -o hwt_coresight.o
	cc -c ${.CURDIR}/hwt_process.c -o hwt_process.o
	cc -c ${.CURDIR}/hwt_record.c -o hwt_record.o
	cc -c ${.CURDIR}/hwt_elf.c -o hwt_elf.o
	cc -c ${.CURDIR}/hwt.c -o hwt.o
	cc -c ${.CURDIR}/libpmcstat_stubs.c -o libpmcstat_stubs.o

	cc ${LIBS} ${OBJS} -o ${.CURDIR}/hwt

clean:
	rm -f ${.CURDIR}/obj/*
	rm -f ${.CURDIR}/hwt
