#include "pch.h"

#include "core.h"

#include "vk_loader.h"
#include "vk_engine.h"
#include "vk_initializers.h"

#include <glm/gtx/quaternion.hpp>

#include <stb_image.h>


static VkFilter ExtractFilter(fastgltf::Filter filter)
{
    switch (filter) {
        case fastgltf::Filter::Nearest:
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::NearestMipMapLinear:
            return VK_FILTER_NEAREST;
        case fastgltf::Filter::Linear:
        case fastgltf::Filter::LinearMipMapNearest:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return VK_FILTER_LINEAR;
    }
}

static VkSamplerMipmapMode ExtractMipmapMode(fastgltf::Filter filter)
{
    switch (filter) {
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::LinearMipMapNearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case fastgltf::Filter::NearestMipMapLinear:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}


void LoadedGLTF::Render(const glm::mat4& topMatrix, RenderContext& ctx)
{
    for (std::shared_ptr<Node>& pNode : topNodes) {
        pNode->Render(topMatrix, ctx);
    }
}


void LoadedGLTF::ClearAll()
{
    VkDevice dv = pCreator->m_pVkDevice;

    descriptorPool.DestroyPools(dv);
    pCreator->DestroyBuffer(materialDataBuffer);

    for (auto& [k, v] : meshes) {
		pCreator->DestroyBuffer(v->meshBuffers.idxBuff);
		pCreator->DestroyBuffer(v->meshBuffers.vertBuff);
    }

    for (auto& [k, v] : images) {   
        if (v.pImage == pCreator->m_checkerboardImage.pImage) {
            continue;
        }
        pCreator->DestroyImage(v);
    }

	for (VkSampler& sampler : samplers) {
		vkDestroySampler(dv, sampler, nullptr);
    }
}


std::optional<std::shared_ptr<LoadedGLTF>> LoadGLTF(VulkanEngine* pEngine, const std::filesystem::path& filepath)
{
    fmt::print("Loading GLTF: {}\n", filepath.string().c_str());

    std::shared_ptr<LoadedGLTF> pScene = std::make_shared<LoadedGLTF>();
    pScene->pCreator = pEngine;
    LoadedGLTF& file = *pScene;

    fastgltf::Parser parser {};

    constexpr fastgltf::Options gltfOptions = fastgltf::Options::DontRequireValidAssetMember 
        | fastgltf::Options::AllowDouble
        | fastgltf::Options::LoadExternalBuffers;

    auto dataExp = fastgltf::GltfDataBuffer::FromPath(filepath);
    ENG_ASSERT_MSG(dataExp, "GLTF {} error: {}", filepath.string().c_str(), fastgltf::getErrorMessage(dataExp.error()))

    fastgltf::GltfDataBuffer& data = dataExp.get();

    fastgltf::Asset gltf;

    auto type = fastgltf::determineGltfFileType(data);
    if (type == fastgltf::GltfType::glTF) {
        auto load = parser.loadGltf(data, filepath.parent_path(), gltfOptions);
        if (load) {
            gltf = std::move(load.get());
        } else {
            fmt::println(stderr, "Failed to load glTF: {}", fastgltf::getErrorMessage(load.error()).data());
            return std::nullopt;
        }
    } else if (type == fastgltf::GltfType::GLB) {
        auto load = parser.loadGltfBinary(data, filepath.parent_path(), gltfOptions);
        if (load) {
            gltf = std::move(load.get());
        } else {
            fmt::println(stderr, "Failed to load glTF: {}", fastgltf::getErrorMessage(load.error()).data());
            return std::nullopt;
        }
    } else {
        fmt::println(stderr, "Failed to determine GLTF container");
        return std::nullopt;
    }

    // we can stimate the descriptors we will need accurately
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 }
    };

    file.descriptorPool.Init(pEngine->m_pVkDevice, gltf.materials.size(), sizes);

    for (fastgltf::Sampler& sampler : gltf.samplers) {
        VkSamplerCreateInfo sampl = {};
        sampl.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampl.maxLod = VK_LOD_CLAMP_NONE;
        sampl.minLod = 0;

        sampl.magFilter = ExtractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        sampl.minFilter = ExtractFilter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        sampl.mipmapMode= ExtractMipmapMode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler newSampler;
        vkCreateSampler(pEngine->m_pVkDevice, &sampl, nullptr, &newSampler);

        file.samplers.push_back(newSampler);
    }
    
    std::vector<ImageHandle> images;
    images.reserve(gltf.images.size());

    for (fastgltf::Image& image : gltf.images) {  
        std::optional<ImageHandle> img = LoadImage(pEngine, gltf, image);

		if (img.has_value()) {
			images.push_back(img.value());
			file.images[image.name.c_str()] = img.value();
		} else {
			images.push_back(pEngine->m_checkerboardImage);
			fmt::print("gltf failed to load texture {}\n", image.name.c_str());
		}
    }

    file.materialDataBuffer = pEngine->CreateBuffer(sizeof(GLTFMetallic_Roughness::MaterialConstants) * gltf.materials.size(),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    
    size_t dataIndex = 0;
    
    GLTFMetallic_Roughness::MaterialConstants* pSceneMaterialConstants =
        (GLTFMetallic_Roughness::MaterialConstants*)file.materialDataBuffer.allocationInfo.pMappedData;

    std::vector<std::shared_ptr<GLTFMaterial>> materials;
    materials.reserve(gltf.materials.size());

    for (fastgltf::Material& mat : gltf.materials) {
        std::shared_ptr<GLTFMaterial> newMat = std::make_shared<GLTFMaterial>();
        materials.push_back(newMat);

        file.materials[mat.name.c_str()] = newMat;

        GLTFMetallic_Roughness::MaterialConstants constants;
        constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
        constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
        constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
        constants.colorFactors.w = mat.pbrData.baseColorFactor[3];

        constants.metallicRoughnessFactors.x = mat.pbrData.metallicFactor;
        constants.metallicRoughnessFactors.y = mat.pbrData.roughnessFactor;
        // write material parameters to buffer
        pSceneMaterialConstants[dataIndex] = constants;

        const MaterialPass passType = mat.alphaMode == fastgltf::AlphaMode::Blend ? MaterialPass::TRANSPARENT : MaterialPass::OPAQUE;

        GLTFMetallic_Roughness::MaterialResources materialResources;
        
        materialResources.colorImage = pEngine->m_whiteImage;
        materialResources.colorSampler = pEngine->m_linearSampler;
        materialResources.metalRoughImage = pEngine->m_whiteImage;
        materialResources.metalRoughSampler = pEngine->m_linearSampler;

        materialResources.dataBuffer = file.materialDataBuffer.pBuffer;
        materialResources.dataBufferOffset = dataIndex * sizeof(GLTFMetallic_Roughness::MaterialConstants);

        if (mat.pbrData.baseColorTexture.has_value()) {
            const size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            const size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

            materialResources.colorImage = images[img];
            materialResources.colorSampler = file.samplers[sampler];
        }
        
        newMat->data = pEngine->m_metalRoughMaterial.WriteMaterial(pEngine->m_pVkDevice, passType, materialResources, file.descriptorPool);

        ++dataIndex;
    }
    
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    meshes.reserve(gltf.meshes.size());

    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    for (const fastgltf::Mesh& mesh : gltf.meshes) {
        std::shared_ptr<MeshAsset> newmesh = std::make_shared<MeshAsset>();
        meshes.push_back(newmesh);

        file.meshes[mesh.name.c_str()] = newmesh;
        newmesh->name = mesh.name;

        indices.clear();
        vertices.clear();

        for (const fastgltf::Primitive& p : mesh.primitives) {
            GeoSurface newSurface;
            newSurface.startIndex = (uint32_t)indices.size();
            newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

            size_t initialVtx = vertices.size();

            {
                fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexaccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                    [&](std::uint32_t idx) {
                        indices.push_back(idx + initialVtx);
                    });
            }

            {
                fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                    [&](glm::vec3 v, size_t index) {
                        Vertex newvtx;
                        newvtx.position = v;
                        newvtx.normal = glm::vec3(1, 0, 0);
                        newvtx.color = glm::vec4(1.f);
                        newvtx.uvX = 0;
                        newvtx.uvY = 0;
                        vertices[initialVtx + index] = newvtx;
                    });
            }

            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->accessorIndex],
                    [&](glm::vec3 v, size_t index) {
                        vertices[initialVtx + index].normal = v;
                    });
            }

            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uv->accessorIndex],
                    [&](glm::vec2 v, size_t index) {
                        vertices[initialVtx + index].uvX = v.x;
                        vertices[initialVtx + index].uvY = v.y;
                    });
            }

            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[colors->accessorIndex],
                    [&](glm::vec4 v, size_t index) {
                        vertices[initialVtx + index].color = v;
                    });
            }

            if (p.materialIndex.has_value()) {
                newSurface.material = materials[p.materialIndex.value()];
            } else {
                newSurface.material = materials[0];
            }

            newmesh->surfaces.push_back(newSurface);
        }

        newmesh->meshBuffers = pEngine->UploadMesh(indices, vertices);
    }

    std::vector<std::shared_ptr<Node>> nodes;
    nodes.reserve(gltf.nodes.size());

    for (const fastgltf::Node& node : gltf.nodes) {
        std::shared_ptr<Node> newNode;

        if (node.meshIndex.has_value()) {
            newNode = std::make_shared<MeshNode>();
            static_cast<MeshNode*>(newNode.get())->pMesh = meshes[node.meshIndex.value()];
        } else {
            newNode = std::make_shared<Node>();
        }

        nodes.push_back(newNode);
        file.nodes[node.name.c_str()];

        std::visit(fastgltf::visitor { 
            [&](const fastgltf::math::fmat4x4& matrix) {
                memcpy(&newNode->localTrs, matrix.data(), sizeof(matrix));
            },
            [&](const fastgltf::TRS& transform) {
                const glm::vec3 tl(transform.translation[0], transform.translation[1], transform.translation[2]);
                const glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
                const glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);
                
                const glm::mat4 tm = glm::translate(glm::identity<glm::mat4>(), tl);
                const glm::mat4 rm = glm::toMat4(rot);
                const glm::mat4 sm = glm::scale(glm::identity<glm::mat4>(), sc);
                
                newNode->localTrs = tm * rm * sm;
            }
        }, node.transform);
    }

    for (size_t i = 0; i < gltf.nodes.size(); ++i) {
        fastgltf::Node& node = gltf.nodes[i];
        std::shared_ptr<Node>& pSceneNode = nodes[i];

        for (size_t childIdx : node.children) {
            pSceneNode->children.push_back(nodes[childIdx]);
            nodes[childIdx]->pParent = pSceneNode;
        }
    }

    for (std::shared_ptr<Node>& pNode : nodes) {
        if (pNode->pParent.lock() == nullptr) {
            file.topNodes.push_back(pNode);
            pNode->RefreshTransform(glm::identity<glm::mat4>());
        }
    }

    return pScene;
}


std::optional<ImageHandle> LoadImage(VulkanEngine* pEngine, fastgltf::Asset& asset, fastgltf::Image& image)
{
    ImageHandle imageHandle = {};

    int width = 0, height = 0, nrChannels = 0;

    std::visit(
        fastgltf::visitor {
            [](auto& arg) {},
            [&](fastgltf::sources::URI& filePath) {
                ENG_ASSERT(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
                ENG_ASSERT(filePath.uri.isLocalPath()); // We're only capable of loading local files.

                const std::string path(filePath.uri.path().begin(), filePath.uri.path().end());
                uint8_t* pData = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
                
                if (pData) {
                    VkExtent3D imageSize;
                    imageSize.width = width;
                    imageSize.height = height;
                    imageSize.depth = 1;

                    imageHandle = pEngine->CreateImage(imageSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, pData, false);

                    stbi_image_free(pData);
                }
            },
            [&](fastgltf::sources::Vector& vector) {
                uint8_t* pData = stbi_load_from_memory((const stbi_uc*)vector.bytes.data(), static_cast<size_t>(vector.bytes.size()), &width, &height, &nrChannels, 4);
                
                if (pData) {
                    VkExtent3D imageSize;
                    imageSize.width = width;
                    imageSize.height = height;
                    imageSize.depth = 1;

                    imageHandle = pEngine->CreateImage(imageSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, pData, false);

                    stbi_image_free(pData);
                }
            },
            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(fastgltf::visitor {
                    [](auto& arg) {},
                    [&](fastgltf::sources::Vector& vector) {
                        uint8_t* pData = stbi_load_from_memory((const stbi_uc*)vector.bytes.data() + bufferView.byteOffset,
                            static_cast<size_t>(bufferView.byteLength), &width, &height, &nrChannels, 4);
                        
                        if (pData) {
                            VkExtent3D imageSize;
                            imageSize.width = width;
                            imageSize.height = height;
                            imageSize.depth = 1;

                            imageHandle = pEngine->CreateImage(imageSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, pData, false);

                           stbi_image_free(pData);
                       }
                   } },
                buffer.data);
            },
        },
        image.data);

    if (imageHandle.pImage != VK_NULL_HANDLE) {
        return imageHandle;
    } else {
        return std::nullopt;
    }
}