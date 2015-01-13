#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "include.h"

int main(int argc, char *argv[])
{
    char buffer[MAX_BUFFER_SIZE];
    bzero(buffer, sizeof(buffer));
    sprintf(buffer, "judge%s_%s.FIFO", argv[1], argv[2]);
    int infd = open(buffer, O_RDONLY);
    if (infd == -1) {
        perror("open infd, player");
        exit(EXIT_FAILURE);
    }
    /* get cards from judge */
    bzero(buffer, sizeof(buffer));
    read(infd, buffer, sizeof(buffer));
    // trim '\n'
    buffer[strlen(buffer) - 1] = '\0';

    int cards[15], card_count = 0;
    // initialize cards with -1's for the later elimination easier.
    int i, j;
    for (i = 0; i < 15; i++)
        cards[i] = -1;

    /* extract cards from buffer */
//    card_count = fetch_data(buffer, cards);
    char *ptr;
    ptr = strtok(buffer, " ");
    while (ptr != NULL) {
        cards[card_count] = atoi(ptr);
        card_count += 1;
        ptr = strtok(NULL, " ");
    }
    /* eliminate two same cards */

    // let the same two cards drop
    for (i = 0; i < card_count - 1; i++)
        if (cards[i] != -1)
            for (j = i + 1; j < card_count; j++)
                if (cards[i] == cards[j]) {
                    cards[i] = -1;
                    cards[j] = -1;
                    break;
                }
    // let the cards fill the front of cards array
    for (i = 0; i < card_count - 1; i++) {
        if (cards[i] == -1)
            for (j = i + 1; j < card_count; j++)
                if (cards[j] != -1) {
                    cards[i] = cards[j];
                    cards[j] = -1;
                    break;
                }
    }
    // count the final card counts
    i = 0;
    while (cards[i] != -1)
        i += 1;
    card_count = i;

    /* open fifo to judge */
    bzero(buffer, sizeof(buffer));
    sprintf(buffer, "judge%s.FIFO", argv[1]);
    int outfd = open(buffer, O_WRONLY);
    if (outfd == -1) {
        perror("open outfd, player");
        exit(EXIT_FAILURE);
    }

    /* write card_count to judge */
    bzero(buffer, sizeof(buffer));
    // [player_index] [random_key] [number_of_cards]
    sprintf(buffer, "%s %s %d\n", argv[2], argv[3], card_count);
//    fprintf(stderr, "%s", buffer);
    write(outfd, buffer, sizeof(buffer));

    /* the game starts */
    while (1) {
        bzero(buffer, sizeof(buffer));
        read(infd, buffer, sizeof(buffer));
        /* get card_count of the next player */

        // trim '\n'
        buffer[strlen(buffer) - 1] = '\0';
        // case choose card to draw
        if (buffer[0] == '<') {
            int num_of_cards_next;
            num_of_cards_next = atoi(&buffer[2]);
            bzero(buffer, sizeof(buffer));
            sprintf(buffer, "%s %s %d\n", argv[2], argv[3], rand() % num_of_cards_next);
            write(outfd, buffer, sizeof(buffer));
        } else if (buffer[0] == '>') { // case my card is to be drawn
            int card_id_been_drawn;
            card_id_been_drawn = atoi(&buffer[2]);
            bzero(buffer, sizeof(buffer));
            sprintf(buffer, "%s %s %d\n", argv[2], argv[3], cards[card_id_been_drawn]);
            write(outfd, buffer, sizeof(buffer));
            // rearrange the card deck
            for (i = card_id_been_drawn; i < card_count - 1; i++)
                cards[i] = cards[i + 1];
//            for (i = card_count - 1; i < 15; i++)
//                cards[i] = -1;
            cards[card_count - 1] = -1;
            card_count -= 1;
        } else { // get a card from the next
            int the_card_number;
            the_card_number = atoi(buffer);
            // put the new card into the card deck
            cards[card_count] = the_card_number;
            card_count += 1;
            int eliminated = 0;
            // let the same two cards drop
            for (i = 0; i < card_count - 1; i++) {
                if (eliminated == 1)
                    break;
                if (cards[i] != -1)
                    for (j = i + 1; j < card_count; j++)
                        if (cards[i] == cards[j]) {
                            cards[i] = -1;
                            cards[j] = -1;
                            eliminated = 1;
                            break;
                        }
            }
            // if some cards eliminated
            if (eliminated) {
                // let the cards fill the front of cards array
                for (i = 0; i < card_count - 1; i++) {
                    if (cards[i] == -1)
                        for (j = i + 1; j < card_count; j++)
                            if (cards[j] != -1) {
                                cards[i] = cards[j];
                                cards[j] = -1;
                                break;
                            }
                }
                // count the final card counts
                i = 0;
                while (cards[i] != -1)
                    i += 1;
                card_count = i;
            }
            bzero(buffer, sizeof(buffer));
            sprintf(buffer, "%s %s %d\n", argv[2], argv[3], eliminated);
            write(outfd, buffer, sizeof(buffer));
        } // else {
    } // while (1) {
    return EXIT_SUCCESS;
}
