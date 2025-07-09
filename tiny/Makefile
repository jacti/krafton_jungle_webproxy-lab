CC = gcc
CFLAGS = -g -O0 -Wall -I .

LIB = -lpthread

# Output directories relative to this Makefile
BINDIR = ../out/bin
OBJDIR = ../out/obj
LIBDIR = ../include

all: $(BINDIR)/tiny cgi

# Create output directories if needed
$(BINDIR) $(OBJDIR):
	mkdir -p $@

# Compile csapp.o into shared obj dir
$(OBJDIR)/csapp.o: $(LIBDIR)/csapp.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $(LIBDIR)/csapp.c -o $@

# Build tiny using out/bin and out/obj
$(BINDIR)/tiny: tiny.c $(OBJDIR)/csapp.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ tiny.c $(OBJDIR)/csapp.o $(LIB)

# Run a binary from BINDIR with optional arguments
run:
	@if [ -z "$(prog)" ]; then \
		echo "Usage: make run prog=target args='arguments'"; \
		exit 1; \
	fi; \
	if [ ! -x $(BINDIR)/$(prog) ]; then \
		echo "Error: $(BINDIR)/$(prog) not found or not executable"; \
		exit 1; \
	fi; \
	echo "Running $(abspath $(BINDIR))/$(prog) $(args)"; \
	cd .. && $(abspath $(BINDIR))/$(prog) $(args)

# Build CGI programs
cgi:
	(cd cgi-bin; make)

clean:
	rm -f *~ 
	rm -f $(OBJDIR)/csapp.o $(BINDIR)/tiny
	(cd cgi-bin; make clean)
