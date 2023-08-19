
// turn this into
// POSITION
// TEXCOORD_0 (UV)
// abstraction
{

	// Find the index of the TEXCOORD_0 attribute
	cgltf_attribute* uvAttribute = NULL;
	for (cgltf_size i = 0; i < mesh->primitives[0].attributes_count; ++i) {
		if (strcmp(mesh->primitives[0].attributes[i].name, "TEXCOORD_0") == 0) {
			uvAttribute = &mesh->primitives[0].attributes[i];
			break;
		}
	}

	if (uvAttribute) {
		cgltf_accessor* uvAccessor = uvAttribute->data;
		cgltf_buffer_view* uvBufferView = uvAccessor->buffer_view;
		cgltf_buffer* uvBuffer = uvBufferView->buffer;

		// Access UV data
		size_t uvCount = uvAccessor->count;
		size_t uvOffset = uvAccessor->offset + uvBufferView->offset;
		float* uvData = (float*)(uvBuffer->data + uvOffset);

		// Now you can loop through uvData to access individual UV coordinates
		for (size_t i = 0; i < uvCount; ++i) {
			float u = uvData[2 * i];
			float v = uvData[2 * i + 1];

			// Use u and v as needed
			// For example, you can print them:
			printf("UV %zu: u = %f, v = %f\n", i, u, v);
		}
	} else {
		// Handle the case where TEXCOORD_0 attribute is not found
	}

}

// GET FROM ABOVE FUNCTION

// Access the POSITION attribute from the base mesh
cgltf_attribute* basePositionAttr = &mesh->attributes[0];
cgltf_accessor* basePositionAccessor = basePositionAttr->data;

// Access the UV attribute from the base mesh
cgltf_attribute* baseUVAttr = &mesh->attributes[1]; // Assuming TEXCOORD_0 is the second attribute
cgltf_accessor* baseUVAccessor = baseUVAttr->data;

for (size_t i = 0; i < mesh->morph_targets_count; ++i) {
    cgltf_morph_target* morphTarget = &mesh->morph_targets[i];

    // Access the POSITION attribute from the morph target
    cgltf_attribute* morphPositionAttr = &morphTarget->attributes[0];
    cgltf_accessor* morphPositionAccessor = morphPositionAttr->mappings[i].accessor;

    // Iterate through each vertex and apply morphing
    for (size_t vertexIndex = 0; vertexIndex < basePositionAccessor->count; ++vertexIndex) {
        // Get vertex position data from base and morph target accessors
        cgltf_float basePosition[3];
        cgltf_float morphPosition[3];
        
        cgltf_accessor_read_float(basePositionAccessor, vertexIndex, basePosition, 3);
        cgltf_accessor_read_float(morphPositionAccessor, vertexIndex, morphPosition, 3);

        // Calculate the morphed position (in this case, just using the morph target data)
        // where adding morphPosition is the same as adding (morphPosition[j] * 1.0f)
        // assuming the span is on a range from 0.0f <-> 1.0f
        cgltf_float finalPosition[3];
        for (int j = 0; j < 3; ++j) {
            finalPosition[j] = basePosition[j] + morphPosition[j];
        }

        // Now you have the finalPosition and uv for the current vertex
        // You can use this data for animation or rendering
    }
}
