#
# /emx/test/makefile
#
CFLAGS=-g -Wall
CC=gcc $(CFLAGS)
B=/emx/bin/
I=/emx/include/
P=/emx/include.cpp/
L=/emx/lib/
SIGNAL=$(I)signal.h $(I)sys/signal.h
FCNTL=$(I)fcntl.h $(I)sys/fcntl.h
LIBS=$(L)crt0.o $(L)libc.a $(L)libgcc.a
LIBP=$(L)libgpp.a
LIBOS2=$(L)libos2.a
LIBBSD=$(L)libbsd.a

.SUFFIXES: .o .c .cc .s .exe

.c.o:
	$(CC) -c $<

.cc.o:
	$(CC) -c $<

.c.s:
	$(CC) -S $<

.o.exe:
	$(CC) -o $*.exe $<

Default :	timeup.exe


all :	timeup.exe vmem.exe


timeup.res :	timeup.rc timeup.ico timeup.h
	rc -r -i \emx\include timeup.rc

timeup.exe : timeup.c timeup.res timeup.def timeup.rc
	gcc -o timeup.exe timeup.c timeup.def timeup.res -O2 -Zomf

vmem.res :	vmem.rc vmem.ico
	rc -r -i \emx\include vmem.rc


vmem.exe :	vmem.c vmem.res vmem.def vmem.rc
	gcc -o vmem.exe vmem.c vmem.def vmem.res -O2 -Zomf

