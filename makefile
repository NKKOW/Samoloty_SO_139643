# Nazwa kompilatora i opcje
CC      = gcc
CFLAGS  = -Wall -Wextra -pthread

# Pliki źródłowe (podzielone na dwie grupy):
OBJS_DYSPOZYTOR = dyspozytor.o gate.o queue.o global.o
OBJS_SAMOLOT    = samolot.o pasazer.o kontrola.o global.o

# Domyślny cel, który buduje oba programy
all: msgqueue.key dyspozytor samolot

# Reguła budowy dyspozytora
dyspozytor: msgqueue.key $(OBJS_DYSPOZYTOR)
	$(CC) $(CFLAGS) -o $@ $(OBJS_DYSPOZYTOR)

# Reguła budowy samolotu
samolot: msgqueue.key $(OBJS_SAMOLOT)
	$(CC) $(CFLAGS) -o $@ $(OBJS_SAMOLOT)

# Reguła tworzenia pliku msgqueue.key
msgqueue.key:
	@echo "Tworzenie pliku msgqueue.key."
	touch msgqueue.key

# Reguły kompilacji plików .c -> .o
%.o: %.c
	$(CC) $(CFLAGS) -c $<

# Sprzątanie po kompilacji
clean:
	rm -f *.o
	rm -f dyspozytor samolot
	rm -f kontrola_fifo_REG
	rm -f kontrola_fifo_vip
	rm -f msgqueue.key

# Phony – te cele nie korespondują z plikami
.PHONY: all clean