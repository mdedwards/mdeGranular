# Project Setup
NAME = mdeGranular~
SRCDIR = ../../src
SRC = $(SRCDIR)/mdeGranular~.c $(SRCDIR)/mdeGranular~maxmsp.c

# MaxMSP SDK Setup
C74SUPPORT = ../sdk/source/c74support

# Compiler / Linker Setup
CC = gcc
CFLAGS = -c -DWIN_VERSION -DWIN_EXT_VERSION -DMAXMSP -I$(C74SUPPORT)/max-includes -I$(C74SUPPORT)/msp-includes
LDFLAGS = -shared -L$(C74SUPPORT)/max-includes/x64 -L$(C74SUPPORT)/msp-includes/x64 -lMaxAPI -lMaxAudio -static-libgcc

Debug:
	$(CC) $(CFLAGS) -g $(SRC)
	$(CC) -o $(NAME).mxe64 mdeGranular~.o mdeGranular~maxmsp.o $(LDFLAGS)

Release:
	$(CC) $(CFLAGS) -g $(SRC)
	$(CC) -o $(NAME).mxe64 mdeGranular~.o mdeGranular~maxmsp.o $(LDFLAGS)

clean:
	rm -f *.o
	rm -f $(NAME).mxe64


