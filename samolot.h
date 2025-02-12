#ifndef SAMOLOT_H
#define SAMOLOT_H

#include "global.h"

void notify_dyspozytor(rodzaj_wiadomosc msg_type);
void simulate_flight_cycle(int planeIndex, int myPlaneMd, int myPlaneP, int myPlaneTi);
void* stairs_thread_func(void* arg);

#endif
