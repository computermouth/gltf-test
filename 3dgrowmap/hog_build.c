
#include <stdint.h>
#include <stdio.h>

#define MAX_MAP 1600

uint8_t map[MAX_MAP][MAX_MAP][MAX_MAP] = { 0 };

int main(){
    
    uint8_t textures[4] = {1, 2, 3, 4};
    
    char buffer[100] = { 0 };
    
    for(int x = 0; x < 1600; x++)
        for(int y = 0; y < 1600; y++)
            for(int z = 0; z < 1600; z++){
                // if(y == 1599 && z == 1599)
                //     printf("%d|%d|%d\n", x, y, z);
                map[x][y][z] = textures[x % 4];
            }
    
    printf("map[1599][1599][1599]: %u\n", map[1599][1599][1599]);
    
    printf("map[0][0][0]: %u\n", map[0][0][0]);
    
    return 0;
}