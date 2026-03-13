struct DirectionalLight
{
    glm::vec4 colorIntensity{}; // w component stores intensity
    glm::vec3 direction{};
};

struct PointLight
{
    glm::vec4 colorIntensity{}; // w component stores intensity
    glm::vec3 position{};
};
