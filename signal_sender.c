#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    kill(atoi(argv[1]), SIGUSR1);
    return EXIT_SUCCESS;
}
