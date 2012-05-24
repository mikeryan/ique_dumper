PROG = ique
OBJS = ique.o main.o

CFLAGS  = -g -Wall -Werror
CFLAGS += `pkg-config --cflags libusb-1.0`

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $(PROG) $(OBJS) `pkg-config --libs libusb-1.0`

install: $(PROG)
	install ique /usr/local/bin

clean:
	rm -f $(PROG) $(OBJS)
