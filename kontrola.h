#ifndef KONTROLA_H
#define KONTROLA_H

#include <pthread.h>

// Struktura stanowiska kontroli
typedef struct {
    int occupied;    // ile osób aktualnie (0,1,2)
    char plec;       // 'K' lub 'M' lub '\0'
} StanowiskoBezp;

extern StanowiskoBezp stanowiska[3];
extern pthread_mutex_t stanowsikoMutex;
extern pthread_cond_t stanowiskoCond;

// Rozszerzamy sygnaturę o isVIP
void kontrola_bezpieczenstwa(long pasazer_id, char plec, int isVIP);

#endif
