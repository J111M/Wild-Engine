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
            desc.height = 2048;
            std::string name = "Shadow map cascade: " + std::to_string(cascade);
            desc.name = name;
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(
                TextureDesc::depthStencil | TextureDesc::shaderResource); // Automatically uses depth stencil format

            passData->shadowMap[cascade] = rg.CreateTransientTexture(name, desc);
        }

        rg.AddPass<CsmPassData>(
            "Cascaded shadow maps", PassType::Graphics, [&renderer, this](CsmPassData& passData, CommandList& list) {
                if (SHADOWMAP_CASCADES > 4)
                {
                    WD_FATAL("Can't have more than 4 cascades");
                    return;
                }
                passData.biasValue = m_shadowBias;

                PipelineStateSettings settings{};
                settings.ShaderState.VertexShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/CascadedShadowsVert.slang");
                settings.ShaderState.FragShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/CascadedShadowsFrag.slang");
                settings.DepthStencilState.DepthEnable = true;
                settings.RasterizerState.CullMode = CullMode::Front;

                // Hard coded size for now TODO use texture size
                settings.RasterizerState.Viewport.size =
                    glm::vec2(passData.shadowMap[0]->Width(), passData.shadowMap[0]->Height());

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
                        m_rc.projView = m_directLight.viewProj[i];
                        m_rc.cascadeIndex = i;

                        list.SetRootConstant<CsmRC>(0, m_rc);

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
                if (m_drawDebugFrustum)
                {
                    for (size_t i = 0; i < SHADOWMAP_CASCADES; i++)
                    {
                        const glm::vec3 color[]{glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1), glm::vec3(1, 1, 0)};

                        float l = m_minExtents[i].x, b = m_minExtents[i].y, n = m_maxExtents[i].z;
                        float r = m_maxExtents[i].x, t = m_maxExtents[i].y, f = m_minExtents[i].z;

                        glm::mat4 invLightView = glm::inverse(m_lightView[i]);

                        glm::vec3 wsCorners[8] = {
                            {l, b, n}, {r, b, n}, {r, t, n}, {l, t, n}, {l, b, f}, {r, b, f}, {r, t, f}, {l, t, f}};

                        for (auto& c : wsCorners)
                        {
                            glm::vec4 ws = invLightView * glm::vec4(c, 1.0f);
                            c = glm::vec3(ws) / ws.w;
                        }

                        renderer.AddLine(wsCorners[0], wsCorners[1], color[i]);
                        renderer.AddLine(wsCorners[1], wsCorners[2], color[i]);
                        renderer.AddLine(wsCorners[2], wsCorners[3], color[i]);
                        renderer.AddLine(wsCorners[3], wsCorners[0], color[i]);

                        renderer.AddLine(wsCorners[4], wsCorners[5], color[i]);
                        renderer.AddLine(wsCorners[5], wsCorners[6], color[i]);
                        renderer.AddLine(wsCorners[6], wsCorners[7], color[i]);
                        renderer.AddLine(wsCorners[7], wsCorners[4], color[i]);

                        renderer.AddLine(wsCorners[0], wsCorners[4], color[i]);
                        renderer.AddLine(wsCorners[1], wsCorners[5], color[i]);
                        renderer.AddLine(wsCorners[2], wsCorners[6], color[i]);
                        renderer.AddLine(wsCorners[3], wsCorners[7], color[i]);

                        renderer.AddLine(m_lightDirDebug[i], m_lightDirDebug2[i], glm::vec3(1, 1, 1));
                    }

                    for (size_t i = 0; i < m_frustumCorners.size(); i++)
                    {
                        const auto& c = m_frustumCorners[i];
                        const glm::vec3& clr = glm::vec3(1, 0, 0);

                        // Near plane (z=0): 0,2,6,4
                        renderer.AddLine(c[0], c[2], clr);
                        renderer.AddLine(c[2], c[6], clr);
                        renderer.AddLine(c[6], c[4], clr);
                        renderer.AddLine(c[4], c[0], clr);
                        // Far plane (z=1): 1,3,7,5
                        renderer.AddLine(c[1], c[3], clr);
                        renderer.AddLine(c[3], c[7], clr);
                        renderer.AddLine(c[7], c[5], clr);
                        renderer.AddLine(c[5], c[1], clr);
                        // Connecting edges
                        renderer.AddLine(c[0], c[1], clr);
                        renderer.AddLine(c[2], c[3], clr);
                        renderer.AddLine(c[4], c[5], clr);
                        renderer.AddLine(c[6], c[7], clr);
                    }
                }

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
            if (ImGui::SliderFloat3("Light direction: ", &m_directLight.lightDirectionIntensity[0], -100.0f, 100.0f))
            {
                m_lightChanged = true;
            }

            ImGui::SliderFloat("Bias value", &m_shadowBias, 0.001, 20.0f);
            ImGui::SliderFloat("Z Mult", &m_zMult, 0.01, 20.0f);

            if (ImGui::Button("Draw light cascaded and frustum")) { m_drawDebugFrustum = !m_drawDebugFrustum; }
            if (ImGui::Button("Lock debug frustum")) { m_lockFrustum = !m_lockFrustum; }
        });

        Camera* camera = GetActiveCamera();

        if (camera)
        {
            if (m_drawDebugFrustum)
            {
                if (!m_lockFrustum)
                {
                    m_minExtents.clear();
                    m_maxExtents.clear();
                    m_lightView.clear();
                    m_frustumCorners.clear();
                    m_lightDirDebug.clear();
                    m_lightDirDebug2.clear();
                }
            }

            for (uint32_t cascade = 0; cascade < SHADOWMAP_CASCADES; cascade++)
            {
                glm::mat4 cascadeProjections{};
                float cascadeFarDistances{};

                std::array<float, 2> nearFar;

                for (uint32_t nf = 0; nf < 2u; nf++)
                {
                    const glm::vec2 camNearFar = camera->GetNearFar();
                    const float shadowDistance = 100.0f;

                    const float ratio = static_cast<float>(cascade + nf) / static_cast<float>(SHADOWMAP_CASCADES);
                    float logS = camNearFar.x * std::powf(shadowDistance / camNearFar.x, ratio);
                    float linS = camNearFar.x + (shadowDistance - camNearFar.x) * ratio;
                    float nearField = glm::mix(logS, linS, 0.175f);
                    nearFar[nf] = nearField;
                }

                cascadeProjections = glm::perspective(glm::radians(70.0f), camera->GetAspect(), nearFar[0], nearFar[1]);
                cascadeFarDistances = nearFar[1];

                const glm::mat4 cascadeViewProj = GetCascadeMatrix(glm::vec3(m_directLight.lightDirectionIntensity.x,
                                                                             m_directLight.lightDirectionIntensity.y,
                                                                             m_directLight.lightDirectionIntensity.z),
                                                                   camera->GetView(),
                                                                   cascadeProjections,
                                                                   cascade);

                // Set direct light data
                m_directLight.viewProj[cascade] = cascadeViewProj;
                m_directLight.cascadeDistance[cascade] = cascadeFarDistances;
            }
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
                    // glm::vec4 pt = inv * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
                    glm::vec4 pt = inv * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, static_cast<float>(z), 1.0f);
                    frustumCorners.push_back(glm::vec3(pt) / pt.w);
                }

        return frustumCorners;
    }

    glm::mat4 CascadedShadowMaps::GetCascadeMatrix(const glm::vec3& lightDir, const glm::mat4& cameraView,
                                                   const glm::mat4& cascadeProj, uint32_t cascadeIndex)
    {
        const auto cornersWS = GetFrustumCornersWorldSpace(cascadeProj, cameraView);

        // Calculate view frostum center by getting the average.
        glm::vec3 frustumCenter(0.0f);

        for (const glm::vec3& v : cornersWS)
        {
            frustumCenter += v;
        }

        frustumCenter /= static_cast<float>(cornersWS.size());

        // Frustum bounds
        glm::vec3 min = glm::vec3(FLT_MAX);
        glm::vec3 max = glm::vec3(-FLT_MAX);

        glm::vec3 normalizeLightDir = glm::normalize(lightDir);

        const glm::vec3 lightPos = frustumCenter + normalizeLightDir;

        const glm::mat4 lightView = glm::lookAtRH(lightPos, frustumCenter, glm::vec3(0, 1, 0));

        // Transform the world space corners to light space
        for (const auto& corner : cornersWS)
        {
            const glm::vec3 cornerLS = glm::vec3(lightView * glm::vec4(corner, 1.0f));
            min = glm::min(min, cornerLS);
            max = glm::max(max, cornerLS);
        }

        // Z multiple should be modified according to the scene
        if (min.z < 0) { min.z *= m_zMult; }
        else
        {
            min.z /= m_zMult;
        }
        if (max.z < 0) { max.z /= m_zMult; }
        else
        {
            max.z *= m_zMult;
        }

        // Set debug line data
        if (m_drawDebugFrustum)
        {
            if (!m_lockFrustum)
            {
                glm::vec3 lsCenter = glm::vec3((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f);
                glm::vec4 wsCenterH = glm::inverse(lightView) * glm::vec4(lsCenter, 1.0f);
                glm::vec3 wsCenter = glm::vec3(wsCenterH) / wsCenterH.w;

                glm::vec3 lightDir = glm::normalize(glm::vec3(m_directLight.lightDirectionIntensity.x,
                                                              m_directLight.lightDirectionIntensity.y,
                                                              m_directLight.lightDirectionIntensity.z));

                float arrowLen =
                    glm::length(glm::vec3(max.x - min.x, max.y - min.y, max.z - min.z)) * 0.3f; // scale relative to cascade size

                m_lightDirDebug.push_back(wsCenter);
                m_lightDirDebug2.push_back(wsCenter + lightDir * arrowLen);

                m_minExtents.push_back(min);
                m_maxExtents.push_back(max);
                m_lightView.push_back(lightView);

                for (size_t i = 0; i < cornersWS.size(); i++)
                {
                    m_frustumCorners.push_back(cornersWS);
                }
            }
        }

        // Get the cascade depth split for the near and far
        m_directLight.cascadeSplitDepthRange[cascadeIndex] = max.z - min.z;

        const glm::mat4 lightProj = glm::orthoRH_ZO(min.x, max.x, min.y, max.y, min.z, max.z);

        return lightProj * lightView;
    }
} // namespace Wild
