# Makefile for Proxy Lab

CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

# Output directories
BINDIR = out/bin
OBJDIR = out/obj

# Object files
OBJS = $(OBJDIR)/proxy.o $(OBJDIR)/csapp.o

all: $(BINDIR)/proxy

# Create output directories if they don't exist
$(BINDIR) $(OBJDIR):
	mkdir -p $@

# Compile object files into OBJDIR
$(OBJDIR)/csapp.o: csapp.c csapp.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c csapp.c -o $@

$(OBJDIR)/proxy.o: proxy.c csapp.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c proxy.c -o $@

# Link to create binary in BINDIR
$(BINDIR)/proxy: $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

# Creates a tarball in ../proxylab-handin.tar (DO NOT MODIFY)
handin:
	(make clean; cd ..; tar cvf $(USER)-proxylab-handin.tar proxylab-handout --exclude tiny --exclude nop-server.py --exclude proxy --exclude driver.sh --exclude port-for-user.pl --exclude free-port.sh --exclude ".*")

clean:
	rm -f *~ *.tar *.zip *.gzip *.bzip *.gz
	rm -rf out
