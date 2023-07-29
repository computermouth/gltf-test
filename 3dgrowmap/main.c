
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_SIZE 2
uint64_t max_x = DEFAULT_SIZE;
uint64_t max_y = DEFAULT_SIZE;
uint64_t max_z = DEFAULT_SIZE;

uint8_t * array3d = NULL;
uint64_t array_size = DEFAULT_SIZE * DEFAULT_SIZE * DEFAULT_SIZE;

typedef struct {
    uint64_t x;
    uint64_t y;
    uint64_t z;
} pos_t;

uint64_t _calc_pos(uint64_t x, uint64_t y, uint64_t z){
    return (z * max_x * max_y ) + (y * max_x ) + x;
}

void _set_val(uint64_t x, uint64_t y, uint64_t z, uint64_t val){
    uint64_t pos = _calc_pos(x, y, z);
    array3d[pos] = val;
}

void _calibrate(uint64_t ox, uint64_t oy, uint64_t oz, uint64_t os){
    for(uint64_t i = os - 1; ; i--){
        uint64_t tmp = i;
        pos_t old = {0};
        old.z = tmp / (ox * oy);
        tmp %= (ox * oy);
        old.y = tmp / (ox);
        tmp %= ox;
        old.x = tmp;
        
        uint64_t mov_val = array3d[i];
        array3d[i] = 0;
        _set_val(old.x, old.y, old.z, mov_val);
        
        if (i == 0)
            return;
    }
}

void insert(uint64_t x, uint64_t y, uint64_t z, uint64_t val){
    uint64_t old_mx = max_x;
    uint64_t old_my = max_y;
    uint64_t old_mz = max_z;
    uint64_t old_sz = array_size;
    
    if (x >= max_x) max_x = x + 1; 
    if (y >= max_y) max_y = y + 1; 
    if (z >= max_z) max_z = z + 1;
    
    if (array_size != max_x * max_y * max_z) {
        // printf("array_size: %lu", array_size);
        array_size = max_x * max_y * max_z;
        // printf(" -- %lu\n", array_size);
        array3d = realloc(array3d, sizeof(uint8_t) * array_size);
        memset(array3d + old_sz, 0, array_size - old_sz);
        _calibrate(old_mx, old_my, old_mz, old_sz);
    }

    _set_val(x, y, z, val);
}

uint64_t read(uint64_t x, uint64_t y, uint64_t z){
    if (x < max_x && y < max_y && z < max_z)
        return array3d[_calc_pos(x, y, z)];
    return 0;
}

int main(){
    
    array3d = calloc(array_size, sizeof(uint8_t));
    
    for(int x = 0; x < 1600; x++)
        for(int y = 0; y < 1600; y++)
            for(int z = 0; z < 1600; z++){
                if(y == 1599 && z == 1599)
                    printf("%d|%d|%d\n", x, y, z);
                insert(x, y, z, (x % 4) + 1);
            }
    
    // insert(0, 0, 0, 10);
    // insert(1, 0, 0, 11);
    // insert(0, 1, 0, 12);
    // insert(1, 1, 0, 13);
    // insert(0, 0, 1, 14);
    // insert(1, 0, 1, 15);
    // insert(0, 1, 1, 16);
    // insert(1, 1, 1, 17);
    
    // for(uint64_t i = 0; i < array_size; i++){
    //     printf("[%lu]: %d\n", i, array3d[i]);
    // }
    
    printf("read(1599,1599,1599): %lu\n", read(1599,1599,1599));
    printf("read(0,0,0): %lu\n", read(0,0,0));
    
}