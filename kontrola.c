#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "kontrola.h"

StanowiskoBezp stanowiska[3] = {
    {0, '\0'},
    {0, '\0'},
    {0, '\0'}
};

pthread_mutex_t stanowsikoMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t stanowiskoCond = PTHREAD_COND_INITIALIZER;

void kontrola_bezpieczenstwa(long pasazer_id, char plec){

    pthread_mutex_lock(&stanowsikoMutex);

    //Szukanie wolnego stanowiska
    while(1)
    {
        int wolneStanowisko = -1; //ustawiony wartownik
        for (int i = 0; i < 3; i++)
        {
            if (stanowiska[i].occupied < 2)
            {
                if (stanowiska[i].occupied == 0 || stanowiska[i].plec == plec)
                {
                    wolneStanowisko = i;
                    break;
                }
            }
        }

        //Po znalezieniu stanowiska pasażer zajmuje stanowisko

        if (wolneStanowisko >= 0)
        {
            stanowiska[wolneStanowisko].occupied++;
            if (stanowiska[wolneStanowisko].occupied == 1)
            {
                stanowiska[wolneStanowisko].plec = plec;
            }

            pthread_mutex_unlock(&stanowsikoMutex);

            //symulacja kontroli
            int kontrola_checktime = (rand() % 3) + 1;
            sleep(kontrola_checktime);
            printf("Pasazer %ld (plec %c) zajal stanowisko %d\n", pasazer_id, plec, wolneStanowisko);
            //Sprawdzanie czy odrzuci pasażera z powodu niebezpiecznego przedmiotu:
            int niebezpieczny = (rand() % 30 == 0); //10% szans na przedmiot zabroniony
            if (niebezpieczny) {
                // Odrzucenie
                pthread_mutex_lock(&stanowsikoMutex);
                stanowiska[wolneStanowisko].occupied--;
                if (stanowiska[wolneStanowisko].occupied == 0) 
                {
                    stanowiska[wolneStanowisko].plec = '\0';
                }
                pthread_cond_broadcast(&stanowiskoCond);
                pthread_mutex_unlock(&stanowsikoMutex);

                // Inkrementacja licznika
                pthread_mutex_lock(&mutex);
                licznik_pasazer++;
                if (licznik_pasazer == N || capacity == 0) 
                {
                    pthread_cond_signal(&samolotCond);
                }
                pthread_mutex_unlock(&mutex);

                printf("Pasażer %ld odrzucony w kontroli bezpieczeństwa!\n", pasazer_id);
                return;
            }

            //Zwalnianie stanowiska
            pthread_mutex_lock(&stanowsikoMutex);
            stanowiska[wolneStanowisko].occupied--;
            if (stanowiska[wolneStanowisko].occupied == 0)
            {
                stanowiska[wolneStanowisko].plec = '\0';
            }

            printf("Pasazer %ld (plec %c) zwolnil stanowisko %d (occupied = %d)\n", pasazer_id,
                    plec, wolneStanowisko, stanowiska[wolneStanowisko].occupied);

            //Powiadamianie wszystkich pasażerów o zmianie
            pthread_cond_broadcast(&stanowiskoCond);
            pthread_mutex_unlock(&stanowsikoMutex);
            break; //koniec pętli while
        } else
            {
                //brak miejsc, pasażer czeka
                pthread_cond_wait(&stanowiskoCond, &stanowsikoMutex);
            }
    }

}
