#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <libgen.h>

#include "vector.h"
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

typedef struct {
    uint64_t x;
    uint64_t y;
    uint64_t z;
} pos3_t;

pos3_t get_max_dimensions(cgltf_data * data) {
    
    pos3_t max_xyz = { 0, 0, 0 };
    // todo, skip entities
    for (size_t i = 0; i < data->nodes_count; ++i) {

        cgltf_node * n = &(data->nodes[i]);

        cgltf_float m[16] = {0};
        cgltf_node_transform_world(n, m);

        cgltf_mesh* mesh = n->mesh;

        // Calculate the dimensions of the primitive
        pos3_t mesh_min_xyz = {UINT64_MAX, UINT64_MAX, UINT64_MAX};
        pos3_t mesh_max_xyz = {0, 0, 0};

        if (mesh->primitives_count > 1)
            fprintf(stderr, "W: primitive_count > 1\n");

        // Iterate over the primitives of the mesh
        for (size_t j = 0; j < mesh->primitives_count; ++j) {

            cgltf_primitive* primitive = &mesh->primitives[j];

            // Access the positions of the primitive
            cgltf_accessor* positionAccessor = primitive->attributes[0].data;
            cgltf_buffer_view* positionView = positionAccessor->buffer_view;
            cgltf_buffer* positionBuffer = positionView->buffer;
            float* positions = (float*)(positionBuffer->data + positionView->offset + positionAccessor->offset);

            size_t positionCount = positionAccessor->count;
            size_t positionStride = positionAccessor->stride / sizeof(float);

            // Apply the transformation matrix to each vertex position
            for (size_t k = 0; k < positionCount; ++k) {
                float x = positions[k * positionStride];
                float y = positions[k * positionStride + 1];
                float z = positions[k * positionStride + 2];

                // Apply the transformation matrix to the vertex position
                float transformedX = x * m[0] + y * m[4] + z * m[8] + m[12];
                float transformedY = x * m[1] + y * m[5] + z * m[9] + m[13];
                float transformedZ = x * m[2] + y * m[6] + z * m[10] + m[14];
                    // fprintf(stderr, "name: %s x: %f y: %f z: %f\n", mesh->name, x, y, z);
                    // fprintf(stderr, "name: %s x: %f y: %f z: %f\n", mesh->name, transformedY, transformedY, transformedZ);
                
                // right-side negative -> cube
                if(transformedX > 0 && transformedY < 0 && transformedZ > 0){
                    fprintf(stderr, "(cube) name: %s x: %f y: %f z: %f\n", mesh->name, transformedX, transformedY, transformedZ);
                    goto skip_negative;
                }
                
                // left-side negative -> entity
                if(transformedX < 0 && transformedY > 0 && transformedZ > 0){
                    fprintf(stderr, "(enty) name: %s x: %f y: %f z: %f\n", mesh->name, transformedX, transformedY, transformedZ);
                    goto skip_negative;
                }
                
                // all-negative -> __ztest
                if(transformedX < 0 && transformedY < 0 && transformedZ < 0){
                    fprintf(stderr, "(ztst) name: %s x: %f y: %f z: %f\n", mesh->name, transformedX, transformedY, transformedZ);
                    goto skip_negative;
                }
                    
            }
        }
        
        // one or more indicies was in negative space
        skip_negative:
            continue;
    }

    // this is used later for alloc size
    // so increase all dimensions by 1
    max_xyz.x += 1;
    max_xyz.y += 1;
    max_xyz.z += 1;
    
    // check if the dimensions are too large
    // todo, test
    uint64_t rollover_xy = max_xyz.x * max_xyz.y;
    if (max_xyz.x != 0 && rollover_xy / max_xyz.x != max_xyz.y){
        // overflowed
        fprintf(stderr, "map is too large to allocate at { %lu, %lu, %lu }", max_xyz.x, max_xyz.y, max_xyz.z);
        exit(1);
    }
    
    uint64_t rollover_z = rollover_xy * max_xyz.z;
    if (max_xyz.y != 0 && rollover_z / rollover_xy != max_xyz.z){
        // overflowed
        fprintf(stderr, "map is too large to allocate at { %lu, %lu, %lu }", max_xyz.x, max_xyz.y, max_xyz.z);
        exit(1);
    }
    
    return max_xyz;
}

bool find_ztest(cgltf_data * data){
    
    for (size_t i = 0; i < data->nodes_count; ++i) {
        cgltf_node * n = &(data->nodes[i]);
        if (strcmp("__ztest", n->name) == 0){
            
            cgltf_float m[16] = {0};
            cgltf_node_transform_world(n, m);
    
            cgltf_mesh* mesh = n->mesh;
            if(!mesh) return false;
            if(mesh->primitives_count != 1) return false;
            
            cgltf_primitive* primitive = &mesh->primitives[0];
            if(!primitive) return false;
            if(primitive->attributes_count == 0) return false;
            if(strcmp(primitive->attributes[0].name, "POSITION") != 0) return false;
            
            // Access the positions of the primitive
            cgltf_accessor* position_accessor = primitive->attributes[0].data;
            cgltf_buffer_view* position_view = position_accessor->buffer_view;
            cgltf_buffer* position_buffer = position_view->buffer;
            float* positions = (float*)(position_buffer->data + position_view->offset + position_accessor->offset);

            size_t position_count = position_accessor->count;
            size_t position_stride = position_accessor->stride / sizeof(float);

            // Apply the transformation matrix to each vertex position
            for (size_t k = 0; k < position_count; ++k) {
                float x = positions[k * position_stride];
                float y = positions[k * position_stride + 1];
                float z = positions[k * position_stride + 2];

                // Apply the transformation matrix to the vertex position
                float transformedX = x * m[0] + y * m[4] + z * m[8] + m[12];
                float transformedY = x * m[1] + y * m[5] + z * m[9] + m[13];
                float transformedZ = x * m[2] + y * m[6] + z * m[10] + m[14];
                
                // skip if negative
                if(transformedX >= 0 || transformedY >= 0 || transformedZ >= 0)
                    return false;
            }
        }
    }
    
    return true;
}

int main(int argc, char * argv[]) {

    if (argc != 2) {
        fprintf(stderr, "usage: %s <file.gltf>\n", argv[0]);
        exit(1);
    }

    char * file_path = argv[1];

    // Load the glTF file using cgltf
    cgltf_data* data;
    cgltf_options options = {0};
    cgltf_result result = cgltf_parse_file(&options, file_path, &data);

    if (result != cgltf_result_success) {
        fprintf(stderr, "Failed to parse glTF file: %s\n", file_path);
        return 1;
    }

    // Load the glTF file and access the meshes
    result = cgltf_load_buffers(&options, data, file_path);

    if (result != cgltf_result_success) {
        fprintf(stderr, "Failed to load buffers\n");
        return 1;
    }
    
    if (!find_ztest(data)){
        fprintf(stderr, "unable to find a node '__ztest' which has all vertices in negative space\n");
        return 1;
    }
    
    pos3_t max_p = get_max_dimensions(data);

    return 0;
}
