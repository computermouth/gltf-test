
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "xxhash.h"
#include "map.h"

#define MAX_MAP 1600

uint64_t i = 0;

uint64_t hash(const void * key){
    // printf("key: %p\n", key);
    size_t len = strlen(key);
    uint64_t h = XXH64(key, len, 0);
    return h;
}

int main(){
    
    //                 hash as u64     uint8_t texture
    map * m = map_init(sizeof(char) * 16, sizeof(uint8_t), hash);
    
    uint8_t textures[4] = {1, 2, 3, 4};
    
    char buffer[16] = { 0 };
    
    for(int x = 0; x < 1600; x++)
        for(int y = 0; y < 1600; y++)
            for(int z = 0; z < 1600; z++){
                memset(buffer, 0, 16);
                sprintf(buffer, "%d|%d|%d", x, y, z);
                // printf("buf[%lu]: %p\n", strlen(buffer), buffer);
                if(y == 1599 && z == 1599)
                    printf("%s\n", buffer);
                map_insert(m, buffer, &(textures[x % 4]));
            }
    
    uint8_t * texture = map_get(m, "[1599][1599][1599]");
    printf("*[1599][1599][1599]: %u\n", *texture);
    
    texture = map_get(m, "[0][0][0]");
    printf("*[0][0][0]: %u\n", *texture);
    
    return 0;
}