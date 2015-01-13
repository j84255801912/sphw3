#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

#include "include.h"

#define MIN_JUDGE_NUM 1
#define MAX_JUDGE_NUM 12
#define MIN_PLAYER_NUM 8
#define MAX_PLAYER_NUM 16

typedef struct judge {
    int judge_id;
    // 0 for read, 1 for write
    int pipe_to_judge_fd[2];
    int pipe_from_judge_fd[2];
    pid_t pid;
    int busy;
} JUDGE;

typedef struct player {
    int id;
    int score;
} PLAYER;

int main(int argc, char *argv[])
{
    // ./organizer judge_num player_num
    if (argc != 3) {
        fprintf(stderr, "Wrong argument format\n");
        exit(EXIT_FAILURE);
    }

    // handling param judge_num
    char *test = '\0';
    unsigned int judge_num = strtol(argv[1], &test, 10);
    if (*test != '\0') {
        fprintf(stderr, "Please give numeric argument for judge_num\n");
        exit(EXIT_FAILURE);
    } else if (judge_num < MIN_JUDGE_NUM || judge_num > MAX_JUDGE_NUM) {
        fprintf(stderr, "Please enter judge_num : %d <= judge_num <= %d\n", MIN_JUDGE_NUM, MAX_JUDGE_NUM);
        exit(EXIT_FAILURE);
    }

    // handling param player_num
    test = '\0';
    unsigned int player_num = strtol(argv[2], &test, 10);
    if (*test != '\0') {
        fprintf(stderr, "Please give numeric argument for player_num\n");
        exit(EXIT_FAILURE);
    } else if (player_num < MIN_PLAYER_NUM || player_num > MAX_PLAYER_NUM) {
        fprintf(stderr, "Please enter player_num : %d <= player_num <= %d\n", MIN_PLAYER_NUM, MAX_PLAYER_NUM);
        exit(EXIT_FAILURE);
    }

    // initiate judges processes and pipes
    int i, j, a, b, c, d;
    char buffer[MAX_BUFFER_SIZE];

    // init players
    PLAYER players[MAX_PLAYER_NUM];
    for (i = 0; i < player_num; i++) {
        players[i].id = i;
        players[i].score = 0;
    }
    // init judges
    JUDGE judges[MAX_JUDGE_NUM];
    for (i = 0; i < judge_num; i++)
        judges[i].busy = 0;

    /* */
    for (i = 0; i < judge_num; i++) {
        judges[i].judge_id = i + 1;
        // create two-direction pipe first.
        if (pipe(judges[i].pipe_from_judge_fd) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
        if (pipe(judges[i].pipe_to_judge_fd) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
        // successfully create pipe, then fork.
        pid_t cpid = fork();
        if (cpid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (cpid == 0) { // child
            /* GOAL : make judge's stdin & stdout are from/to organizer */
            /* handle to_judge first */
            // close read end
            close(judges[i].pipe_to_judge_fd[1]);
            dup2(judges[i].pipe_to_judge_fd[0], STDIN_FILENO);
            close(judges[i].pipe_to_judge_fd[0]);

            /* handle from_judge */
            // close write end of org to listen from judge
            close(judges[i].pipe_from_judge_fd[0]);
            dup2(judges[i].pipe_from_judge_fd[1], STDOUT_FILENO);
            close(judges[i].pipe_from_judge_fd[1]);

            /* call judge */
            bzero(buffer, sizeof(buffer));
            sprintf(buffer, "%d", judges[i].judge_id);
            // use (char *)0 instead of NULL,
            // due to the fact that in some system NULL is not equal to 0.
            execl("./judge", "./judge", buffer, (char *)0);

            /* should never run to here */
            perror("execl");
            _exit(EXIT_FAILURE);
        } else {
            judges[i].pid = cpid;
            // close read end cuz dont need it.
            close(judges[i].pipe_to_judge_fd[0]);
        }
    } // for (i = 0; i < judge_num; i++)

    int counting = 0;
    /* distribute all possible competitions to idle judges */
    for (a = player_num - 1; a >= 0; a--) {
        for (b = player_num - 1; b > a; b--) {
            for (c = player_num - 1; c > b; c--) {
                for (d = player_num - 1; d > c; d--) {
                    /* find an idle judge */
                    i = 0;
                    /*
                    while (judges[i].busy) {
                        i = (i + 1) % judge_num;
                        fprintf(stderr, "fuck");
                    }
                    */
                    judges[i].busy = 1;
                    fprintf(stderr, "%dth competition start, judge %d is busy now\n", counting, i+1);
                    counting += 1;
                    /* the judge[i] is assigned by these players */
                    // write
                    bzero(buffer, sizeof(buffer));
                    sprintf(buffer, "%d %d %d %d\n", a, b, c, d);
                    write(judges[i].pipe_to_judge_fd[1], buffer, sizeof(buffer));

                    /* deduct points for loser */
                    bzero(buffer, sizeof(buffer));
                    fprintf(stderr, "before read\n");
                    read(judges[i].pipe_from_judge_fd[0], buffer, sizeof(buffer));
                    fprintf(stderr, "after read\n");
                    buffer[strlen(buffer) - 1] = '\0';
                    players[atoi(buffer)].score -= 1;
                    // change the judge's state to idle
                    judges[i].busy = 0;
                } // for d
            } // for c
        } // for b
    } // for a
    for (i = 0; i < judge_num; i++) {
        bzero(buffer, sizeof(buffer));
        sprintf(buffer, "0 0 0 0\n");
        write(judges[i].pipe_to_judge_fd[1], buffer, sizeof(buffer));
    }
    for (i = 0; i < judge_num; i++) {
        int status;
        wait(&status);
//        printf("%d\n", status);
    }
    fprintf(stderr, "\n[RESULT] : %d competition completed\n", counting);
    fprintf(stderr, "\n- scores -\n");
    for (i = 0; i < player_num; i++) {
        fprintf(stderr, "%d %d\n", i, players[i].score);
    }
    return EXIT_SUCCESS;
}
