LIBSO = xvid.dll


SRCDIR = ../src


SRC = \
 	$(SRCDIR)/driverproc.c \
	$(SRCDIR)/config.c \
	$(SRCDIR)/codec.c \
	$(SRCDIR)/2pass.c

SRC_RES = \
	$(SRCDIR)/config.rc


DIRS = \
	$(SRCDIR)/*.o

