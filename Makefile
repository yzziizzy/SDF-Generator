INCLUDES=-I/usr/include/freetype2
CC=gcc
CFLAGS=$(INCLUDES) -std=gnu11 -g -DLINUX


SOURCES=main.c \
	c3dlas.c \
	ds.c \
	dumpImage.c \
	fcfg.c \
	font.c \
	hash.c \
	MurmurHash3.c



LIBS=-lfreetype -lfontconfig -lpng -lm -pthread


OBJS = $(SOURCES:.c=.o)

all: sdfgen


.c.o:
	$(CC) $(CFLAGS) -o $@ -c $< 

sdfgen: $(OBJS)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^




clean:
	$(RM) *.o sdfgen
	
	
# .PHONY: clean

# clean:
# 	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~ 
