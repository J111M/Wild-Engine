#include "Renderer/Passes/CascadedShadowPass.hpp"
#include "Renderer/Passes/ProceduralTerrainPass.hpp"
#include "Renderer/Resources/LightTypes.hpp"
#include "Renderer/Resources/Mesh.hpp"

#include "Core/Camera.hpp"

namespace Wild
{
    CascadedShadowPass::CascadedShadowPass()
    {
        BufferDesc desc{};
        desc.size = sizeof(DirectLightBuffer);
        desc.usage = BufferUsage::Constant;
        desc.access = MemoryAccess::CpuToGpu;
        m_directionalLightBuffer = std::make_shared<GPUBuffer>(desc);
    }

    void CascadedShadowPass::Add(Renderer& renderer, RenderGraph& rg)
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
                settings.shaderState.vertexShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/CascadedShadowsVert.slang");
                settings.shaderState.fragShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/CascadedShadowsFrag.slang");
                settings.depthStencilState.depthEnable = true;
                settings.rasterizerState.cullMode = CullMode::Front;

                // Hard coded size for now TODO use texture size
                settings.rasterizerState.viewport.size =
                    glm::vec2(passData.shadowMap[0]->Width(), passData.shadowMap[0]->Height());

                // Setting up the input layout
                settings.shaderState.inputLayout.emplace_back(InputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0));
                settings.shaderState.inputLayout.emplace_back(
                    InputElement("COLOR", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3)));
                settings.shaderState.inputLayout.emplace_back(
                    InputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3) * 2));
                settings.shaderState.inputLayout.emplace_back(
                    InputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, sizeof(glm::vec3) * 3));
                settings.shaderState.inputLayout.emplace_back(
                    InputElement("TANGENT", DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(glm::vec3) * 3 + sizeof(glm::vec2)));

                std::vector<Uniform> uniforms;
                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(CsmRootConstants)};
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

                    auto meshes = engine.GetECS()->GetRegistry().view<Transform, MeshComponent>();
                    for (auto&& [entity, trans, meshComponent] : meshes.each())
                    {
                        if (!meshComponent.mesh) continue;
                        auto& mesh = *meshComponent.mesh;

                        m_rc.localModel = trans.GetWorldMatrix();
                        m_rc.projView = m_directLight.viewProj[i];
                        m_rc.cascadeIndex = i;

                        list.SetRootConstant<CsmRootConstants>(0, m_rc);

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

    void CascadedShadowPass::Update(const float dt)
    {
        engine.GetImGui()->AddPanel("Shadowmap settings", [this]() {
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

            auto ecs = engine.GetECS();
            auto view = ecs->View<DirectionalLight>();
            for (auto entity : view)
            {
                auto& directionalLight = ecs->GetComponent<DirectionalLight>(entity);

                float yaw = dt * 0.05; // radians per second

                float cosYaw = cosf(yaw);
                float sinYaw = sinf(yaw);
                float x = directionalLight.direction[0];
                float z = directionalLight.direction[2];

                directionalLight.direction[0] = x * cosYaw - z * sinYaw;
                directionalLight.direction[2] = x * sinYaw + z * cosYaw;

                m_directLight.lightDirectionIntensity = glm::vec4(directionalLight.direction, directionalLight.colorIntensity.a);
                break;
            }

            for (uint32_t cascade = 0; cascade < SHADOWMAP_CASCADES; cascade++)
            {
                glm::mat4 cascadeProjections{};
                float cascadeFarDistances{};

                std::array<float, 2> nearFar;

                for (uint32_t nf = 0; nf < 2u; nf++)
                {
                    const glm::vec2 camNearFar = camera->GetNearFar();
                    const float shadowDistance = camNearFar.y;

                    const float ratio = static_cast<float>(cascade + nf) / static_cast<float>(SHADOWMAP_CASCADES);
                    float logS = camNearFar.x * std::powf(shadowDistance / camNearFar.x, ratio);
                    float linS = camNearFar.x + (shadowDistance - camNearFar.x) * ratio;
                    float nearField = glm::mix(logS, linS, 0.175f);
                    nearFar[nf] = nearField;
                }

                cascadeProjections = glm::perspectiveRH_ZO(camera->GetFOV(), camera->GetAspect(), nearFar[0], nearFar[1]);
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
    std::vector<glm::vec3> CascadedShadowPass::GetFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view)
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

    glm::mat4 CascadedShadowPass::GetCascadeMatrix(const glm::vec3& lightDir, const glm::mat4& cameraView,
                                                   const glm::mat4& cascadeProj, uint32_t cascadeIndex)
    {
        const auto cornersWS = GetFrustumCornersWorldSpace(cascadeProj, cameraView);

        glm::vec3 frustumCenter(0.0f);
        for (const glm::vec3& v : cornersWS)
        {
            frustumCenter += v;
        }
        frustumCenter /= static_cast<float>(cornersWS.size());

        float radius = 0.0f;
        for (const glm::vec3& v : cornersWS)
        {
            radius = glm::max(radius, glm::length(v - frustumCenter));
        }
        radius = std::ceil(radius * 16.0f) / 16.0f;

        const glm::vec3 L = glm::normalize(lightDir); // direction light travels
        const glm::vec3 up = (std::abs(L.y) > 0.99f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);

        // Pull the light back past the sphere plus m_zMult of caster room.
        const glm::vec3 lightPos = frustumCenter - L * (radius + m_zMult);
        const glm::mat4 lightView = glm::lookAtRH(lightPos, frustumCenter, up);

        const float zNear = 0.0f;
        const float zFar = 2.0f * radius + m_zMult;

        glm::mat4 lightProj = glm::orthoRH_ZO(-radius, radius, -radius, radius, zNear, zFar);

        // Snap the shadow texel grid to stop edge crawl as the camera moves.
        const float res = static_cast<float>(2048);
        glm::mat4 shadowMatrix = lightProj * lightView;
        glm::vec4 origin = shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        origin *= res * 0.5f;
        glm::vec4 rounded = glm::round(origin);
        glm::vec4 offset = (rounded - origin) * 2.0f / res;
        offset.z = 0.0f;
        offset.w = 0.0f;
        lightProj[3] += offset;

        if (m_drawDebugFrustum && !m_lockFrustum)
        {
            const glm::vec3 dbgMin(-radius, -radius, -zFar);
            const glm::vec3 dbgMax(radius, radius, -zNear);

            glm::vec4 wsCenterH = glm::inverse(lightView) * glm::vec4(0.0f, 0.0f, -(zNear + zFar) * 0.5f, 1.0f);
            glm::vec3 wsCenter = glm::vec3(wsCenterH) / wsCenterH.w;

            m_lightDirDebug.push_back(wsCenter);
            m_lightDirDebug2.push_back(wsCenter + L * (radius * 0.6f));

            m_minExtents.push_back(dbgMin);
            m_maxExtents.push_back(dbgMax);
            m_lightView.push_back(lightView);
            m_frustumCorners.push_back(cornersWS); // was pushed 8 times
        }

        m_directLight.cascadeSplitDepthRange[cascadeIndex] = zFar - zNear;

        return lightProj * lightView;
    }
} // namespace Wild
