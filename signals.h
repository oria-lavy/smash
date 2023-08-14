#ifndef SMASH__SIGNALS_H_
#define SMASH__SIGNALS_H_


//        note that from
//        the point of view of Linux shell, the smash is the process who is running in foreground and
//        it’s not the process you are currently running from your smash, so the SIGINT is supposed to
//        be sent to the smash main process

void ctrlZHandler(int sig_num); //route SIGSTP to fg cmd
void ctrlCHandler(int sig_num); //route SIGINT to fg cmd
void alarmHandler(int sig_num);

#endif //SMASH__SIGNALS_H_