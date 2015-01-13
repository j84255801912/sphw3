#include <signal.h>
#include <stdio.h>
#include <unistd.h>

void sig_handler(int signo)
{
    if (signo == SIGUSR1)
        fprintf(stderr, "received SIGUSR1\n");
    else if (signo == SIGINT)
        fprintf(stderr, "received SIGINT\n");
}
/*
void sig_handler1(int signo)
{
    if (signo == SIGINT)
        fprintf(stderr, "received SIGINT\n");
}
*/
int main(void)
{
    if (signal(SIGUSR1, sig_handler) == SIG_ERR)
        fprintf(stderr, "\ncan't catch SIGUSR1\n");
    if (signal(SIGINT, sig_handler) == SIG_ERR)
        fprintf(stderr, "\ncan't catch SIGINT\n");
    while (1)
        sleep(1);
    return 0;
}
