#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
 
int main(int argc, char * argv[])
{
    if(argc != 4){
        printf("usage: %s [max_x] [max_y] [max_z]\n", argv[0]);
        exit(1);
    }
    
    int max_x = atoi(argv[1]);
    int max_y = atoi(argv[2]);
    int max_z = atoi(argv[3]);
    if (max_x <= 0 || max_y <= 0 || max_z <= 0){
        printf("E: maxes must be larger than 0\n");
        exit(1);
    }
    
    uint8_t (*arr)[max_x][max_z][max_y] = malloc(sizeof *arr);
    if(arr == NULL){
        printf("E: failed to alloc %lu\n", sizeof *arr);
        exit(1);
    }
    
    printf("alloc'd %luB\n", sizeof *arr);
    
    uint8_t count = 0;
    for (uint64_t i = 0; i < max_x; i++)
        for (uint64_t j = 0; j < max_y; j++)
            for (uint64_t k = 0; k < max_z; k++)
                (*arr)[i][j][k] = count++;
 
    uint64_t last_x = max_x - 1;
    uint64_t last_y = max_y - 1;
    uint64_t last_z = max_z - 1;
    printf("arr[%lu][%lu][%lu]: %u\n",
        last_x, last_y, last_z, (*arr)[last_x][last_y][last_z]);
    free(arr);
     
    return 0;
}
