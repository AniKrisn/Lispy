#include <stdio.h>

int main (int argc, char **argv) {
    printf("%d\n", argc);

    for (int i=0; i<argc; i++) {
        printf("%d\n", argv[i]);
    }

}