#include "scene.h"
#include "alloc.h"
#include "cglm/mat4.h"
#include "cglm/types.h"
#include "renderer.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "cglm/cglm.h"
#include "stb/stb_image.h"
#include "util.h"
#include "vulkan/vulkan_core.h"
#include <stdint.h>

// returns the index of the texture of a given type for a given material
// returns -1 if that type does not exist
// if a new texture was created, adds it to the scene
static int get_texture_types(enum aiTextureType texture_type, struct aiMaterial* mat, struct Scene* scene, int* textures_length, const char* dir) {
	// this is only a relative path such as ./texture.png	
	struct aiString texture_path;

	// this store the absolute path to the texture
	char full_texture_path[1024];
	if (aiGetMaterialTexture(mat, texture_type, 0, &texture_path, NULL, NULL, NULL, NULL, NULL, NULL) != aiReturn_SUCCESS) {
		return -1;
	}
	strcpy(full_texture_path, dir);
	strcat(full_texture_path, texture_path.data);
	for (int j = 0; j < scene->texture_count; ++j) {
		if (strcmp(full_texture_path, scene->textures[j].filepath) == 0) {
			return j;
		}
	}
	
	scene->texture_count += 1;
	if (scene->texture_count > *textures_length) {
		*textures_length *= 2;
		scene->textures = realloc(scene->textures, *textures_length*sizeof(struct Texture));
	}
	struct Texture new_texture;
	strncpy(new_texture.filepath, full_texture_path, 1024);
	scene->textures[scene->texture_count-1] = new_texture;
	return scene->texture_count - 1;
}

// load a gltf file
// it is important that dir has a trailing slash
bool load_obj(const char* const dir, const char* const file_name, struct Scene* scene) {	
	char full_path[1024];
	strcpy(full_path, dir);
	strcat(full_path, file_name);
	const struct aiScene* ai_scene = aiImportFile(full_path, 0); //aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);
	if (!ai_scene || ai_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !ai_scene->mRootNode) {
		return false;
	}
	
	scene->material_count = ai_scene->mNumMaterials;
	scene->materials = malloc(scene->material_count * sizeof(struct Material));

	int textures_length = 5;

	scene->textures = malloc(textures_length * sizeof(struct Texture));
	scene->texture_count = 0;

	for (int i = 0; i < ai_scene->mNumMaterials; ++i) {
		struct Material material;
		struct aiMaterial* ai_material = ai_scene->mMaterials[i];
		material.diffuse = get_texture_types(aiTextureType_DIFFUSE, ai_material, scene, &textures_length, dir);
		material.emissive = get_texture_types(aiTextureType_EMISSIVE, ai_material, scene, &textures_length, dir);
		scene->materials[i] = material;
	}

	scene->mesh_count = ai_scene->mNumMeshes;
	scene->meshes = malloc(sizeof(struct Mesh) * scene->mesh_count);

	for (int i = 0; i < scene->mesh_count; ++i) {
		struct aiMesh* m = ai_scene->mMeshes[i];
		struct Mesh mesh;
		process_mesh(m, ai_scene, &mesh, scene);

		scene->meshes[i] = mesh;
	}

	scene->root = malloc(sizeof(struct Node));

	process_node(ai_scene->mRootNode, scene->root, ai_scene, scene);
	aiReleaseImport(ai_scene);
	return true;
}

void process_node(struct aiNode *ai_node, struct Node* node, const struct aiScene* ai_scene, struct Scene* scene) {
	struct aiMatrix4x4 transform =  ai_node->mTransformation;
	struct aiMatrix4x4* from = &transform;
	mat4 to;
	to[0][0] = (float)from->a1; to[0][1] = (float)from->b1;  to[0][2] = (float)from->c1; to[0][3] = (float)from->d1;
    to[1][0] = (float)from->a2; to[1][1] = (float)from->b2;  to[1][2] = (float)from->c2; to[1][3] = (float)from->d2;
    to[2][0] = (float)from->a3; to[2][1] = (float)from->b3;  to[2][2] = (float)from->c3; to[2][3] = (float)from->d3;
    to[3][0] = (float)from->a4; to[3][1] = (float)from->b4;  to[3][2] = (float)from->c4; to[3][3] = (float)from->d4;

	memcpy(node->transform, to, sizeof(mat4));
	// if (scene->mesh_count && ai_node->mNumMeshes) {
	// 	struct Mesh* new_meshes = (struct Mesh*) realloc(scene->meshes, sizeof(struct Mesh) * (ai_node->mNumMeshes + scene->mesh_count));
	// 	scene->meshes = new_meshes;

	// } else if (ai_node->mNumMeshes > 0) {
	// 	scene->meshes = (struct Mesh*) malloc(sizeof(struct Mesh)*(ai_node->mNumMeshes));
	// }
	
	node->meshes = malloc(sizeof(unsigned) * ai_node->mNumMeshes);
	node->mesh_count = ai_node->mNumMeshes;
	for (int i = 0; i < ai_node->mNumMeshes; ++i) {
		node->meshes[i] = ai_node->mMeshes[i];
	}

	node->child_count = ai_node->mNumChildren;
	node->children = malloc(node->child_count * sizeof(struct Node));	
	for (int i = 0; i < ai_node->mNumChildren; ++i) {
		process_node(ai_node->mChildren[i], &node->children[i], ai_scene, scene);
	}
}

void process_mesh(struct aiMesh* ai_mesh, const struct aiScene *ai_scene, struct Mesh* mesh, struct Scene* scene) {
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
            vec2 texture = { ai_texture_coord.x, 1 - ai_texture_coord.y};
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
	mesh->m = scene->materials[ai_mesh->mMaterialIndex];
} 

void destroy_node(struct Node* node) {
	for (int i = 0; i < node->child_count; ++i) {
		destroy_node(&node->children[i]);
	}
	free(node->children);
}

void destroy_scene(VkDevice dev, struct Scene scene) {
	if (scene.mesh_count) {
		free(scene.meshes);
	}
	free(scene.materials);
	for (int i = 0; i < scene.texture_count; ++i) {
		vkDestroyImage(dev, scene.textures[i].image, NULL);
		vkDestroyImageView(dev, scene.textures[i].image_view, NULL);
		free_allocation(dev, scene.textures[i].texture_alloc);
	}
	free(scene.textures);
	destroy_node(scene.root);
	free(scene.root);
}


// TODO: fix validation errors generated by this
static void generate_mips(VkDevice device, VkPhysicalDevice phys_dev, VkCommandBuffer command_buffer, VkQueue queue, VkImage image, int32_t width, int32_t height, uint32_t mips) {
	VkImageMemoryBarrier barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.image = image,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseArrayLayer = 0,
		.subresourceRange.layerCount = 1,
		.subresourceRange.levelCount = 1,
	};

	int32_t mip_width = width;
	int32_t mip_height = height;

	for (int i = 1; i < mips; ++i) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(command_buffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, NULL,
			0, NULL,
			1, &barrier);

		VkImageBlit blit_image = {
			.srcOffsets[0] = { 0, 0, 0 },
			.srcOffsets[1] = { mip_width, mip_height },
			.srcSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = i-1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
			.dstOffsets[0] = { 0, 0, 0 },
			.dstOffsets[1] = { mip_width > 1 ? mip_width / 2 : 1, mip_height > 1 ? mip_height / 2 : 1, 1},
			.dstSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = i,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};
		vkCmdBlitImage(
			command_buffer,
		 	image, 
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&blit_image,
			VK_FILTER_LINEAR
		);
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(command_buffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, NULL,
			0, NULL,
			1, &barrier);
		if (mip_width > 1) mip_width /= 2;
		if (mip_height > 1) mip_height /= 2;
	}
	barrier.subresourceRange.baseMipLevel = mips - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, NULL,
        0, NULL,
        1, &barrier);
	VkSubmitInfo submit_info = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL,
		0, NULL,
		0,
		1, &command_buffer,
		0, NULL
	};
	flush_command_buffer(&device, &command_buffer, &queue, &submit_info);
}

static void load_texture(char* const filename, VkDevice device, VkPhysicalDevice phys_dev, VkCommandBuffer comm_buff, VkQueue queue, struct Texture* texture) {
	int tex_width;
	int tex_height;
	int tex_channels;
	stbi_uc* pixels = stbi_load(texture->filepath, &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
	VkDeviceSize image_size = tex_width * tex_height * 4;
	unsigned mip_levels = ((uint32_t) floor(log2(fmax(tex_width, tex_height)))) + 1;
	
	if (!pixels) {
		abort();
		exit(EXIT_FAILURE);
	}

	VkImageCreateInfo texture_image_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.extent.width = (uint32_t) tex_width,
		.extent.height = (uint32_t) tex_height,
		.extent.depth = 1,
		.mipLevels = mip_levels,
		.arrayLayers = 1,
		.format = VK_FORMAT_R8G8B8A8_SRGB,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.samples = VK_SAMPLE_COUNT_1_BIT,
	};

	void* texture_data[] = { (void*) pixels };
	VkFormat formats[] = { VK_FORMAT_B8G8R8A8_SRGB };
	VkImageLayout from[] = { VK_IMAGE_LAYOUT_UNDEFINED };
	VkImageLayout to[] = { VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL };

	create_images(&phys_dev, &device, 1, &texture_image_info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
		&texture->image, &texture->texture_alloc);
	staged_buffer_write_to_image(&phys_dev, &device, &comm_buff, &queue, 1, &texture->image, texture_data, 
		&image_size, formats, from, to, tex_width, tex_height);	
	
	// generate_mips(device, phys_dev, comm_buff, queue, texture->image, tex_width, tex_height, mip_levels);

	VkImageViewCreateInfo tex_view_info = {};
	tex_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	tex_view_info.image = texture->image;
	tex_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	tex_view_info.format = VK_FORMAT_R8G8B8A8_SRGB;
	tex_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	tex_view_info.subresourceRange.baseMipLevel = 0;
	tex_view_info.subresourceRange.levelCount = 1;
	tex_view_info.subresourceRange.baseArrayLayer = 0;
	tex_view_info.subresourceRange.layerCount = 1;

	vkCreateImageView(device, &tex_view_info, NULL, &texture->image_view);
	VkPhysicalDeviceProperties properties = {};

	texture->loaded = true;
	stbi_image_free(pixels);
}

void load_textures(VkDevice device, VkPhysicalDevice phys_dev, VkCommandBuffer comm_buff, VkQueue queue, struct Scene* scene) {
	for (int i = 0; i < scene->texture_count; ++i) {
		struct Texture text = scene->textures[i];
		load_texture(text.filepath, device, phys_dev, comm_buff, queue, &scene->textures[i]);
	}

}

