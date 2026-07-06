#include "Systems/Physics/PhysicsSystem.hpp"

#include "Core/Engine.hpp"
#include "Core/ImGui/ImGuiCore.hpp"
#include "Core/Transform.hpp"

#include "Renderer/Resources/Mesh.hpp"

#include "Tools/Log.hpp"

// Jolt must always be included first
#include <Jolt/Jolt.h>

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

namespace Wild
{
    namespace
    {
        constexpr float kFixedTimeStep = 1.0f / 60.0f;
        constexpr int kMaxStepsPerFrame = 4;

        constexpr uint32_t kMaxBodies = 65536;
        constexpr uint32_t kNumBodyMutexes = 0;
        constexpr uint32_t kMaxBodyPairs = 65536;
        constexpr uint32_t kMaxContactConstraints = 10240;

        // Jolt requires at least one layer for static and one for moving bodies.
        namespace Layers
        {
            constexpr JPH::ObjectLayer NON_MOVING = 0;
            constexpr JPH::ObjectLayer MOVING = 1;
            constexpr JPH::ObjectLayer NUM_LAYERS = 2;
        } // namespace Layers

        namespace BroadPhaseLayers
        {
            constexpr JPH::BroadPhaseLayer NON_MOVING(0);
            constexpr JPH::BroadPhaseLayer MOVING(1);
            constexpr uint32_t NUM_LAYERS = 2;
        } // namespace BroadPhaseLayers

        class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
        {
          public:
            BPLayerInterfaceImpl()
            {
                m_objectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
                m_objectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
            }

            uint32_t GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }

            JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
            {
                return m_objectToBroadPhase[inLayer];
            }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
            const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
            {
                switch (static_cast<JPH::BroadPhaseLayer::Type>(inLayer))
                {
                case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::NON_MOVING): return "NON_MOVING";
                case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::MOVING): return "MOVING";
                default: return "INVALID";
                }
            }
#endif

          private:
            JPH::BroadPhaseLayer m_objectToBroadPhase[Layers::NUM_LAYERS];
        };

        class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
        {
          public:
            bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
            {
                switch (inLayer1)
                {
                case Layers::NON_MOVING: return inLayer2 == BroadPhaseLayers::MOVING;
                case Layers::MOVING: return true;
                default: return false;
                }
            }
        };

        class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
        {
          public:
            bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
            {
                switch (inObject1)
                {
                case Layers::NON_MOVING: return inObject2 == Layers::MOVING;
                case Layers::MOVING: return true;
                default: return false;
                }
            }
        };

        JPH::EMotionType ToJolt(MotionType type)
        {
            switch (type)
            {
            case MotionType::Static: return JPH::EMotionType::Static;
            case MotionType::Kinematic: return JPH::EMotionType::Kinematic;
            case MotionType::Dynamic: return JPH::EMotionType::Dynamic;
            }
            return JPH::EMotionType::Static;
        }

        JPH::Vec3 ToJolt(const glm::vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }
        JPH::Quat ToJolt(const glm::quat& q) { return JPH::Quat(q.x, q.y, q.z, q.w); }

        glm::vec3 FromJolt(JPH::Vec3Arg v) { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }
        glm::quat FromJolt(const JPH::Quat& q) { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }

        bool IsIdentity(const glm::quat& q) { return q.x == 0.0f && q.y == 0.0f && q.z == 0.0f && q.w == 1.0f; }

        // Same extraction Transform::SetFromMatrix already does, pulled out so physics
        // can decompose an arbitrary world matrix without adding a GLM GTX dependency.
        void DecomposeMatrix(const glm::mat4& m, glm::vec3& outPosition, glm::quat& outRotation, glm::vec3& outScale)
        {
            outPosition = glm::vec3(m[3]);

            outScale.x = glm::length(glm::vec3(m[0]));
            outScale.y = glm::length(glm::vec3(m[1]));
            outScale.z = glm::length(glm::vec3(m[2]));

            glm::mat3 rotationMatrix(glm::vec3(m[0]) / (outScale.x != 0.0f ? outScale.x : 1.0f),
                                      glm::vec3(m[1]) / (outScale.y != 0.0f ? outScale.y : 1.0f),
                                      glm::vec3(m[2]) / (outScale.z != 0.0f ? outScale.z : 1.0f));

            outRotation = glm::quat_cast(rotationMatrix);
        }

        void GetWorldPositionRotationScale(Transform& transform, glm::vec3& outPosition, glm::quat& outRotation, glm::vec3& outScale)
        {
            DecomposeMatrix(transform.GetWorldMatrix(), outPosition, outRotation, outScale);
        }

        // Physics only ever reads/writes position+rotation, never scale
        void WorldToLocal(entt::registry& registry, Transform& transform, const glm::vec3& worldPosition, const glm::quat& worldRotation,
                           glm::vec3& outLocalPosition, glm::quat& outLocalRotation)
        {
            if (!transform.HasParent())
            {
                outLocalPosition = worldPosition;
                outLocalRotation = worldRotation;
                return;
            }

            auto& parentTransform = registry.get<Transform>(transform.GetParent());
            glm::mat4 worldMatrix = glm::translate(glm::mat4(1.0f), worldPosition) * glm::toMat4(worldRotation);
            glm::mat4 localMatrix = glm::inverse(parentTransform.GetWorldMatrix()) * worldMatrix;

            glm::vec3 unusedScale;
            DecomposeMatrix(localMatrix, outLocalPosition, outLocalRotation, unusedScale);
        }

        void DrawWireBox(Renderer& renderer, const glm::mat4& shapeWorld, const glm::vec3& halfExtents, const glm::vec3& color)
        {
            glm::vec3 corners[8];
            int i = 0;
            for (float sx : {-1.0f, 1.0f})
                for (float sy : {-1.0f, 1.0f})
                    for (float sz : {-1.0f, 1.0f})
                        corners[i++] = glm::vec3(shapeWorld * glm::vec4(sx * halfExtents.x, sy * halfExtents.y, sz * halfExtents.z, 1.0f));

            constexpr int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                                           {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
            for (auto& edge : edges)
                renderer.AddLine(corners[edge[0]], corners[edge[1]], color);
        }

        // Draws a circle of the given radius in the shape's local XZ, XY, or YZ plane
        void DrawWireCircle(Renderer& renderer, const glm::mat4& shapeWorld, float radius, int axisA, int axisB,
                             float centerAlongThirdAxis, int thirdAxis, const glm::vec3& color, int segments = 24)
        {
            glm::vec3 prev{};
            for (int i = 0; i <= segments; ++i)
            {
                float angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(segments);
                glm::vec3 local{0.0f};
                local[axisA] = radius * cosf(angle);
                local[axisB] = radius * sinf(angle);
                local[thirdAxis] = centerAlongThirdAxis;

                glm::vec3 world = glm::vec3(shapeWorld * glm::vec4(local, 1.0f));
                if (i > 0) renderer.AddLine(prev, world, color);
                prev = world;
            }
        }

        void DrawWireSphere(Renderer& renderer, const glm::mat4& shapeWorld, float radius, const glm::vec3& color)
        {
            DrawWireCircle(renderer, shapeWorld, radius, 0, 2, 0.0f, 1, color); // XZ
            DrawWireCircle(renderer, shapeWorld, radius, 0, 1, 0.0f, 2, color); // XY
            DrawWireCircle(renderer, shapeWorld, radius, 1, 2, 0.0f, 0, color); // YZ
        }

        void DrawWireCapsule(Renderer& renderer, const glm::mat4& shapeWorld, float halfHeight, float radius, const glm::vec3& color)
        {
            DrawWireCircle(renderer, shapeWorld, radius, 0, 2, halfHeight, 1, color);
            DrawWireCircle(renderer, shapeWorld, radius, 0, 2, -halfHeight, 1, color);

            for (float angle : {0.0f, glm::half_pi<float>(), glm::pi<float>(), glm::pi<float>() * 1.5f})
            {
                glm::vec3 top = glm::vec3(shapeWorld * glm::vec4(radius * cosf(angle), halfHeight, radius * sinf(angle), 1.0f));
                glm::vec3 bottom = glm::vec3(shapeWorld * glm::vec4(radius * cosf(angle), -halfHeight, radius * sinf(angle), 1.0f));
                renderer.AddLine(top, bottom, color);
            }

            for (float sign : {1.0f, -1.0f})
            {
                glm::vec3 prev{};
                for (int i = 0; i <= 12; ++i)
                {
                    float angle = glm::pi<float>() * static_cast<float>(i) / 12.0f;
                    glm::vec3 local(radius * sinf(angle), sign * (halfHeight + radius * cosf(angle)), 0.0f);
                    glm::vec3 world = glm::vec3(shapeWorld * glm::vec4(local, 1.0f));
                    if (i > 0) renderer.AddLine(prev, world, color);
                    prev = world;
                }
            }
        }

        void DrawWireMesh(Renderer& renderer, const glm::mat4& shapeWorld, const Mesh& mesh, const glm::vec3& color)
        {
            const auto& positions = mesh.GetCollisionPositions();
            const auto& indices = mesh.GetCollisionIndices();

            for (size_t i = 0; i + 2 < indices.size(); i += 3)
            {
                glm::vec3 a = glm::vec3(shapeWorld * glm::vec4(positions[indices[i]], 1.0f));
                glm::vec3 b = glm::vec3(shapeWorld * glm::vec4(positions[indices[i + 1]], 1.0f));
                glm::vec3 c = glm::vec3(shapeWorld * glm::vec4(positions[indices[i + 2]], 1.0f));

                renderer.AddLine(a, b, color);
                renderer.AddLine(b, c, color);
                renderer.AddLine(c, a, color);
            }
        }

        void DrawColliderWireframe(Renderer& renderer, const ColliderShape& collider, const glm::mat4& entityWorld, Entity entity,
                                    const glm::vec3& color)
        {
            glm::mat4 shapeWorld = entityWorld * glm::translate(glm::mat4(1.0f), collider.offset) * glm::toMat4(collider.rotation);

            switch (collider.type)
            {
            case ColliderType::Box: DrawWireBox(renderer, shapeWorld, collider.boxHalfExtents, color); break;
            case ColliderType::Sphere: DrawWireSphere(renderer, shapeWorld, collider.sphereRadius, color); break;
            case ColliderType::Capsule:
                DrawWireCapsule(renderer, shapeWorld, collider.capsuleHalfHeight, collider.capsuleRadius, color);
                break;
            case ColliderType::ConvexHull:
            case ColliderType::Mesh:
            {
                if (!engine.GetECS()->HasComponent<MeshComponent>(entity)) return;
                auto meshPtr = engine.GetECS()->GetComponent<MeshComponent>(entity).mesh;
                if (meshPtr) DrawWireMesh(renderer, shapeWorld, *meshPtr, color);
                break;
            }
            }
        }

        // Builds one sub-shape's settings, baking in the entity's world scale
        JPH::Ref<JPH::ShapeSettings> BuildSubShapeSettings(const ColliderShape& collider, Entity entity, MotionType motionType,
                                                            const glm::vec3& worldScale)
        {
            switch (collider.type)
            {
            case ColliderType::Box: return new JPH::BoxShapeSettings(ToJolt(collider.boxHalfExtents * worldScale));
            case ColliderType::Sphere:
            {
                float uniformScale = (worldScale.x + worldScale.y + worldScale.z) / 3.0f;
                return new JPH::SphereShapeSettings(collider.sphereRadius * uniformScale);
            }
            case ColliderType::Capsule:
            {
                float uniformScale = (worldScale.x + worldScale.y + worldScale.z) / 3.0f;
                return new JPH::CapsuleShapeSettings(collider.capsuleHalfHeight * uniformScale, collider.capsuleRadius * uniformScale);
            }

            case ColliderType::ConvexHull:
            case ColliderType::Mesh:
            {
                if (!engine.GetECS()->HasComponent<MeshComponent>(entity))
                {
                    WD_WARN("RigidBody collider needs a MeshComponent to build a {} shape", collider.type == ColliderType::Mesh ? "Mesh" : "ConvexHull");
                    return nullptr;
                }

                auto meshPtr = engine.GetECS()->GetComponent<MeshComponent>(entity).mesh;
                if (!meshPtr) return nullptr;

                const auto& positions = meshPtr->GetCollisionPositions();
                if (positions.empty()) return nullptr;

                if (collider.type == ColliderType::ConvexHull)
                {
                    std::vector<JPH::Vec3> points;
                    points.reserve(positions.size());
                    for (const auto& p : positions)
                        points.emplace_back(p.x * worldScale.x, p.y * worldScale.y, p.z * worldScale.z);
                    return new JPH::ConvexHullShapeSettings(points.data(), static_cast<int>(points.size()));
                }

                if (motionType == MotionType::Dynamic)
                {
                    WD_WARN("Mesh collider is Static/Kinematic-only; skipping it on a Dynamic body");
                    return nullptr;
                }

                JPH::VertexList vertices;
                vertices.reserve(positions.size());
                for (const auto& p : positions)
                    vertices.emplace_back(p.x * worldScale.x, p.y * worldScale.y, p.z * worldScale.z);

                const auto& indices = meshPtr->GetCollisionIndices();
                JPH::IndexedTriangleList triangles;
                triangles.reserve(indices.size() / 3);
                for (size_t i = 0; i + 2 < indices.size(); i += 3)
                    triangles.emplace_back(indices[i], indices[i + 1], indices[i + 2], 0);

                return new JPH::MeshShapeSettings(vertices, triangles);
            }
            }
            return nullptr;
        }

        struct BuiltSubShape
        {
            glm::vec3 offset;
            glm::quat rotation;
            JPH::Ref<JPH::ShapeSettings> settings;
        };

        JPH::Ref<JPH::ShapeSettings> BuildBodyShapeSettings(Entity entity, RigidBody& rigidBody, const glm::vec3& worldScale)
        {
            std::vector<BuiltSubShape> built;
            for (const auto& collider : rigidBody.shapes)
            {
                auto settings = BuildSubShapeSettings(collider, entity, rigidBody.motionType, worldScale);
                if (!settings) continue;
                built.push_back({collider.offset * worldScale, collider.rotation, settings});
            }

            if (built.empty()) return nullptr;

            if (built.size() == 1)
            {
                const auto& shape = built[0];
                if (shape.offset == glm::vec3(0.0f) && IsIdentity(shape.rotation)) return shape.settings;
                return new JPH::RotatedTranslatedShapeSettings(ToJolt(shape.offset), ToJolt(shape.rotation), shape.settings.GetPtr());
            }

            auto* compound = new JPH::StaticCompoundShapeSettings();
            for (const auto& shape : built)
                compound->AddShape(ToJolt(shape.offset), ToJolt(shape.rotation), shape.settings.GetPtr());
            return compound;
        }
    } // namespace

    struct PhysicsSystem::Impl
    {
        BPLayerInterfaceImpl broadPhaseLayerInterface;
        ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter;
        ObjectLayerPairFilterImpl objectVsObjectLayerFilter;

        JPH::TempAllocatorImpl tempAllocator{10 * 1024 * 1024};
        JPH::JobSystemThreadPool jobSystem{JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
                                            std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1)};

        JPH::PhysicsSystem physicsSystem;

        Impl()
        {
            physicsSystem.Init(kMaxBodies, kNumBodyMutexes, kMaxBodyPairs, kMaxContactConstraints, broadPhaseLayerInterface,
                                objectVsBroadPhaseLayerFilter, objectVsObjectLayerFilter);
        }
    };

    PhysicsSystem::PhysicsSystem()
    {
        JPH::RegisterDefaultAllocator();

        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();

        m_impl = std::make_unique<Impl>();
    }

    PhysicsSystem::~PhysicsSystem()
    {
        m_impl.reset();

        JPH::UnregisterTypes();

        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    void PhysicsSystem::DestroyBody(RigidBody& rigidBody)
    {
        if (rigidBody.bodyId == kInvalidBodyId) return;

        auto& bodyInterface = m_impl->physicsSystem.GetBodyInterface();
        JPH::BodyID bodyId(rigidBody.bodyId);

        bodyInterface.RemoveBody(bodyId);
        bodyInterface.DestroyBody(bodyId);

        rigidBody.bodyId = kInvalidBodyId;
    }

    void PhysicsSystem::RebuildBody(Entity entity, RigidBody& rigidBody, Transform& transform)
    {
        DestroyBody(rigidBody);

        glm::vec3 worldPosition, worldScale;
        glm::quat worldRotation;
        GetWorldPositionRotationScale(transform, worldPosition, worldRotation, worldScale);

        auto shapeSettings = BuildBodyShapeSettings(entity, rigidBody, worldScale);
        if (!shapeSettings)
        {
            rigidBody.bodyDirty = false;
            rigidBody.lastSyncedScale = worldScale;
            rigidBody.lastSyncedPosition = worldPosition;
            rigidBody.lastSyncedRotation = worldRotation;
            return;
        }

        auto shapeResult = shapeSettings->Create();
        if (shapeResult.HasError())
        {
            WD_WARN("Failed to build collider shape: {}", shapeResult.GetError().c_str());
            rigidBody.bodyDirty = false;
            return;
        }

        JPH::ObjectLayer layer = rigidBody.motionType == MotionType::Static ? Layers::NON_MOVING : Layers::MOVING;

        JPH::BodyCreationSettings settings(shapeResult.Get(), ToJolt(worldPosition), ToJolt(worldRotation),
                                            ToJolt(rigidBody.motionType), layer);

        settings.mIsSensor = rigidBody.isTrigger;
        settings.mGravityFactor = rigidBody.useGravity ? 1.0f : 0.0f;
        settings.mLinearDamping = rigidBody.linearDamping;
        settings.mAngularDamping = rigidBody.angularDamping;

        if (!rigidBody.shapes.empty())
        {
            settings.mFriction = rigidBody.shapes[0].friction;
            settings.mRestitution = rigidBody.shapes[0].restitution;
        }

        if (rigidBody.motionType == MotionType::Dynamic && !rigidBody.autoMass)
        {
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = rigidBody.mass;
        }

        auto& bodyInterface = m_impl->physicsSystem.GetBodyInterface();
        JPH::BodyID bodyId = bodyInterface.CreateAndAddBody(settings, JPH::EActivation::Activate);

        rigidBody.bodyId = bodyId.GetIndexAndSequenceNumber();
        rigidBody.bodyDirty = false;
        rigidBody.lastSyncedScale = worldScale;
        rigidBody.lastSyncedPosition = worldPosition;
        rigidBody.lastSyncedRotation = worldRotation;
    }

    void PhysicsSystem::Update(float deltaTime)
    {
        auto ecs = engine.GetECS();
        auto& registry = ecs->GetRegistry();
        auto& bodyInterface = m_impl->physicsSystem.GetBodyInterface();

        for (auto entity : registry.view<RigidBody, Transform>())
        {
            auto& rigidBody = registry.get<RigidBody>(entity);
            auto& transform = registry.get<Transform>(entity);

            glm::vec3 worldPosition, worldScale;
            glm::quat worldRotation;
            GetWorldPositionRotationScale(transform, worldPosition, worldRotation, worldScale);

            if (rigidBody.bodyDirty || rigidBody.lastSyncedScale != worldScale)
            {
                RebuildBody(entity, rigidBody, transform);
                continue;
            }

            if (rigidBody.bodyId == kInvalidBodyId) continue;

            glm::vec3 positionDelta = worldPosition - rigidBody.lastSyncedPosition;
            bool movedExternally = glm::dot(positionDelta, positionDelta) > 1e-8f ||
                                    std::abs(glm::dot(worldRotation, rigidBody.lastSyncedRotation)) < 0.999999f;

            if (movedExternally)
            {
                JPH::BodyID bodyId(rigidBody.bodyId);
                bodyInterface.SetPositionAndRotation(bodyId, ToJolt(worldPosition), ToJolt(worldRotation), JPH::EActivation::Activate);

                // A manual reposition shouldn't carry over whatever velocity the body
                // had, or it'll visibly lurch right after the teleport.
                if (rigidBody.motionType == MotionType::Dynamic)
                {
                    bodyInterface.SetLinearVelocity(bodyId, JPH::Vec3::sZero());
                    bodyInterface.SetAngularVelocity(bodyId, JPH::Vec3::sZero());
                }

                rigidBody.lastSyncedPosition = worldPosition;
                rigidBody.lastSyncedRotation = worldRotation;
            }
        }

        m_accumulator += deltaTime;
        int steps = 0;
        while (m_accumulator >= kFixedTimeStep && steps < kMaxStepsPerFrame)
        {
            m_impl->physicsSystem.Update(kFixedTimeStep, 1, &m_impl->tempAllocator, &m_impl->jobSystem);
            m_accumulator -= kFixedTimeStep;
            ++steps;
        }
        if (steps == kMaxStepsPerFrame) m_accumulator = 0.0f;

        for (auto entity : registry.view<RigidBody, Transform>())
        {
            auto& rigidBody = registry.get<RigidBody>(entity);
            if (rigidBody.motionType != MotionType::Dynamic || rigidBody.bodyId == kInvalidBodyId) continue;

            auto& transform = registry.get<Transform>(entity);

            JPH::RVec3 position;
            JPH::Quat rotation;
            bodyInterface.GetPositionAndRotation(JPH::BodyID(rigidBody.bodyId), position, rotation);

            glm::vec3 worldPosition = FromJolt(position);
            glm::quat worldRotation = FromJolt(rotation);

            glm::vec3 localPosition;
            glm::quat localRotation;
            WorldToLocal(registry, transform, worldPosition, worldRotation, localPosition, localRotation);

            transform.SetPosition(localPosition);
            transform.SetRotation(localRotation);

            rigidBody.lastSyncedPosition = worldPosition;
            rigidBody.lastSyncedRotation = worldRotation;
        }

        DrawDebugShapes();
    }

    void PhysicsSystem::DrawDebugShapes()
    {
        auto& debugFlags = engine.GetImGui()->GetState().debug;
        if (!debugFlags.masterDebugDraw || !debugFlags.showColliders) return;

        auto renderer = engine.GetRenderer();
        if (!renderer) return;

        auto ecs = engine.GetECS();
        auto& registry = ecs->GetRegistry();

        for (auto entity : registry.view<RigidBody, Transform>())
        {
            auto& rigidBody = registry.get<RigidBody>(entity);
            if (rigidBody.bodyId == kInvalidBodyId) continue;

            glm::vec3 color = rigidBody.isTrigger ? glm::vec3(1.0f, 0.0f, 1.0f)
                               : rigidBody.motionType == MotionType::Dynamic ? glm::vec3(1.0f, 1.0f, 0.0f)
                               : rigidBody.motionType == MotionType::Kinematic ? glm::vec3(0.0f, 1.0f, 1.0f)
                                                                                : glm::vec3(0.0f, 1.0f, 0.0f);

            const glm::mat4& worldMatrix = registry.get<Transform>(entity).GetWorldMatrix();
            for (const auto& collider : rigidBody.shapes)
                DrawColliderWireframe(*renderer, collider, worldMatrix, entity, color);
        }
    }
} // namespace Wild
