#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <libgen.h>

#include "vector.h"
#include "microtar.h"
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#define JSON_MAX_TOKENS 64

typedef struct {
    uint64_t x;
    uint64_t y;
    uint64_t z;
} pos3_t;

typedef struct {
    float x;
    float y;
    float z;
} fpos3_t;
    
typedef struct {
    uint8_t texture;
    pos3_t start;
    pos3_t end;
} out_cube_t;
    
typedef struct {
    fpos3_t fpos;
} out_entt_t;

vector * ref_cube_vec = NULL;
vector * ref_entt_vec = NULL;

vector * map_cube_vec = NULL;
vector * map_entt_vec = NULL;

vector * out_cube_vec = NULL;
vector * out_entt_vec = NULL;

vector * mat_vec = NULL;

void get_material_vector(cgltf_material * materials, cgltf_size size) {
    mat_vec = vector_init(sizeof(cgltf_material *));

    // todo, skip entities
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

bool get_material_image_bytes(cgltf_material* material, char ** imageName, unsigned char** imageBytes, size_t* imageSize) {

    if (material == NULL) {
        fprintf(stderr, "Primitive does not have a material.\n");
        return false;
    }

    cgltf_pbr_metallic_roughness* pbr = &material->pbr_metallic_roughness;

    if (pbr->base_color_texture.texture == NULL) {
        fprintf(stderr, "Material does not have an associated texture.\n");
        return false;
    }

    cgltf_texture* texture_view = pbr->base_color_texture.texture;

    if (texture_view->image == NULL) {
        fprintf(stderr, "Texture does not have an associated image.\n");
        return false;
    }

    cgltf_image* image = texture_view->image;

    if (image->uri != NULL && strlen(image->uri) > 0) {
        fprintf(stderr, "URI-based images are not supported in this example.\n");
        return false;
    }

    if (image->buffer_view == NULL) {
        fprintf(stderr, "Image does not have a buffer view.\n");
        return false;
    }

    fprintf(stderr, "found image: %s.png\n", image->name);

    *imageName = image->name;
    *imageBytes = (unsigned char*)(image->buffer_view->buffer->data + image->buffer_view->offset);
    *imageSize = image->buffer_view->size;

    return true;
}

void prep_out_cubes(){
    
    size_t len = vector_size(map_cube_vec);
    cgltf_node ** n = vector_begin(map_cube_vec);
    
    for(size_t i = 0; i < len; i++){
        fprintf(stderr, "pe: %s\n", n[i]->name);
    }
    
}

void map_meshes_to_array(cgltf_data * data, pos3_t max, uint8_t (*arr)[max.x][max.y][max.z]) {

    fprintf(stderr, "nodes: %d\n", (int)data->nodes_count);

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
            cgltf_accessor* position_accessor = primitive->attributes[0].data;
            cgltf_buffer_view* position_view = position_accessor->buffer_view;
            cgltf_buffer* position_buffer = position_view->buffer;
            float* positions = (float*)(position_buffer->data + position_view->offset + position_accessor->offset);

            size_t positionCount = position_accessor->count;
            size_t positionStride = position_accessor->stride / sizeof(float);

            // Apply the transformation matrix to each vertex position
            for (size_t k = 0; k < positionCount; ++k) {
                float x = positions[k * positionStride];
                float y = positions[k * positionStride + 1];
                float z = positions[k * positionStride + 2];

                // Apply the transformation matrix to the vertex position
                float transformedX = x * m[0] + y * m[4] + z * m[8] + m[12];
                float transformedY = x * m[1] + y * m[5] + z * m[9] + m[13];
                float transformedZ = x * m[2] + y * m[6] + z * m[10] + m[14];
                
                // Negative space, shouldn't be found
                if(transformedX < 0 && transformedY < 0 && transformedZ < 0){
                    fprintf(stderr, "W: encountered mesh('%s') in negative space {%ld, %ld, %ld}\n",
                        mesh->name, (int64_t)transformedX, (int64_t)transformedY, (int64_t)transformedZ);
                    vector_push(ref_cube_vec, &n);
                    goto skip_oob;
                }

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
        
        if( //mins
               mesh_min_xyz.x < 0 || mesh_min_xyz.x > max.x
            || mesh_min_xyz.y < 0 || mesh_min_xyz.y > max.y
            || mesh_min_xyz.z < 0 || mesh_min_xyz.z > max.z
            // maxes
            || mesh_max_xyz.x < 0 || mesh_max_xyz.x > max.x
            || mesh_max_xyz.y < 0 || mesh_max_xyz.y > max.y
            || mesh_max_xyz.z < 0 || mesh_max_xyz.z > max.z
        ){
            fprintf(stderr,
                "W: block { %lu, %lu, %lu } - { %lu, %lu, %lu } is outside range of { 0, 0, 0 } <-> { %lu, %lu, %lu } -- skipping\n -- (this should never be seen in practice)",
                mesh_min_xyz.x, mesh_min_xyz.y, mesh_min_xyz.z,
                mesh_max_xyz.x, mesh_max_xyz.y, mesh_max_xyz.z,
                max.x, max.y, max.z
            );
            goto skip_oob;
        }

        // old array packer
        // // Set the corresponding indices in the boolean map array
        // for (uint64_t x = mesh_min_xyz.x; x < mesh_max_xyz.x; ++x)
        //     for (uint64_t y = mesh_min_xyz.y; y < mesh_max_xyz.y; ++y)
        //         for (uint64_t z = mesh_min_xyz.z; z < mesh_max_xyz.z; ++z)
        //             (*arr)[x][y][z] = texture_id;
        
        // vector_push()

        fprintf(stderr, "mesh: %s\n", mesh->name);
        fprintf(stderr, "  - { %lu, %lu, %lu } - { %lu, %lu, %lu }\n",
             mesh_min_xyz.x, mesh_min_xyz.y, mesh_min_xyz.z,
             mesh_max_xyz.x, mesh_max_xyz.y, mesh_max_xyz.z
        );
        
        // one or more indicies was in negative space
        skip_oob:
            continue;
    }

    // Clean up cgltf resources
    // cgltf_free(data);
}

// Function to find the value of a key in the JSON object
const char* findJsonValue(const char* json, const jsmntok_t* tokens, const char* key) {
    for (int i = 0; i < tokens[0].size; ++i) {
        if (tokens[i].type == JSMN_STRING && tokens[i].size > 0 &&
            strncmp(json + tokens[i].start, key, tokens[i].end - tokens[i].start) == 0) {
            return json + tokens[i + 1].start;
        }
    }
    return NULL;
}

bool node_is_cube(cgltf_node * node){
    if(node->extras.data == NULL)
        return false;
    
    jsmn_parser parser = { 0 };
    jsmntok_t tokens[JSON_MAX_TOKENS] = { 0 };
    const char * xjson = node->extras.data;
    
    jsmn_init(&parser);
    int count = jsmn_parse(&parser, xjson, strlen(xjson), tokens, JSON_MAX_TOKENS);
    
    for(int i = 0; i < count; i++){
        if( tokens[i].type == JSMN_STRING
            && tokens[i].size > 0
            && strncmp(xjson + tokens[i].start, "type", tokens[i].end - tokens[i].start) == 0
            && tokens[i + 1].type == JSMN_STRING
            && strncmp(xjson + tokens[i + 1].start, "cube", tokens[i + 1].end - tokens[i + 1].start) == 0
        )
            return true;
    }
    
    return false;
}

bool node_is_entity(cgltf_node * node){
    if(node->extras.data == NULL)
        return false;
    
    jsmn_parser parser = { 0 };
    jsmntok_t tokens[JSON_MAX_TOKENS] = { 0 };
    const char * xjson = node->extras.data;
    
    jsmn_init(&parser);
    int count = jsmn_parse(&parser, xjson, strlen(xjson), tokens, JSON_MAX_TOKENS);
    
    for(int i = 0; i < count; i++){
        if( tokens[i].type == JSMN_STRING
            && tokens[i].size > 0
            && strncmp(xjson + tokens[i].start, "type", tokens[i].end - tokens[i].start) == 0
            && tokens[i + 1].type == JSMN_STRING
            && strncmp(xjson + tokens[i + 1].start, "entity", tokens[i + 1].end - tokens[i + 1].start) == 0
        )
            return true;
    }
    
    return false;
}

typedef enum {
    CLASS_REF,
    CLASS_MAP,
} mesh_class_t;

typedef enum {
    GROUP_INVALID,
    GROUP_CUBE,
    GROUP_ENTT,
} mesh_group_t;

pos3_t group_meshes(cgltf_data * data){
    pos3_t max_xyz = { 0, 0, 0 };
    
    // todo, skip entities
    for (size_t i = 0; i < data->nodes_count; ++i) {
        
        cgltf_node * n = &(data->nodes[i]);
        mesh_group_t mg = GROUP_INVALID;
        mesh_class_t mc = CLASS_MAP;
        
        // determine group
        if(node_is_cube(n))
            mg = GROUP_CUBE;
        else if (node_is_entity(n))
            mg = GROUP_ENTT;
        
        cgltf_float m[16] = {0};
        cgltf_node_transform_world(n, m);

        cgltf_mesh* mesh = n->mesh;

        // Calculate the max dimensions of the primitive(s)
        pos3_t mesh_min_xyz = {UINT64_MAX, UINT64_MAX, UINT64_MAX};
        pos3_t mesh_max_xyz = {0, 0, 0};

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
                
                // if any vertex is ever negative, it's reference, don't include in min/max
                if (transformedX < 0 || transformedY < 0 || transformedZ < 0){
                    mc = CLASS_REF;
                    goto skip_negative;
                }
                
                // if it's an entity, don't expand min/max
                // if it could, an entity off a platform would just fall
                // and trip a underflow
                if (mg == GROUP_ENTT)
                    goto skip_negative;

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
        
        switch (mc) {
            case CLASS_REF: 
                switch (mg) {
                    case GROUP_CUBE:
                        // ref cube
                        vector_push(ref_cube_vec, &n);
                        fprintf(stderr, "ref_cube_vec '%s'\n", n->name);
                        break;
                    case GROUP_ENTT: 
                        // ref entt
                        vector_push(ref_entt_vec, &n);
                        fprintf(stderr, "ref_entt_vec '%s'\n", n->name);
                        break;
                    default:
                        fprintf(stderr, "invalid group for ref node[%lu]('%s')\n", i, n->name);
                        break;
                }
                break;
            case CLASS_MAP: 
                switch (mg) {
                    case GROUP_CUBE:
                        // map cube
                        vector_push(map_cube_vec, &n);
                        fprintf(stderr, "map_cube_vec '%s'\n", n->name);
                        break;
                    case GROUP_ENTT: 
                        // map entt
                        vector_push(map_entt_vec, &n);
                        fprintf(stderr, "map_entt_vec '%s'\n", n->name);
                        break;
                    default:
                        fprintf(stderr, "invalid group for map node[%lu]('%s')\n", i, n->name);
                        break;
                }
                break;
            default: 
                fprintf(stderr, "invalid class for node[%lu]('%s')\n", i, n->name);
                break;
        }
        
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
    // char * tar_path = argv[2];

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
    
    // validate y/z axis
    if (!find_ztest(data)){
        fprintf(stderr, "unable to find a node '__ztest' which has all vertices in negative space\n");
        return 1;
    }
    
    ref_cube_vec = vector_init(sizeof(cgltf_node *));
    map_cube_vec = vector_init(sizeof(cgltf_node *));
    
    ref_entt_vec = vector_init(sizeof(cgltf_node *));
    map_entt_vec = vector_init(sizeof(cgltf_node *));
    
    out_cube_vec = vector_init(sizeof(out_cube_t));
    out_entt_vec = vector_init(sizeof(out_entt_t));
    
    pos3_t max_p = group_meshes(data);
    uint8_t (*arr)[max_p.x][max_p.y][max_p.z] = calloc(1, sizeof *arr);
    fprintf(stderr, "max: { .x = %lu, .y = %lu, .z = %lu }\n", max_p.x, max_p.y, max_p.z);
    if(arr == NULL){
        printf("E: failed to alloc %luB\n", sizeof *arr);
        exit(1);
    }
    
    // uint8_t (*map_bytes)[max_p.x * max_p.y * max_p.z] = (uint8_t (*)[])(*arr);
    
    prep_out_cubes();
    
    goto cleanup;
    
    get_material_vector(data->materials, data->materials_count);

    // Call the function to map the meshes to the boolean array
    map_meshes_to_array(data, max_p, arr);
    
    // todo, msgpack
    
    // C source print
    // fprintf(stdout, "uint8_t %s[%lu][%lu][%lu] = {\n", argv[1], max_p.x, max_p.y, max_p.z);
    // for(int x = 0; x < max_p.x; x++){
    //     fprintf(stdout, "\t{\n");
    //     for(int y = 0; y < max_p.y; y++){
    //         fprintf(stdout, "\t\t{");
    //         for(int z = 0; z < max_p.z; z++){
    //             fprintf(stdout, "%u", (*arr)[x][y][z]);
    //             if (z != max_p.z - 1)
    //                 fprintf(stdout, ",");
    //         }
    //         fprintf(stdout, "},\n");
    //     }
    //     fprintf(stdout, "\t},\n");
    // }
    // fprintf(stdout, "};\n");
    
    // char * file_base = basename(argv[1]);
    // char * gltf_subs = strstr(file_base, ".gl");
    // if (gltf_subs == NULL){
    //     fprintf(stderr, "couldn't find a .{gltf,glb} suffix");
    //     return 1;
    // }
    // gltf_subs[0] = '\0';
    
    // {
    //     mtar_t tar;
        
    //     /* Open archive for reading in old ini */
    //     int open_err = mtar_open(&tar, tar_path, "r");
    //     if (open_err && open_err != MTAR_EOPENFAIL){
    //         fprintf(stderr, "E: failed to open %s -- %s\n", tar_path, mtar_strerror(open_err));
    //         exit(1);
    //     }
        
    //     // maps/ + strlen(name) + .map + \0
    //     char * map_name = calloc(strlen(file_base) + 5 + 4 + 1, sizeof(char));
    //     char * ini_name = calloc(strlen(file_base) + 5 + 4 + 1, sizeof(char));
    //     char * ini_data = calloc(500, sizeof(char));
    //     char * ini_data_end = ini_data;
        
    //     sprintf(map_name, "maps/%s.map", file_base);
    //     // todo, one manifest file for all maps
    //     sprintf(ini_name, "maps/%s.ini", file_base);
        
    //     if (!open_err) {
    //         mtar_header_t ini_header;
    //         int err = mtar_find(&tar, ini_name, &ini_header);
    //         if (err && err != MTAR_ENOTFOUND){
    //             fprintf(stderr, "E: failed to search for %s -- %s\n", ini_name, mtar_strerror(err));
    //             exit(1);
    //         }
    //         // found it
    //         if (!err) {
    //             ini_data = realloc(ini_data, ini_header.size + 500 + 1);
    //             // overkill, could just set new data
    //             // but easier math c:
    //             memset(ini_data, 0, ini_header.size + 500 + 1);
    //             err = mtar_read_data(&tar, ini_data, ini_header.size);
    //             if (err){
    //                 fprintf(stderr, "E: failed to search for %s\n", ini_name);
    //                 exit(1);
    //             }
    //             // fprintf(stderr, "ini_data: %s\n", ini_data);
    //             ini_data_end = &(ini_data[ini_header.size]);
    //             // fprintf(stderr, "ini_data_end: %s\n", ini_data_end);
    //         }
            
    //         mtar_close(&tar);
    //     }
    //     tar = (mtar_t){ 0 };
    //     /* Open archive for writing(append) */
    //     mtar_open(&tar, tar_path, "w");
        

    //     sprintf(ini_data_end, "\n[%s]\nx = %lu\ny = %lu\nz = %lu\n", file_base, max_p.x, max_p.y, max_p.z);
        
    //     /* Write map file */
    //     int err = mtar_write_file_header(&tar, map_name, sizeof *map_bytes);
    //     if (err){
    //         fprintf(stderr, "E: failed to search for %s -- %s\n", ini_name, mtar_strerror(err));
    //         exit(1);
    //     }
    //     err = mtar_write_data(&tar, *map_bytes, sizeof *map_bytes);
    //     if (err){
    //         fprintf(stderr, "E: failed to search for %s -- %s\n", ini_name, mtar_strerror(err));
    //         exit(1);
    //     }
    //     /* Write man file */
    //     err = mtar_write_file_header(&tar, ini_name, strlen(ini_data));
    //     if (err){
    //         fprintf(stderr, "E: failed to search for %s -- %s\n", ini_name, mtar_strerror(err));
    //         exit(1);
    //     }
    //     err = mtar_write_data(&tar, ini_data, strlen(ini_data));
    //     if (err){
    //         fprintf(stderr, "E: failed to search for %s -- %s\n", ini_name, mtar_strerror(err));
    //         exit(1);
    //     }
        
    //     char * mat_name = calloc(100, sizeof(char));
    //     size_t mat_count = vector_size(mat_vec);
    //     for(size_t i = 0; i < mat_count; i++){
    //         cgltf_material ** mat_p = vector_at(mat_vec, i);
    //         cgltf_material * mat_m = *mat_p;
    //         memset(mat_name, 0, 100);
            
    //         char * img_name = NULL;
    //         unsigned char * img_bytes = NULL;
    //         size_t img_size = 0;
    //         get_material_image_bytes(mat_m, &img_name, &img_bytes, &img_size);
    //         snprintf(mat_name, 99, "txtr/%s.png", img_name);
            
    //         /* Write image file */
    //         mtar_write_file_header(&tar, mat_name, img_size);
    //         mtar_write_data(&tar, img_bytes, img_size);
    //     }
        
    //     /* Finalize -- this needs to be the last thing done before closing */
    //     mtar_finalize(&tar);
        
    //     /* Close archive */
    //     mtar_close(&tar);
        
    //     free(map_name);
    //     free(ini_name);
    //     free(ini_data);
    //     free(mat_name);
    // }
    
    
    // // byte map print
    // fprintf(stdout, "%lu,%lu,%lu\n", max_p.x, max_p.y, max_p.z);
    for(int x = 0; x < max_p.x; x++)
        for(int y = 0; y < max_p.y; y++)
            for(int z = 0; z < max_p.z; z++)
                fprintf(stdout, "%c", (*arr)[x][y][z]);
    
    cleanup:
    
    free(arr);
    vector_free(mat_vec);
    cgltf_free(data);

	return 0;
}
