#include "pch.h"

#include "vk_loader.h"
#include "vk_engine.h"
#include "vk_initializers.h"

#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/core.hpp>

#include <stb_image.h>


    std::optional<std::vector<std::shared_ptr<MeshAsset>>> LoadGLTFMeshes(VulkanEngine& engine, const std::filesystem::path& filepath)
    {
        using namespace fastgltf;

        Accessor accessor;
        Expected<GltfDataBuffer> data = GltfDataBuffer::FromPath(filepath);
        
        if (!data) {
            const std::string filepathStr = filepath.string();
            fmt::print("Failed to load GLTF {}: {}\n", filepathStr.c_str(), getErrorName(data.error()).data());
            
            return std::nullopt;
        }

        constexpr Options gltfOptions = Options::LoadExternalBuffers;

        Asset gltf;
        Parser parser;

        auto load = parser.loadGltfBinary(data.get(), filepath.parent_path(), gltfOptions);
        if (load) {
            gltf = std::move(load.get());
        } else {
            const std::string filepathStr = filepath.string();
            fmt::print("Failed to load GLTF {}: {}\n", filepathStr.c_str(), getErrorName(data.error()).data());
            
            return std::nullopt;
        }

        std::vector<std::shared_ptr<MeshAsset>> meshes;

        std::vector<uint32_t> indices;
        std::vector<Vertex> vertices;

        for (fastgltf::Mesh& mesh : gltf.meshes) {
            MeshAsset newMesh;

            newMesh.name = mesh.name;

            indices.clear();
            vertices.clear();

            for (auto&& p : mesh.primitives) {
                GeoSurface newSurface;
                newSurface.startIndex = (uint32_t)indices.size();
                newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

                size_t initialVtx = vertices.size();

                // load indexes
                {
                    fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                    indices.reserve(indices.size() + indexaccessor.count);

                    fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                        [&](std::uint32_t idx) {
                            indices.push_back(idx + initialVtx);
                        });
                }

                // load vertex positions
                {
                    fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
                    vertices.resize(vertices.size() + posAccessor.count);

                    fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                        [&](glm::vec3 v, size_t index) {
                            Vertex newvtx;
                            newvtx.position = v;
                            newvtx.normal = { 1, 0, 0 };
                            newvtx.color = glm::vec4 { 1.f };
                            newvtx.uvX = 0;
                            newvtx.uvY = 0;
                            vertices[initialVtx + index] = newvtx;
                        });
                }

                // load vertex normals
                auto normals = p.findAttribute("NORMAL");
                if (normals != p.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).accessorIndex],
                        [&](glm::vec3 v, size_t index) {
                            vertices[initialVtx + index].normal = glm::normalize(v);
                        });
                }

                // load UVs
                auto uv = p.findAttribute("TEXCOORD_0");
                if (uv != p.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).accessorIndex],
                        [&](glm::vec2 v, size_t index) {
                            vertices[initialVtx + index].uvX = v.x;
                            vertices[initialVtx + index].uvY = v.y;
                        });
                }

                // load vertex colors
                auto colors = p.findAttribute("COLOR_0");
                if (colors != p.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).accessorIndex],
                        [&](glm::vec4 v, size_t index) {
                            vertices[initialVtx + index].color = v;
                        });
                }
                newMesh.surfaces.push_back(newSurface);
            }

            // display the vertex normals
            constexpr bool overrideColors = true;
            if (overrideColors) {
                for (Vertex& vtx : vertices) {
                    vtx.color = glm::vec4(vtx.normal, 1.f);
                }
            }
            newMesh.meshBuffers = engine.UploadMesh(indices, vertices);

            meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newMesh)));
        }

        return meshes;
    }
