
#include <string.h>

#include "src/microtar.h"

int main(){
    
    mtar_t tar;
    mtar_header_t h;
    char *p;
    int rc = 0;
    
    /* Open archive for reading */
    if ( (rc = mtar_open(&tar, "98-102.tar", "r")) )
        goto quit;
    
    /* Print all file names and sizes */
    rc = mtar_read_header(&tar, &h);
    while ( rc == MTAR_ESUCCESS ){
        printf("head: %s -- type: %c -- bytes: %d\n", h.name, h.type, h.size);
        rc = mtar_next(&tar);
        if (rc)
            break;
        rc = mtar_read_header(&tar, &h);
    }
    
    quit:
    
    if (rc != MTAR_ENULLRECORD){
        printf("E: %s\n", mtar_strerror(rc));
    }
    /* Load and print contents of file "test.txt" */
    mtar_find(&tar, "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678", &h);
    p = calloc(1, h.size + 1);
    mtar_read_data(&tar, p, h.size);
    printf("%s", p);
    free(p);
    
    /* Close archive */
    mtar_close(&tar);
    return 0;
}