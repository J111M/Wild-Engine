#pragma once

namespace Wild
{

#define MAXGRASSBLADES 500000

    struct GrassBladeData
    {
        glm::vec3 worldPosition{};
        float rotation{};
        float height{};
    };

    struct GrassComputeRC
    {
        glm::mat4 modelMatrix{};
        glm::vec2 chunkPosition{};
        glm::vec2 minMaxHeight{};
        uint32_t seed{};
        uint32_t bufferOffset{};
    };

    // TODO modify Grass compute so it is done after the chunk generation in the terrain pass
    class GrassCompute
    {
      public:
        GrassCompute();
        ~GrassCompute() {};

        void Update() {};
        void Render(CommandList& list);

        std::shared_ptr<Buffer> GetGrassData() { return m_bladeDataBuffer; }

      private:
        GrassComputeRC m_rc{};

        std::shared_ptr<Buffer> m_bladeDataBuffer;
        std::shared_ptr<Shader> m_computeShader;

        PipelineStateSettings m_settings;
        std::shared_ptr<PipelineState> m_pipeline{};
    };
} // namespace Wild
