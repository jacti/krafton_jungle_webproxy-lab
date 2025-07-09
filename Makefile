# Makefile for Proxy Lab
.PHONY: tiny

CC = gcc
CFLAGS = -g -O0 -Wall
LDFLAGS = -lpthread

# Output directories
BINDIR = out/bin
OBJDIR = out/obj
LIBDIR = include

# Object files
OBJS = $(OBJDIR)/proxy.o $(OBJDIR)/csapp.o

all: $(BINDIR)/proxy tiny

# Create output directories if they don't exist
$(BINDIR) $(OBJDIR):
	mkdir -p $@

# Compile object files into OBJDIR
$(OBJDIR)/csapp.o: $(LIBDIR)/csapp.c $(LIBDIR)/csapp.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $(LIBDIR)/csapp.c -o $@

$(OBJDIR)/proxy.o: proxy.c $(LIBDIR)/csapp.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c proxy.c -o $@

# Link to create binary in BINDIR
$(BINDIR)/proxy: $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

tiny:
	(cd tiny; make)

run-tiny:
	(cd tiny; make run prog=tiny args="9000")

# Creates a tarball in ../proxylab-handin.tar (DO NOT MODIFY)
handin:
	(make clean; cd ..; tar cvf $(USER)-proxylab-handin.tar proxylab-handout --exclude tiny --exclude nop-server.py --exclude proxy --exclude driver.sh --exclude port-for-user.pl --exclude free-port.sh --exclude ".*")

clean:
	rm -f *~ *.tar *.zip *.gzip *.bzip *.gz
	rm -rf out
	rm -rf .proxy
	rm -rf .noproxy
