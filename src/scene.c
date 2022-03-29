#include "scene.h"
#include "renderer.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "cglm/cglm.h"
#include "stb/stb_image.h"

enum ObjVertexFormat {P, PT, PN, PTN};

// load a gltf file
bool load_obj(const char* const filename, struct Scene* scene) {	
	const struct aiScene* ai_scene = aiImportFile(filename, 0); //, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);
	if (!ai_scene || ai_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !ai_scene->mRootNode) {
		return false;
	}
	scene->mesh_count = 0;
	// scene->texture_count = ai_scene->mNumTextures;
	// for now hardcode the texture loading
	struct Texture text = load_texture("../models/viking_room/viking_room.png", 0, 0);	
	scene->texture_count = 1;
	scene->textures = malloc(scene->texture_count * sizeof(struct Texture));
	scene->textures[0] = text;
	// for (int i = 0; i < scene->texture_count; ++i) {
	// 	struct aiTexture* texture = ai_scene->mTextures[i];
	// 	puts(texture->mFilename);
	// 	// texture->
	// }

	process_node(ai_scene->mRootNode, ai_scene, scene);
	aiReleaseImport(ai_scene);
	return true;
}

void process_node(struct aiNode *node, const struct aiScene* ai_scene, struct Scene* scene) {
	if (scene->mesh_count && node->mNumMeshes) {
		struct Mesh* new_meshes = (struct Mesh*) realloc(scene->meshes, sizeof(struct Mesh) * (node->mNumMeshes + scene->mesh_count));
		scene->meshes = new_meshes;

	} else if (node->mNumMeshes > 0) {
		scene->meshes = (struct Mesh*) malloc(sizeof(struct Mesh)*(node->mNumMeshes));
	}
	
	for (int i = 0; i < node->mNumMeshes; ++i) {
		process_mesh(ai_scene->mMeshes[node->mMeshes[i]], ai_scene, &scene->meshes[i+scene->mesh_count]);
	}
	scene->mesh_count = scene->mesh_count + node->mNumMeshes;
	
	for (int i = 0; i < node->mNumChildren; ++i) {
		process_node(node->mChildren[i], ai_scene, scene);
	}
}

void process_mesh(struct aiMesh* ai_mesh, const struct aiScene *scene, struct Mesh* mesh) {
	mesh->vertex_count = ai_mesh->mNumVertices;
	mesh->vertices = (struct Vertex*) malloc(sizeof(struct Vertex) * mesh->vertex_count);

	for (int i = 0; i < mesh->vertex_count; ++i) {
		struct Vertex vertex;
		struct aiVector3D ai_vertex = ai_mesh->mVertices[i];
		vec3 pos = { ai_vertex.x, ai_vertex.y, ai_vertex.z };
		memcpy(&vertex.pos, pos, sizeof(vec3));

		struct aiVector3D ai_normal = ai_mesh->mNormals[i];
		vec3 norm = { ai_normal.x, ai_normal.y, ai_normal.z };
		memcpy(&vertex.normal, norm, sizeof(vec3));

		if (ai_mesh->mTextureCoords[0]) {
            struct aiVector3D ai_texture_coord = ai_mesh->mTextureCoords[0][i];
            vec2 texture = { ai_texture_coord.x, ai_texture_coord.y};
            memcpy(&vertex.tex, texture, sizeof(vec2));
        } else {
            vec2 texture = {};
            memcpy(&vertex.tex, texture, sizeof(vec2));
		}
	 	
		mesh->vertices[i] = vertex;
	}

	mesh->index_count = 0;
	mesh->indices = (unsigned*) malloc(sizeof(uint32_t) * mesh->vertex_count);
	unsigned indices_len = mesh->vertex_count;
	size_t index = 0;
	for (int i = 0; i < ai_mesh->mNumFaces; ++i) {
		struct aiFace face = ai_mesh->mFaces[i];
		mesh->index_count += face.mNumIndices;
		if (mesh->index_count >= indices_len) {
			mesh->indices = (unsigned*) realloc(mesh->indices, sizeof(uint32_t) * 2*indices_len);
			indices_len *= 2;
		}
		for (int j = 0; j < face.mNumIndices; ++j) {
			mesh->indices[index] = face.mIndices[j];
			index+=1;
		}
	}
} 

void destroy_mesh(struct Mesh* mesh) {
	if (mesh->vertex_count) free(mesh->vertices);
	if (mesh->index_count) free(mesh->indices);
}

void destroy_scene(struct Scene scene) {
	if (scene.mesh_count) {
		for (int i = 0; i < scene.mesh_count; ++i) {
			destroy_mesh(&scene.meshes[i]);
		}
		free(scene.meshes);
	}
}

struct Texture load_texture(char* const filename, uint32_t width, uint32_t height) {
	struct Texture text = {
		.width = width,
		.height = height,
		.filepath = filename,
		.loaded = false,
	};
	return text;
	// int tex_width;
	// int tex_height;
	// int tex_channels;

	// stbi_uc* pixels = stbi_load("../models/viking_room/viking_room.png", &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
	// VkDeviceSize image_size = tex_width * tex_height * 4;

	// if (!pixels) {
	// 	exit(EXIT_FAILURE);
	// }

	// VkImageCreateInfo texture_image_info = {
	// 	.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
	// 	.imageType = VK_IMAGE_TYPE_2D,
	// 	.extent.width = (uint32_t) tex_width,
	// 	.extent.height = (uint32_t) tex_height,
	// 	.extent.depth = 1,
	// 	.mipLevels = 1,
	// 	.arrayLayers = 1,
	// 	.format = VK_FORMAT_R8G8B8A8_SRGB,
	// 	.tiling = VK_IMAGE_TILING_OPTIMAL,
	// 	.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	// 	.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
	// 	.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	// 	.samples = VK_SAMPLE_COUNT_1_BIT,
	// };

	// void* texture_data[] = { malloc(image_size) };
	// VkFormat formats[] = { VK_FORMAT_B8G8R8A8_SRGB };
	// VkImageLayout from[] = { VK_IMAGE_LAYOUT_UNDEFINED };
	// VkImageLayout to[] = { VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL };

	// create_images(&r.physical_device, &r.device, 1, &texture_image_info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
	// 	&r.texture_image, &r.texture_mem);
	// staged_buffer_write_to_image(&r.physical_device, &r.device, &r.command_buffer, &r.graphics_queue, 1, &r.texture_image, texture_data, 
	// 	&image_size, formats, from, to, tex_width, tex_height);	

	// VkImageViewCreateInfo tex_view_info = {};
	// tex_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	// tex_view_info.image = r.texture_image;
	// tex_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	// tex_view_info.format = VK_FORMAT_R8G8B8A8_SRGB;
	// tex_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	// tex_view_info.subresourceRange.baseMipLevel = 0;
	// tex_view_info.subresourceRange.levelCount = 1;
	// tex_view_info.subresourceRange.baseArrayLayer = 0;
	// tex_view_info.subresourceRange.layerCount = 1;

	// vkCreateImageView(r.device, &tex_view_info, NULL, &r.texture_image_view);
	// VkPhysicalDeviceProperties properties = {};
	// vkGetPhysicalDeviceProperties(r.physical_device, &properties);

	// VkSamplerCreateInfo texture_sampler_info = {
	// 	.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
	// 	.magFilter = VK_FILTER_LINEAR,
	// 	.minFilter = VK_FILTER_LINEAR,
	// 	.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
	// 	.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
	// 	.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
	// 	.anisotropyEnable = VK_TRUE,
	// 	.maxAnisotropy = properties.limits.maxSamplerAnisotropy,
	// 	.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
	// 	.unnormalizedCoordinates = VK_FALSE,
	// 	.compareEnable = VK_FALSE,
	// 	.compareOp = VK_COMPARE_OP_ALWAYS,
	// 	.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
	// 	.mipLodBias = 0.0f,
	// 	.minLod = 0.0f,
	// 	.maxLod = 0.0f,
	// };
	// vkCreateSampler(r.device, &texture_sampler_info, NULL, &r.texture_sampler);

	// VkDescriptorPoolSize ubo_pool_size = {
	// 	.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	// 	.descriptorCount = 1
	// };
	
	// VkDescriptorPoolSize texture_sampler_size = {
	// 	.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	// 	.descriptorCount = 1
	// };

	// VkDescriptorPoolSize descriptor_pool_sizes[] = { texture_sampler_size };

	// VkDescriptorPoolCreateInfo descriptor_pool_info = {
	// 	.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
	// 	.poolSizeCount = 1,
	// 	.pPoolSizes = descriptor_pool_sizes,
	// 	.maxSets = 1,
	// };
	// vkCreateDescriptorPool(r.device, &descriptor_pool_info, NULL, &r.descriptor_pool);

	// VkDescriptorSetAllocateInfo descriptor_set_alloc_info = {
	// 	.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
	// 	.descriptorPool = r.descriptor_pool,
	// 	.descriptorSetCount = 1,
	// 	.pSetLayouts = &r.descriptor_set_layout,
	// };

	// vkAllocateDescriptorSets(r.device, &descriptor_set_alloc_info, &r.descriptor_set);

	// // VkDescriptorSetBuf

	// VkDescriptorImageInfo texture_buffer_info = {
	// 	.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	// 	.imageView = r.texture_image_view,
	// 	.sampler = r.texture_sampler,
	// };

	// VkWriteDescriptorSet descriptor_write = {
	// 	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	// 	.dstSet = r.descriptor_set,
	// 	.dstBinding = 1,
	// 	.dstArrayElement = 0,
	// 	.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	// 	.descriptorCount = 1,
	// 	.pImageInfo = &texture_buffer_info,
	// 	.pBufferInfo = NULL,
	// };

	// vkUpdateDescriptorSets(r.device, 1, &descriptor_write, 0, NULL);



	// stbi_image_free(pixels);

}
