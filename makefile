# --- Makefile ---

CC      = gcc
CFLAGS  = -Wall -Wextra -pthread

# Pliki obiektów do dyspozytora:
OBJS_DYSPOZYTOR = dyspozytor.o gate.o queue.o global.o

# Pliki obiektów do samolotu
OBJS_SAMOLOT    = samolot.o global.o
# (jeśli samolot używa funkcji z kontrola.c, dodaj kontrola.o)

# Pliki obiektów do pasażera
OBJS_PASAZER    = pasazer.o kontrola.o global.o

# ----------------------------------------------
# Budujemy wszystko (3 programy i plik msgqueue.key)
all: dyspozytor samolot pasazer msgqueue.key

dyspozytor: $(OBJS_DYSPOZYTOR)
	$(CC) $(CFLAGS) -o $@ $(OBJS_DYSPOZYTOR)

samolot: $(OBJS_SAMOLOT)
	$(CC) $(CFLAGS) -o $@ $(OBJS_SAMOLOT)

pasazer: $(OBJS_PASAZER)
	$(CC) $(CFLAGS) -o $@ $(OBJS_PASAZER)

msgqueue.key:
	@echo "Tworzenie pliku msgqueue.key."
	touch msgqueue.key

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o
	rm -f dyspozytor samolot pasazer
	rm -f kontrola_fifo_REG
	rm -f kontrola_fifo_vip
	rm -f msgqueue.key

.PHONY: all clean
