#include <string.h>

int fetch_data(char *str, int *a)
{
    int count = 0;
    char *ptr;
    ptr = strtok(str, " ");
    while (ptr != NULL) {
        a[count] = atoi(ptr);
        count += 1;
        ptr = strtok(NULL, "");
    }
    return count;
}
