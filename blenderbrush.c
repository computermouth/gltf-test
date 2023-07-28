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
#define MAP_MAX 1024
uint8_t map[MAP_MAX][MAP_MAX][MAP_MAX] = {0};
uint32_t map_max_x = 0;
uint32_t map_max_y = 0;
uint32_t map_max_z = 0;

vector * mat_vec = NULL;

void get_material_vector(cgltf_material * materials, cgltf_size size) {
    mat_vec = vector_init(sizeof(cgltf_material *));

    for(cgltf_size i = 0; i < size; i++) {
        cgltf_material * mat = &(materials[i]);
        vector_push(mat_vec, &mat);
    }

    printf("len(mat_vec): %lu\n", vector_size(mat_vec));
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
        printf("Primitive pointer is NULL.\n");
        return false;
    }

    cgltf_material* material = primitive->material;

    if (material == NULL) {
        printf("Primitive does not have a material.\n");
        return false;
    }

    cgltf_pbr_metallic_roughness* pbr = &material->pbr_metallic_roughness;

    if (pbr->base_color_texture.texture == NULL) {
        printf("Material does not have an associated texture.\n");
        return false;
    }

    cgltf_texture* textureView = pbr->base_color_texture.texture;

    if (textureView->image == NULL) {
        printf("Texture does not have an associated image.\n");
        return false;
    }

    cgltf_image* image = textureView->image;

    if (image->uri != NULL && strlen(image->uri) > 0) {
        printf("URI-based images are not supported in this example.\n");
        return false;
    }

    if (image->buffer_view == NULL) {
        printf("Image does not have a buffer view.\n");
        return false;
    }

    printf("found image: %s.png\n", image->name);

    *imageBytes = (unsigned char*)(image->buffer_view->buffer->data + image->buffer_view->offset);
    *imageSize = image->buffer_view->size;

    char namebuf[500] = {0};
    sprintf(namebuf, "%s.png", image->name);

    FILE * imgout = fopen(namebuf, "w");
    fwrite(*imageBytes, sizeof(unsigned char), *imageSize, imgout);

    return true;
}

void mapMeshesToBooleanArray(cgltf_data * data) {

    printf("nodes: %d\n", (int)data->nodes_count);

    for (size_t i = 0; i < data->nodes_count; ++i) {

        cgltf_node * n = &(data->nodes[i]);

        cgltf_float m[16] = {0};
        cgltf_node_transform_world(n, m);

        cgltf_mesh* mesh = n->mesh;
        printf("mesh: %s\n", mesh->name);

        // Calculate the dimensions of the primitive
        int minPos[3] = {INT_MAX, INT_MAX, INT_MAX};
        int maxPos[3] = {INT_MIN, INT_MIN, INT_MIN};

        if (mesh->primitives_count > 1)
            printf("W: primitive_count > 1\n");

        uint8_t texture_id = 0;

        // Iterate over the primitives of the mesh
        for (size_t j = 0; j < mesh->primitives_count; ++j) {

            cgltf_primitive* primitive = &mesh->primitives[j];

            if (primitive->material) {
                texture_id = get_material_index(primitive->material);
            } else {
                printf("E: primitive has no texture\n");
                continue;
            }

            if (texture_id == 0) {
                printf("W: texture_id is 0\n");
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

        // Set the corresponding indices in the boolean map array
        for (int x = minPos[0]; x < maxPos[0]; ++x) {
            for (int y = minPos[1]; y < maxPos[1]; ++y) {
                for (int z = minPos[2]; z < maxPos[2]; ++z) {

                    if (x >= MAP_MAX || y >= MAP_MAX || z >= MAP_MAX) {
                        printf("E: block is beyond MAP_MAX (%u)\n", MAP_MAX);
                        exit(1);
                    }

                    if (x > map_max_x) map_max_x = x;
                    if (y > map_max_y) map_max_y = y;
                    if (z > map_max_z) map_max_x = z;

                    map[x][y][z] = texture_id;
                }
            }
        }
    }

    // Clean up cgltf resources
    cgltf_free(data);
}

int main(int argc, char * argv[]) {

    if (argc != 2) {
        printf("usage: %s <file.gltf>\n", argv[0]);
        exit(1);
    }

    char * file_path = argv[1];

    // Load the glTF file using cgltf
    cgltf_data* data;
    cgltf_options options = {0};
    cgltf_result result = cgltf_parse_file(&options, file_path, &data);

    if (result != cgltf_result_success) {
        printf("Failed to parse glTF file: %s\n", file_path);
        return 1;
    }

    // Load the glTF file and access the meshes
    result = cgltf_load_buffers(&options, data, file_path);

    if (result != cgltf_result_success) {
        printf("Failed to load buffers\n");
        return 1;
    }

    get_material_vector(data->materials, data->materials_count);

    // Call the function to map the meshes to the boolean array
    mapMeshesToBooleanArray(data);

    // Set the corresponding indices in the boolean map array
    for (int z = 0; z < 3; z++) {
        printf("{\n");
        printf("  {%d,%d,%d}\n", map[0][0][z], map[0][1][z], map[0][2][z]);
        printf("  {%d,%d,%d}\n", map[1][0][z], map[1][1][z], map[1][2][z]);
        printf("  {%d,%d,%d}\n", map[2][0][z], map[0][1][z], map[2][2][z]);
        printf("}\n");
    }

    // vector_free(mat_vec);

    return 0;
}
