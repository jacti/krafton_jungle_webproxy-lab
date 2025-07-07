CC = gcc
CFLAGS = -g -O0 -Wall -I ..

# Output directory
BINDIR = ../../out/bin/cgi-bin
TARGET = $(BINDIR)/adder $(BINDIR)/player

all: $(TARGET)

# Create bin directory if needed
$(BINDIR):
	mkdir -p $@

# Compile adder into BINDIR
$(BINDIR)/adder: adder.c | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ adder.c

$(BINDIR)/player: player.c | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ player.c

clean:
	rm -f *~ $(TARGET)
