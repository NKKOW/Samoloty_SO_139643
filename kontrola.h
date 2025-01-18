#ifndef KONTROLA_H
#define KONTROLA_H

#include <pthread.h>

//Struktura stanowiska kontroli
typedef struct {
    int occupied; //czy zajęte, ile osób na stanowsiku (0, 1, 2)
    char plec; // 'K' / 'M' lub '\0' gdy brak
} StanowiskoBezp;

extern StanowiskoBezp stanowiska[3];
extern pthread_mutex_t stanowsikoMutex;
extern pthread_cond_t stanowiskoCond;
extern pthread_mutex_t mutex;
extern pthread_cond_t samolotCond;
extern int licznik_pasazer;
extern int N;
extern int capacity;

void kontrola_bezpieczenstwa(long pasazer_id, char plec);

#endif