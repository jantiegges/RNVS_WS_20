#include <stdio.h>
#include "foo.h"

int main(int argc, char* argv[1]) {
    printf("Hello world!\n");

    int x = 0;
    x++;
    x++;
    x = addOne(x);
    //printf("%d\n", x);
    //printf("Das Argument war: %s\n", argv[1]);
    return 0;
}
