#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define LEN 1024

int main(int argc, char **argv)
{
    char op[LEN];
    unsigned long long i = 0;
    clock_t start = clock();
    for (i = 0; i < 1000000000; i++)
    {
        bzero(op, LEN);
        //memset(op,0,sizeof(op));
    }

    clock_t finish = clock();
    printf("we use bzero\n");
    clock_t duration = (double)(finish - start) / CLOCKS_PER_SEC;
    printf("%f seconds\n", duration);
    //printf("we use memset");

    start = clock();
    for (i = 0; i < 1000000000; i++)
    {
        // bzero(op, sizeof(op));
        memset(op, 0, LEN);
    }
    finish = clock();
    // printf("we use bzero");
    printf("we use memset\n");
    duration = (double)(finish - start) / CLOCKS_PER_SEC;
    printf("%f seconds\n", duration);
    return 0;
}
