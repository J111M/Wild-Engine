#include "Model.hpp"

#include "Tools/TangentGeneration/MikktDataExtractor.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/tools.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Tools/log.hpp"

#include <optional>
#include <stdexcept>
#include <variant>

namespace Wild
{
    Model::Model(std::filesystem::path filePath, Entity parent)
    {
        m_filePath = filePath.parent_path().string() + "/";

        fastgltf::Parser parser{};

        auto data = fastgltf::GltfDataBuffer::FromPath(filePath);

        if (data.error() != fastgltf::Error::None)
        {
            WD_WARN("Model could not be loaded: {}", filePath.string());
            return;
        }

        auto asset = parser.loadGltf(data.get(), filePath.parent_path(), fastgltf::Options::LoadExternalBuffers);

        if (auto error = asset.error(); error != fastgltf::Error::None)
        {
            WD_WARN("Model could not be loaded: {}", filePath.string());
            WD_WARN("Failed to read the buffer, parsing the JSON, or validating the data!");
            return;
        }

        Load(asset.get(), parent);
    }

    void Model::Load(fastgltf::Asset& model, Entity parent)
    {
        for (uint32_t node : model.scenes[0].nodeIndices)
        {
            LoadNode(model, node, parent);
        }
    }

    void Model::LoadNode(fastgltf::Asset& model, uint32_t nodeIndex, Entity parent)
    {
        auto& node = model.nodes[nodeIndex];
        auto nodeEntity = engine.GetECS()->CreateEntity();

        auto& transform = engine.GetECS()->AddComponent<Transform>(nodeEntity, glm::vec3(0, 0, 0), nodeEntity);
        transform.Name = node.name;

        if (parent != entt::null) { transform.SetParent(parent); }

        auto matrix = fastgltf::getTransformMatrix(node);

        transform.SetFromMatrix(glm::make_mat4(matrix.data()));

        // Recurse over child nodes
        for (uint32_t node : node.children)
            LoadNode(model, node, parent);

        if (!node.meshIndex.has_value())
        {
            WD_INFO("Mesh Index is -1!");
            return;
        }

        auto& mesh = model.meshes[node.meshIndex.value()];

        for (auto& primitive : mesh.primitives)
        {
            if (mesh.primitives.size() == 1)
            {
                std::vector<Vertex> meshVertex{};
                std::vector<uint32_t> meshIndices{};

                LoadMeshData(model, primitive, meshVertex, meshIndices);

                auto& mesh = engine.GetECS()->AddComponent<Mesh>(nodeEntity, meshVertex, meshIndices);
                mesh.SetMaterial(LoadMaterials(model, primitive));

                // auto& resourceSystem = engine.GetResourceSystems().m_meshResourceSystem;
                // if (resourceSystem->HasResource(node.name.c_str()))
                //{
                //     // If map contains resource create a meshcomponent with the coresponding resource
                //     auto& component =
                //         engine.GetECS()->AddComponent<MeshComponent>(nodeEntity,
                //         resourceSystem->GetResource(node.name.c_str()));

                //    WD_INFO("Mesh found in map: {}", node.name.c_str());
                //}
                // else
                //{
                //    // If resource is not yet in the map load it and add it
                //    LoadMeshData(model, primitive, meshVertex, meshIndices);

                //    auto& meshModel = engine.GetECS()->AddComponent<MeshComponent>(
                //        nodeEntity, resourceSystem->GetOrCreateResource(node.name.c_str(), meshVertex, meshIndices));
                //    meshModel.mesh->SetMaterial(LoadMaterials(model, primitive));
                //}

                meshVertex.clear();
                meshIndices.clear();
            }
            else
                LoadPrimitive(model, primitive, nodeEntity, mesh.name.c_str());
        }
        // m_childMeshes.emplace_back(Mesh(model, primitive));
    }

    void Model::LoadPrimitive(fastgltf::Asset& model, fastgltf::Primitive& primitive, Entity parent, const char* meshName)
    {
        auto primitiveEntity = engine.GetECS()->CreateEntity();

        auto& transform = engine.GetECS()->AddComponent<Transform>(primitiveEntity, glm::vec3(0, 0, 0), primitiveEntity);

        if (parent != entt::null) { transform.SetParent(parent); }

        std::vector<Vertex> meshVertex{};
        std::vector<uint32_t> meshIndices{};

        // If resource is not yet in the map load it and add it
        LoadMeshData(model, primitive, meshVertex, meshIndices);

        auto& mesh = engine.GetECS()->AddComponent<Mesh>(primitiveEntity, meshVertex, meshIndices);
        mesh.SetMaterial(LoadMaterials(model, primitive));

        // Add mesh to bottom level acceleation structure
        if (engine.GetGfxContext()->GetCapabilities().SupportsRayTracing()) {
             D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
             geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
             geometryDesc.Triangles.IndexBuffer = mesh.GetIndexBuffer()->GetIBView()->GetGPUVirtualAddress();
             geometryDesc.Triangles.IndexCount = static_cast<UINT>(mesh.GetIndexBuffer()->GetBuffer()->GetDesc().Width) / sizeof(uint32_t);
             geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
             geometryDesc.Triangles.Transform3x4 = 0;
             geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
             geometryDesc.Triangles.VertexCount =
                 static_cast<UINT>(mesh.GetVertexBuffer()->GetBuffer()->GetDesc().Width) / sizeof(Vertex);
             geometryDesc.Triangles.VertexBuffer.StartAddress = mesh.GetVertexBuffer()->GetVBView()->GetGPUVirtualAddress();
             geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);

             geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;


            engine.GetAccelerationStructureManager()->AddBottomLevelAS(
                 &geometryDesc, 1, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE);
        }
       

        // auto& resourceSystem = engine.GetResourceSystems().m_meshResourceSystem;
        // if (resourceSystem->HasResource(meshName))
        //{
        //     // If map contains resource create a meshcomponent with the coresponding resource
        //     auto& component =
        //         engine.GetECS()->AddComponent<MeshComponent>(primitiveEntity, resourceSystem->GetResource(meshName));

        //    WD_INFO("Mesh found in map: {}", meshName);
        //}
        // else
        //{
        //
        //}

        meshVertex.clear();
        meshIndices.clear();
    }

    void Model::LoadMeshData(fastgltf::Asset& model, fastgltf::Primitive& primitive, std::vector<Vertex>& vertexData,
                             std::vector<uint32_t>& indicesData)
    {
        {
            fastgltf::Accessor& indexAccessor = model.accessors[primitive.indicesAccessor.value()];

            // const auto& bufferView = model.bufferViews[indexAccessor];
            // const auto& buffer = model.buffers[bufferView.bufferIndex];

            // Read raw index data
            // const uint8_t* data = reinterpret_cast<const uint8_t*>(buffer.data.data()) + bufferView.byteOffset +
            // indexAccessor.byteOffset;

            size_t count = indexAccessor.count;
            indicesData.reserve(count);

            switch (indexAccessor.componentType)
            {
            case fastgltf::ComponentType::UnsignedByte:
                fastgltf::iterateAccessor<std::uint8_t>(
                    model, indexAccessor, [&](std::uint8_t idx) { indicesData.push_back(static_cast<uint32_t>(idx)); });
                break;
            case fastgltf::ComponentType::UnsignedShort:
                fastgltf::iterateAccessor<std::uint16_t>(
                    model, indexAccessor, [&](std::uint16_t idx) { indicesData.push_back(static_cast<uint32_t>(idx)); });
                break;
            case fastgltf::ComponentType::UnsignedInt:
                fastgltf::iterateAccessor<std::uint32_t>(
                    model, indexAccessor, [&](std::uint32_t idx) { indicesData.push_back(idx); });
                break;
            default:
                throw std::runtime_error("Unsupported index format");
            }

            // Load Vertices
            {
                fastgltf::Accessor& posAccessor = model.accessors[primitive.findAttribute("POSITION")->accessorIndex];
                vertexData.resize(vertexData.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(model, posAccessor, [&](glm::vec3 v, size_t index) {
                    Vertex newVertex;

                    newVertex.position = v;
                    newVertex.color = glm::vec3{1.0f};
                    newVertex.normal = glm::vec3{0.0f};
                    newVertex.uv = glm::vec2(0.0f, 0.0f);
                    vertexData[index] = newVertex;
                });
            }

            // Load colors
            {
                auto colors = primitive.findAttribute("COLOR_0");
                if (colors != primitive.attributes.end())
                {

                    fastgltf::iterateAccessorWithIndex<glm::vec4>(
                        model, model.accessors[(*colors).accessorIndex], [&](glm::vec4 v, size_t index) {
                            vertexData[index].color = glm::vec3(v.x, v.y, v.z);
                        });
                }
            }

            // Load normals
            {
                auto normals = primitive.findAttribute("NORMAL");
                if (normals != primitive.attributes.end())
                {

                    fastgltf::iterateAccessorWithIndex<glm::vec3>(
                        model, model.accessors[(*normals).accessorIndex], [&](glm::vec3 v, size_t index) {
                            vertexData[index].normal = glm::vec3(v.x, v.y, v.z);
                        });
                }
            }

            // Load UVs
            {
                auto uv = primitive.findAttribute("TEXCOORD_0");
                if (uv != primitive.attributes.end())
                {

                    fastgltf::iterateAccessorWithIndex<glm::vec2>(model,
                                                                  model.accessors[(*uv).accessorIndex],
                                                                  [&](glm::vec2 v, size_t index) { vertexData[index].uv = v; });
                }
            }

            // Load tangents
            {
                auto tangentAttr = primitive.findAttribute("TANGENT");
                if (tangentAttr != primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(
                        model, model.accessors[(*tangentAttr).accessorIndex], [&](glm::vec4 v, size_t index) {
                            vertexData[index].tangent = v;
                        });
                }
                else
                {
                    MikktDataExtractor userData{vertexData, indicesData};

                    SMikkTSpaceInterface mikktInterface{};
                    mikktInterface.m_getNumFaces = MikktDataExtractor::GetNumFaces;
                    mikktInterface.m_getNumVerticesOfFace = MikktDataExtractor::GetNumVerticesOfFace;
                    mikktInterface.m_getPosition = MikktDataExtractor::GetPosition;
                    mikktInterface.m_getNormal = MikktDataExtractor::GetNormal;
                    mikktInterface.m_getTexCoord = MikktDataExtractor::GetTexCoord;
                    mikktInterface.m_setTSpace = MikktDataExtractor::SetTSpace;

                    SMikkTSpaceContext ctx{};
                    ctx.m_pInterface = &mikktInterface;
                    ctx.m_pUserData = &userData;

                    if (!genTangSpaceDefault(&ctx)) { WD_WARN("Failed to load and create tangents."); }
                }
            }
        }
    }

    Material Model::LoadMaterials(fastgltf::Asset& model, fastgltf::Primitive& primitive)
    {
        if (!primitive.materialIndex.has_value()) { WD_WARN("No material found."); }

        Material materials;

        auto& material = model.materials[primitive.materialIndex.value()];

        materials.roughness = material.pbrData.roughnessFactor;
        materials.metallic = material.pbrData.metallicFactor;
        materials.emissiveStrenght = material.emissiveStrength;

        if (material.pbrData.baseColorTexture.has_value())
        {
            auto& gltfTexture = model.textures[material.pbrData.baseColorTexture->textureIndex];
            size_t imageIndex = gltfTexture.imageIndex.value();
            auto& image = model.images[imageIndex];

            auto& uri = std::get<fastgltf::sources::URI>(image.data);

            std::string file = m_filePath + (std::string)uri.uri.string();

            materials.m_albedo = std::make_unique<Texture>(file, TextureType::TEXTURE_2D);
        }

        if (material.pbrData.metallicRoughnessTexture.has_value())
        {
            auto& gltfTexture = model.textures[material.pbrData.metallicRoughnessTexture->textureIndex];
            size_t imageIndex = gltfTexture.imageIndex.value();
            auto& image = model.images[imageIndex];

            auto& uri = std::get<fastgltf::sources::URI>(image.data);

            std::string file = m_filePath + (std::string)uri.uri.string();

            materials.m_roughnessMetallic = std::make_unique<Texture>(file, TextureType::TEXTURE_2D);
        }

        if (material.emissiveTexture.has_value())
        {
            auto& gltfTexture = model.textures[material.emissiveTexture->textureIndex];
            size_t imageIndex = gltfTexture.imageIndex.value();
            auto& image = model.images[imageIndex];

            auto& uri = std::get<fastgltf::sources::URI>(image.data);

            std::string file = m_filePath + (std::string)uri.uri.string();

            materials.m_emissive = std::make_unique<Texture>(file, TextureType::TEXTURE_2D);
        }

        if (material.occlusionTexture.has_value())
        {
            auto& gltfTexture = model.textures[material.occlusionTexture->textureIndex];
            size_t imageIndex = gltfTexture.imageIndex.value();
            auto& image = model.images[imageIndex];

            auto& uri = std::get<fastgltf::sources::URI>(image.data);

            std::string file = m_filePath + (std::string)uri.uri.string();

            materials.m_occlusion = std::make_unique<Texture>(file, TextureType::TEXTURE_2D);
        }

        if (material.normalTexture.has_value())
        {
            auto& gltfTexture = model.textures[material.normalTexture->textureIndex];
            size_t imageIndex = gltfTexture.imageIndex.value();
            auto& image = model.images[imageIndex];

            auto& uri = std::get<fastgltf::sources::URI>(image.data);

            std::string file = m_filePath + (std::string)uri.uri.string();

            materials.m_normal = std::make_unique<Texture>(file, TextureType::TEXTURE_2D);
        }

        return materials;
    }
} // namespace Wild
