#include "Renderer/Passes/CascadedShadowPass.hpp"
#include "Renderer/Passes/ProceduralTerrainPass.hpp"
#include "Renderer/Resources/Mesh.hpp"

#include "Core/Camera.hpp"

namespace Wild
{
    CascadedShadowMaps::CascadedShadowMaps()
    {
        BufferDesc desc{};
        desc.bufferSize = sizeof(DirectLightBuffer);
        m_directionalLightBuffer = std::make_shared<Buffer>(desc, BufferType::constant);
    }

    void CascadedShadowMaps::Add(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<CsmPassData>();

        for (size_t cascade = 0; cascade < SHADOWMAP_CASCADES; cascade++)
        {
            TextureDesc desc;
            desc.width = 2048;
            desc.Height = 2048;
            std::string name = "Shadow map cascade: " + std::to_string(cascade);
            desc.name = name;
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(
                TextureDesc::depthStencil | TextureDesc::shaderResource); // Automatically uses depth stencil format

            passData->shadowMap[cascade] = rg.CreateTransientTexture(name, desc);
        }

        rg.AddPass<CsmPassData>(
            "Cascaded shadow maps", PassType::Graphics, [&renderer, this](CsmPassData& passData, CommandList& list) {
                PipelineStateSettings settings{};
                settings.ShaderState.VertexShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/CascadedShadowsVert.slang");
                settings.ShaderState.FragShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/CascadedShadowsFrag.slang");
                settings.DepthStencilState.DepthEnable = true;
                settings.RasterizerState.CullMode = CullMode::Front;

                // Hard coded size for now TODO use texture size
                settings.RasterizerState.Viewport.size = glm::vec2(2048, 2048);

                // Setting up the input layout
                settings.ShaderState.InputLayout.emplace_back(InputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("COLOR", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3)));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3) * 2));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, sizeof(glm::vec3) * 3));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("TANGENT", DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(glm::vec3) * 3 + sizeof(glm::vec2)));

                std::vector<Uniform> uniforms;
                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(CsmRC)};
                uniforms.emplace_back(rootConstant);

                Uniform lightBuffer{1, 0, RootParams::RootResourceType::ConstantBufferView};
                uniforms.emplace_back(lightBuffer);

                auto& pipeline =
                    renderer.GetOrCreatePipeline("Cascaded shadow maps pass", PipelineStateType::Graphics, settings, uniforms);

                for (size_t i = 0; i < SHADOWMAP_CASCADES; i++)
                {
                    list.SetPipelineState(pipeline);

                    list.BeginRender({},
                                     {ClearOperation::Store},
                                     {passData.shadowMap[i]},
                                     DSClearOperation::DepthClear,
                                     "Cascaded shadow pass");

                    auto meshes = engine.GetECS()->GetRegistry().view<Transform, Mesh>();
                    for (auto&& [entity, trans, mesh] : meshes.each())
                    {
                        m_rc.localModel = trans.GetWorldMatrix();
                        m_rc.cascadeIndex = i;

                        list.SetRootConstant<CsmRC>(0, m_rc);
                        list.SetConstantBufferView(1, m_directionalLightBuffer.get());

                        list.GetList()->IASetVertexBuffers(0, 1, &mesh.GetVertexBuffer()->GetVBView()->View());

                        if (mesh.HasIndexBuffer())
                        {
                            list.GetList()->IASetIndexBuffer(&mesh.GetIndexBuffer()->GetIBView()->View());
                            list.GetList()->DrawIndexedInstanced(mesh.GetDrawCount(), 1, 0, 0, 0);
                        }
                        else
                        {
                            list.GetList()->DrawInstanced(mesh.GetDrawCount(), 1, 0, 0);
                        }
                    }

                    list.EndRender();

                    passData.shadowMap[i]->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                }

                passData.directLightBuffer = m_directionalLightBuffer;

                // Debug the shadow map
                engine.GetImGui()->AddPanel("Shadowmap Textures", [this, passData]() {
                    for (size_t i = 0; i < SHADOWMAP_CASCADES; i++)
                    {
                        engine.GetImGui()->DisplayTexture(passData.shadowMap[i]);
                    }
                });
            });
    }

    void CascadedShadowMaps::Update(const float dt)
    {
        engine.GetImGui()->AddPanel("Shadowmap settings", [this]() {
            if (ImGui::SliderFloat3("Light direction: ", &m_directLight.lightDirectionIntensity[0], -20.0f, 20.0f))
            {
                m_lightChanged = true;
            }
        });

        const auto& camView = engine.GetECS()->GetRegistry().view<Camera, Transform>();
        const Camera& camera = engine.GetECS()->GetRegistry().get<Camera>(camView.front());

        for (uint32_t cascade = 0; cascade < SHADOWMAP_CASCADES; cascade++)
        {
            glm::mat4 cascadeProjections{};
            float cascadeFarDistances{};

            std::array<float, 2> nearFar;

            for (uint32_t j = 0; j < 2u; j++)
            {
                const glm::vec2 camNearFar = camera.GetNearFar();
                const float shadowDistance = 100.0f;

                const float ratio = static_cast<float>(cascade + j) / static_cast<float>(SHADOWMAP_CASCADES);
                float logS = camNearFar.x * std::powf(shadowDistance / camNearFar.x, ratio);
                float linS = camNearFar.x + (shadowDistance - camNearFar.x) * ratio;
                float nearField = glm::mix(logS, linS, 0.175f);
                nearFar[j] = nearField;
            }

            cascadeProjections = glm::perspective(camera.GetFOV(), camera.GetAspect(), nearFar[0], nearFar[1]);
            cascadeFarDistances = nearFar[1];

            glm::vec3 light = glm::normalize(glm::vec3(m_directLight.lightDirectionIntensity.x,
                                                       m_directLight.lightDirectionIntensity.y,
                                                       m_directLight.lightDirectionIntensity.z));

            glm::vec3 lightDir = light;
            // glm::vec3 up = (glm::abs(lightDir.y) > 0.99f) ? glm::vec3(0, 0, 1) : ;
            glm::vec3 lightPos = light;

            const glm::mat4 cascadeViewProj = glm::orthoRH_ZO(-20.0f, 20.0f, -20.0f, 20.0f, 1.0f, 20.0f) *
                glm::lookAtRH(lightPos, glm::vec3(0.0f), glm::vec3(0, 1, 0));
            //= GetCascadeMatrix(glm::vec3(m_directLight.lightDirectionIntensity.x,
            // m_directLight.lightDirectionIntensity.y,m_directLight.lightDirectionIntensity.z),camera.GetView(),cascadeProjections);

            m_directLight.viewProj[cascade] = cascadeViewProj;
            m_directLight.cascadeDistance[cascade] = cascadeFarDistances;
        }
        m_directionalLightBuffer->Allocate(&m_directLight);
    }

    // Function taken from https://learnopengl.com/Guest-Articles/2021/CSM and modified to work in my case
    std::vector<glm::vec3> CascadedShadowMaps::GetFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view)
    {
        glm::mat4 inv = glm::inverse(proj * view);

        std::vector<glm::vec3> frustumCorners;
        for (int x = 0; x < 2; ++x)
            for (int y = 0; y < 2; ++y)
                for (int z = 0; z < 2; ++z)
                {
                    glm::vec4 pt = inv * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, static_cast<float>(z), 1.0f);
                    frustumCorners.push_back(glm::vec3(pt) / pt.w);
                }

        return frustumCorners;
    }

    glm::mat4 CascadedShadowMaps::GetCascadeMatrix(const glm::vec3& lightDir, const glm::mat4& cameraView,
                                                   const glm::mat4& cascadeProj)
    {
        const auto cornersWS = GetFrustumCornersWorldSpace(cascadeProj, cameraView);

        // Calculate view frostum center.
        glm::vec3 frustumCenter(0.0f);

        for (const glm::vec3& v : cornersWS)
        {
            frustumCenter += v;
        }

        frustumCenter /= static_cast<float>(cornersWS.size());

        float radius = 0.0f;
        for (const glm::vec3& v : cornersWS)
        {
            float distance = glm::distance(v, frustumCenter);
            radius = std::max(radius, distance);
        }
        radius = std::ceil(radius * 8.0f) / 8.0f;

        const glm::vec3 maxExtents(radius, radius, radius);
        const glm::vec3 minExtents = -maxExtents;
        const glm::vec3 cascadeExtents = maxExtents - minExtents;

        const glm::vec3 lightPos = frustumCenter + glm::normalize(lightDir) * radius;
        const glm::mat4 lightView = glm::lookAtRH(lightPos, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));

        float l = minExtents.x;
        float b = minExtents.y;
        float n = minExtents.z;
        float r = maxExtents.x;
        float t = maxExtents.y;
        float f = maxExtents.z * 1.5f;

        const glm::mat4 lightProj = glm::orthoRH_ZO(l, r, b, t, n, f);

        return lightProj * lightView;
    }
} // namespace Wild
