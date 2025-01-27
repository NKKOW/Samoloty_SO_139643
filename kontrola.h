#ifndef KONTROLA_H
#define KONTROLA_H

#include <pthread.h>

// Struktura stanowiska kontroli bezpieczeństwa
typedef struct {
    int occupied;    // ile osób aktualnie (0,1,2)
    char plec;       // 'K' lub 'M' lub '\0'
} StanowiskoBezp;

// Stanowiska zwykłe
extern StanowiskoBezp stanowiska[3];
extern pthread_mutex_t stanowiskoMutex;
extern pthread_cond_t  stanowiskoCond;

// Stanowiska VIP
extern StanowiskoBezp vipStanowiska[3];
extern pthread_mutex_t vipStanowiskoMutex;
extern pthread_cond_t  vipStanowiskoCond;

// Deklaracje funkcji kontroli bezpieczeństwa
void kontrola_bezpieczenstwa(long pasazer_id, char plec);
void kontrola_bezpieczenstwa_vip(long pasazer_id, char plec);

#endif  
