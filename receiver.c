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
// indicate the processing document.
int ptr[2];
int doc_num[3];
float doc_time[3][NUM_DOCUMENTS];
int times[3] = {ORDINARY_TIME, URGENT_TIME, VERY_URGENT_TIME};
int child_pid;
int serial_num = 1;

void init(void)
{
    int i, j;
    for (i = 0; i < 3; i++) {
        doc_num[i] = 0;
        for (j = 0; j < NUM_DOCUMENTS; j++)
            doc_time[i][j] = 0.0;
    }

    // init ptr
    ptr[0] = -1;
    ptr[1] = -1;

    // init child_pid
    child_pid = -1;
}

int add_doc(int type)
{
    doc_time[type][doc_num[type]] = times[type];
    doc_num[type]++;
    return type;
}

void finish_doc(void)
{
    // shift the doc behind ptr 1 unit.
    int i;
    int max_num = doc_num[ptr[0]];
    char buffer[BUFFER_SIZE];
    for (i = ptr[1]; i < max_num - 1; i++)
        doc_time[ptr[0]][i] = doc_time[ptr[0]][i + 1];
    doc_time[ptr[0]][max_num - 1] = 0;

    // write to log
    bzero(buffer, sizeof(buffer));
    sprintf(buffer, "finish %d %d\n", ptr[0], serial_num);
    write(out_fd, buffer, strlen(buffer));

    // signal the writer
    if (ptr[0] == ORDINARY)
        kill(child_pid, SIGINT);
    else if (ptr[0] == URGENT)
        kill(child_pid, SIGUSR1);
    else if (ptr[0] == VERY_URGENT)
        kill(child_pid, SIGUSR2);

    // refresh the doc_num
    doc_num[ptr[0]]--;

    // reset ptr
    ptr[0] = -1;
    ptr[1] = -1;
}

// if ptr = -1:
//      return
// else
//      find a doc that is undone
//      else
//          return ptr = -1
void get_doc(void)
{
    if (ptr[0] == -1 && ptr[1] == -1)
        return;
    int i, j;
    for (i = 2; i >= 0; i--)
        for (j = 0; j < doc_num[i]; j++)
            // only consider docs whose priority is higher than the ptr
            if (i > ptr[0] && doc_time[i][j] != 0) {
                ptr[0] = i;
                ptr[1] = j;
                return;
            }

    // no doc available
    ptr[0] = -1;
    ptr[1] = -1;
}

void sig_handler(int signo)
{
    char buffer[BUFFER_SIZE];
    if (signo == SIGINT) {
        fprintf(stderr, "received SIGINT\n");
        // kill sender and write to receiver_log
        if (child_pid > 0) {
            kill(child_pid, SIGKILL);
            bzero(buffer, sizeof(buffer));
            sprintf(buffer, "terminate\n");
            write(out_fd, buffer, strlen(buffer));
            exit(EXIT_SUCCESS);
        }
    } else if (signo == SIGUSR1) {
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
    } else {
        child_pid = pid;
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
    out_fd = open("receiver_log", O_WRONLY | O_CREAT, 0644);
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

    fd_set fds;
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
                // write terminate and kill itself
                bzero(buffer, sizeof(buffer));
                sprintf(buffer, "terminate\n");
                write(out_fd, buffer, strlen(buffer));
                exit(EXIT_SUCCESS);
            }
            if (strcmp(buffer, "ordinary\n") == 0) {
                fprintf(stderr, "received \n");
                add_doc(ORDINARY);
            }
        }
        // if is free, we distribute CPU to documents.
        get_doc();
        // if there is a new doc
        if (ptr[0] != -1 && ptr[1] != -1) {
            /* deal with the doc */

            // if we just handle this doc,
            int remain_time = doc_time[ptr[0]][ptr[1]];
            if (remain_time == times[ptr[0]]) {
                // write to log
                bzero(buffer, sizeof(buffer));
                sprintf(buffer, "receive %d %d\n", ptr[0], serial_num);
                write(out_fd, buffer, strlen(buffer));
            }

            // struct  timeval{
            //   long  tv_sec;
            //   long  tv_usec; // microsec
            // }；
            struct timeval t1;
            gettimeofday(&t1, NULL);
            int time1 = t1.tv_sec * 1000000 + t1.tv_usec;

            usleep(remain_time);

            struct timeval t2;
            gettimeofday(&t2, NULL);
            int time2 = t2.tv_sec * 1000000 + t2.tv_usec;

            int time_elapse = time2 - time1;
            if (time_elapse >= remain_time) {
                finish_doc();
            }
        } // if (ptr[0] != -1 && ptr[1] != -1) {
    } // while (1)
    return EXIT_SUCCESS;
}
