# Makefile for zblast

# change these if you want anything anywhere else
BINDIR=/usr/games
MANDIR=/usr/man/man6
SOUNDSDIR=/usr/games/zblastsounds


CFLAGS=-I. -L. -O2 -finline-functions -fomit-frame-pointer \
	-DSOUNDSDIR=\"$(SOUNDSDIR)\"

all : zblast

ZBLASTOBJS = zblast.o font.o levels.o

zblast : $(ZBLASTOBJS)
	$(CC) -s -o zblast $(ZBLASTOBJS) -lvga -lrawkey -lm

install: zblast
	install -o root -m 4511 zblast $(BINDIR)
	install -m 444 zblast.6 $(MANDIR)
	@mkdir $(SOUNDSDIR)
	chmod 555 $(SOUNDSDIR)
	install -m 444 sounds/*.raw $(SOUNDSDIR)

clean:
	$(RM) *.o zblast *~

# Dependancies
font.obj : font.c
zblast.obj : zblast.c font.h levels.h
levels.obj : levels.c
