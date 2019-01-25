#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>

FILE *f;
unsigned char last_char;
int is_last_char_buffered = 0;

void connect_to_service() 
{
    f = fopen("cept.bin", "r");
    printf("opened: %p\n", f);
}

int layer2getc()
{
    if (is_last_char_buffered) {
        is_last_char_buffered = 0;
    } else {
        last_char = fgetc(f);
    }
    return last_char;
}

void layer2ungetc()
{
    is_last_char_buffered = 1;
}

void layer2write(unsigned char *s, unsigned int len)
{
}
