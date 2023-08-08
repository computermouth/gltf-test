#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "vector.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

// magic number, largest power of 2 that didn't
// give me a linker error :D
#define MAP_MAX 256
uint8_t map[MAP_MAX][MAP_MAX][MAP_MAX] = {0};
uint32_t map_max_x = 0;
uint32_t map_max_y = 0;
uint32_t map_max_z = 0;

typedef struct {
    uint64_t x;
    uint64_t y;
    uint64_t z;
} pos3_t;

vector * mat_vec = NULL;

void get_material_vector(cgltf_material * materials, cgltf_size size) {
    mat_vec = vector_init(sizeof(cgltf_material *));

    for(cgltf_size i = 0; i < size; i++) {
        cgltf_material * mat = &(materials[i]);
        vector_push(mat_vec, &mat);
    }

    fprintf(stderr, "len(mat_vec): %lu\n", vector_size(mat_vec));
}

uint8_t get_material_index(cgltf_material * material) {
    // todo, what if there's 256+ textures?
    size_t len = vector_size(mat_vec);
    for(int i = 0; i < len; i++) {
        cgltf_material ** out = vector_at(mat_vec, i);
        if( material == *out)
            return i + 1;
    }

    return 0;
}

bool getMaterialImageBytes(cgltf_primitive* primitive, unsigned char** imageBytes, size_t* imageSize) {
    if (primitive == NULL) {
        fprintf(stderr, "Primitive pointer is NULL.\n");
        return false;
    }

    cgltf_material* material = primitive->material;

    if (material == NULL) {
        fprintf(stderr, "Primitive does not have a material.\n");
        return false;
    }

    cgltf_pbr_metallic_roughness* pbr = &material->pbr_metallic_roughness;

    if (pbr->base_color_texture.texture == NULL) {
        fprintf(stderr, "Material does not have an associated texture.\n");
        return false;
    }

    cgltf_texture* textureView = pbr->base_color_texture.texture;

    if (textureView->image == NULL) {
        fprintf(stderr, "Texture does not have an associated image.\n");
        return false;
    }

    cgltf_image* image = textureView->image;

    if (image->uri != NULL && strlen(image->uri) > 0) {
        fprintf(stderr, "URI-based images are not supported in this example.\n");
        return false;
    }

    if (image->buffer_view == NULL) {
        fprintf(stderr, "Image does not have a buffer view.\n");
        return false;
    }

    fprintf(stderr, "found image: %s.png\n", image->name);

    *imageBytes = (unsigned char*)(image->buffer_view->buffer->data + image->buffer_view->offset);
    *imageSize = image->buffer_view->size;

    char namebuf[500] = {0};
    sprintf(namebuf, "%s.png", image->name);

    FILE * imgout = fopen(namebuf, "w");
    fwrite(*imageBytes, sizeof(unsigned char), *imageSize, imgout);

    return true;
}

void mapMeshesToBooleanArray(cgltf_data * data) {

    fprintf(stderr, "nodes: %d\n", (int)data->nodes_count);

    for (size_t i = 0; i < data->nodes_count; ++i) {

        cgltf_node * n = &(data->nodes[i]);

        cgltf_float m[16] = {0};
        cgltf_node_transform_world(n, m);

        cgltf_mesh* mesh = n->mesh;
        fprintf(stderr, "mesh: %s\n", mesh->name);

        // Calculate the dimensions of the primitive
        int minPos[3] = {INT_MAX, INT_MAX, INT_MAX};
        int maxPos[3] = {INT_MIN, INT_MIN, INT_MIN};

        if (mesh->primitives_count > 1)
            fprintf(stderr, "W: primitive_count > 1\n");

        uint8_t texture_id = 0;

        // Iterate over the primitives of the mesh
        for (size_t j = 0; j < mesh->primitives_count; ++j) {

            cgltf_primitive* primitive = &mesh->primitives[j];

            if (primitive->material) {
                texture_id = get_material_index(primitive->material);
            } else {
                fprintf(stderr, "W: primitive has no texture\n");
                continue;
            }

            if (texture_id == 0) {
                fprintf(stderr, "W: texture_id is 0\n");
                continue;
            }

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

                // Calculate the integer indices of the transformed vertex position
                int posX = (int)transformedX;
                int posY = (int)transformedY;
                int posZ = (int)transformedZ;

                // Update the minimum and maximum positions
                minPos[0] = (int)fmin(minPos[0], posX);
                minPos[1] = (int)fmin(minPos[1], posY);
                minPos[2] = (int)fmin(minPos[2], posZ);

                maxPos[0] = (int)fmax(maxPos[0], posX);
                maxPos[1] = (int)fmax(maxPos[1], posY);
                maxPos[2] = (int)fmax(maxPos[2], posZ);
            }
        }
        
        if( minPos[0] < 0 || minPos[0] >= MAP_MAX
            || minPos[1] < 0 || minPos[1] >= MAP_MAX
            || minPos[2] < 0 || minPos[2] >= MAP_MAX
            || maxPos[0] < 0 || maxPos[0] >= MAP_MAX
            || maxPos[1] < 0 || maxPos[1] >= MAP_MAX
            || maxPos[2] < 0 || maxPos[2] >= MAP_MAX
        ){
            fprintf(stderr, "W: block is outside range of 0 <-> MAP_MAX (%u) -- skipping\n", MAP_MAX);
        }

        // Set the corresponding indices in the boolean map array
        for (int x = minPos[0]; x < maxPos[0]; ++x) {
            for (int y = minPos[1]; y < maxPos[1]; ++y) {
                for (int z = minPos[2]; z < maxPos[2]; ++z) {

                    if (x > map_max_x) map_max_x = x;
                    if (y > map_max_y) map_max_y = y;
                    if (z > map_max_z) map_max_z = z;

                    map[x][y][z] = texture_id;
                }
            }
        }
    }

    // Clean up cgltf resources
    cgltf_free(data);
}

pos3_t get_max_dimensions(cgltf_data * data) {
    
    pos3_t max_xyz = { 0, 0, 0 };

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
                
                if(transformedX < 0 || transformedY < 0 || transformedZ < 0)
                    goto skip_negative;
                
                // todo, if any of the transformed are negative, skip

                // Calculate the integer indices of the transformed vertex position
                uint64_t posX = (uint64_t)transformedX;
                uint64_t posY = (uint64_t)transformedY;
                uint64_t posZ = (uint64_t)transformedZ;

                // Update the minimum and maximum positions
                mesh_min_xyz.x = (uint64_t)fmin(mesh_min_xyz.x, posX);
                mesh_min_xyz.y = (uint64_t)fmin(mesh_min_xyz.y, posY);
                mesh_min_xyz.z = (uint64_t)fmin(mesh_min_xyz.z, posZ);

                mesh_max_xyz.x = (uint64_t)fmax(mesh_max_xyz.x, posX);
                mesh_max_xyz.y = (uint64_t)fmax(mesh_max_xyz.y, posY);
                mesh_max_xyz.z = (uint64_t)fmax(mesh_max_xyz.z, posZ);
            }
        }

        // find the largest index in the mesh, and check against max
        for (uint64_t x = mesh_min_xyz.x; x < mesh_max_xyz.x; ++x) {
            for (uint64_t y = mesh_min_xyz.y; y < mesh_max_xyz.y; ++y) {
                for (uint64_t z = mesh_min_xyz.z; z < mesh_max_xyz.z; ++z) {
                    if (x > max_xyz.x) max_xyz.x = x;
                    if (y > max_xyz.y) max_xyz.y = y;
                    if (z > max_xyz.z) max_xyz.z = z;
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
    
    pos3_t max_p = get_max_dimensions(data);
    fprintf(stderr, "max: { .x = %lu, .y = %lu, .z = %lu }\n", max_p.x, max_p.y, max_p.z);

    get_material_vector(data->materials, data->materials_count);

    // Call the function to map the meshes to the boolean array
    mapMeshesToBooleanArray(data);
    
    fprintf(stdout, "uint8_t %s[%d][%d][%d] = {\n", argv[1], map_max_x + 1, map_max_y + 1, map_max_z + 1);
    for(int x = 0; x < map_max_x + 1; x++){
    fprintf(stdout, "   {\n");
        for(int y = 0; y < map_max_y + 1; y++){
    fprintf(stdout, "       {");
            for(int z = 0; z < map_max_z + 1; z++){
    fprintf(stdout, "%u,", map[x][y][z]);
            }
    fprintf(stdout, "},\n");
        }
    fprintf(stdout, "   },\n");
    }
    fprintf(stdout, "};\n");
    
    
    vector_free(mat_vec);

    return 0;
}
