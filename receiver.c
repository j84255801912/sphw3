#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <fcntl.h>
#include <math.h>
#include <sys/time.h>

#include "receiver.h"

int out_fd;
int doc_serial[3];
float doc_time[3][NUM_DOCUMENTS];

void init(void)
{
    int i, j;
    for (i = 0; i < 3; i++) {
        doc_serial[i] = 0;
        for (j = 0; j < NUM_DOCUMENTS; j++)
            doc_time[i][j] = 0.0;
    }
}

int add_doc(int type)
{
    float time;
    switch(type) {
        case ORDINARY:
            time = ORDINARY_TIME;
            break;
        case URGENT:
            time = URGENT_TIME;
            break;
        case VERY_URGENT:
            time = VERY_URGENT_TIME;
            break;
        default:
            return -1;
    }
    doc_time[type][doc_serial[type]] = time;
    doc_serial[type]++;
    return type;
}

int num_doc(int type)
{
    float time;
    int i;
    int count = 0;
    for (i = 0; i < NUM_DOCUMENTS; i++)
        if (doc_time[type][i] != 0.0)
            count++;
    return count;
}

int doc_remain_processing(void)
{
    if (num_doc(VERY_URGENT) != 0)
        return VERY_URGENT;
    else if (num_doc(URGENT) != 0)
        return URGENT;
    else if (num_doc(ORDINARY) != 0)
        return ORDINARY;
    else
        return -1;
}

void sig_handler(int signo)
{
    if (signo == SIGINT)
        fprintf(stderr, "received SIGINT\n");
    else if (signo == SIGUSR1) {
        fprintf(stderr, "received URGENT\n");
        add_doc(URGENT);
    } else if (signo == SIGUSR2) {
        fprintf(stderr, "received VERY URGENT\n");
        add_doc(VERY_URGENT);
    }
//    printf("%x\n", &errno);
}

int main(int argc, char *argv[])
{
//    printf("%x\n", __builtin_return_address(0));
    // check argument
    if (argc != 2) {
        fprintf(stderr, "argument error\n");
        exit(EXIT_FAILURE);
    }

    // init all globals
    init();

    // get the test_data file name from argument
    char test_data_name[STRING_SIZE];
    strncpy(test_data_name, argv[1], STRING_SIZE - 1);
    test_data_name[strlen(argv[1])] = '\0';

    // claim some necessary variables
    int i, j;
    char buffer[BUFFER_SIZE];
    buffer[0] = 0;

    // open pipe to redirect sender's stdin & stdout
    // 0 for read, 1 for write
    int pipe_to_sender[2];
    int pipe_from_sender[2];

    // create two-direction pipe first.
    if (pipe(pipe_to_sender) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    if (pipe(pipe_from_sender) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }


    // fork one child to execute the sender program
    pid_t pid;
    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        /* GOAL : make sender's stdin & stdout are from/to receiver */
        /* handle to_sender first, here is "sender"! */
        // close read end
        close(pipe_to_sender[1]);
        dup2(pipe_to_sender[0], STDIN_FILENO);
        close(pipe_to_sender[0]);

        /* handle from_judge */
        // close write end of org to listen from judge
        close(pipe_from_sender[0]);
        dup2(pipe_from_sender[1], STDOUT_FILENO);
        close(pipe_from_sender[1]);

        execl("./sender", "./sender", test_data_name, (char *)0);

        // this should never be run
        perror("execl");
        _exit(EXIT_FAILURE);
    }

    // open a file named with "receiver_log" to record the action of receiver
    // 1. if "start dealing with the doc"
    //      write "receive [priority] [serial_number]"
    // 2. if "receiver finishes the doc"
    //      write "finish [priority] [serial_number]"
    // 3. if receiver read "EOF" from pipe
    //      write "terminate" to "receiver_log"
    //      terminate itself
    // 4. if receiver signal "SIGINT" from keyboard
    //      kill sender
    //      write "terminate" to "receiver_log"
    //      terminate itself
    out_fd = open("receiver_log", O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (out_fd < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    // init the doc_time array
    // setting up the signal_handler
    if (signal(SIGINT, sig_handler) == SIG_ERR)
        fprintf(stderr, "\ncan't catch SIGINT\n");
    if (signal(SIGUSR1, sig_handler) == SIG_ERR)
        fprintf(stderr, "\ncan't catch SIGUSR1\n");
    if (signal(SIGUSR2, sig_handler) == SIG_ERR)
        fprintf(stderr, "\ncan't catch SIGUSR2\n");
    // struct  timeval{
    //   long  tv_sec;
    //   long  tv_usec; //microsec
    // }；
    struct timeval tt;
    gettimeofday(&tt, NULL);
    printf("%d\n", tt.tv_usec);

    fd_set fds;
    usleep(100000000);
                bzero(buffer, sizeof(buffer));
                sprintf(buffer, "terminate\n");
                write(out_fd, buffer, strlen(buffer));
    while (1) {
        FD_ZERO(&fds);
        FD_SET(pipe_from_sender[0], &fds);
        // if sender send us an ordinary
        if (select(pipe_from_sender[0] + 1, &fds, 0, 0, NULL) > 0) {
            bzero(buffer, sizeof(buffer));
            int byte = read(pipe_from_sender[0], buffer, sizeof(buffer));
            if (byte < 0) {
                perror("read");
                exit(EXIT_FAILURE);
            } else if (byte == 0) {
                bzero(buffer, sizeof(buffer));
                sprintf(buffer, "terminate\n");
                write(out_fd, buffer, strlen(buffer));
            }
            if (strcmp(buffer, "ordinary\n") == 0) {
                fprintf(stderr, "received ORDINARY\n");
                add_doc(ORDINARY);
                bzero(buffer, sizeof(buffer));
//                sprintf(buffer, "\n");
//                write(out_fd
            }
        }
        // test if there is document remaining undone
        int doc_type = doc_remain_processing();
        // yes
        if (doc_type != -1) {
        }
        // process files
    }
    return EXIT_SUCCESS;
}
