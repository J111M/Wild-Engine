#pragma once

#include <glm/glm.hpp>

#include <memory>

namespace Wild
{
    class Texture;

    namespace EditorWidgets
    {
        bool DragFloat3Colored(const char* name, glm::vec3& value, float speed = 0.1f);

        void TextureImage(Texture* texture, glm::vec2 imageSize = {150.0f, 150.0f});
        void TextureImage(const std::shared_ptr<Texture>& texture, glm::vec2 imageSize = {150.0f, 150.0f});
    } // namespace EditorWidgets
} // namespace Wild
