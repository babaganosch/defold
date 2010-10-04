#include "comp_collision_object.h"

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/math.h>

#include <physics/physics.h>

#include "gamesys.h"
#include "../resources/res_collision_object.h"

#include "../proto/physics_ddf.h"

namespace dmGameSystem
{
    void GetWorldTransform(void* user_data, Vectormath::Aos::Point3& position, Vectormath::Aos::Quat& rotation)
    {
        if (!user_data)
            return;
        dmGameObject::HInstance instance = (dmGameObject::HInstance)user_data;
        position = dmGameObject::GetWorldPosition(instance);
        rotation = dmGameObject::GetWorldRotation(instance);
    }

    void SetWorldTransform(void* user_data, const Vectormath::Aos::Point3& position, const Vectormath::Aos::Quat& rotation)
    {
        if (!user_data)
            return;
        dmGameObject::HInstance instance = (dmGameObject::HInstance)user_data;
        dmGameObject::SetPosition(instance, position);
        dmGameObject::SetRotation(instance, rotation);
    }

    dmGameObject::CreateResult CompCollisionObjectNewWorld(void* context, void** world)
    {
        *world = dmPhysics::NewWorld(Vectormath::Aos::Point3(-1000, -1000, -1000), Vectormath::Aos::Point3(1000, 1000, 1000), &GetWorldTransform, &SetWorldTransform);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollisionObjectDeleteWorld(void* context, void* world)
    {
        dmPhysics::DeleteWorld((dmPhysics::HWorld)world);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollisionObjectCreate(dmGameObject::HCollection collection,
                                               dmGameObject::HInstance instance,
                                               void* resource,
                                               void* world,
                                               void* context,
                                               uintptr_t* user_data)
    {
        assert(user_data);

        CollisionObjectPrototype* collision_object_prototype = (CollisionObjectPrototype*) resource;
        dmPhysics::HWorld physics_world = (dmPhysics::HWorld) world;

        dmPhysics::HCollisionObject collision_object = dmPhysics::NewCollisionObject(physics_world, collision_object_prototype->m_CollisionShape, collision_object_prototype->m_Mass, collision_object_prototype->m_Type, collision_object_prototype->m_Group, collision_object_prototype->m_Mask, instance);
        *user_data = (uintptr_t) collision_object;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollisionObjectInit(dmGameObject::HCollection collection,
                                            dmGameObject::HInstance instance,
                                            void* context,
                                            uintptr_t* user_data)
    {
        assert(user_data);
        dmPhysics::HCollisionObject collision_object = (dmPhysics::HCollisionObject)*user_data;

        Point3 position = dmGameObject::GetWorldPosition(instance);
        Quat rotation = dmGameObject::GetWorldRotation(instance);

        dmPhysics::SetCollisionObjectInitialTransform(collision_object, position, rotation);

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollisionObjectDestroy(dmGameObject::HCollection collection,
                                                dmGameObject::HInstance instance,
                                                void* world,
                                                void* context,
                                                uintptr_t* user_data)
    {
        assert(user_data);
        dmPhysics::HWorld physics_world = (dmPhysics::HWorld) world;

        dmPhysics::HCollisionObject collision_object = (dmPhysics::HCollisionObject) *user_data;
        dmPhysics::DeleteCollisionObject(physics_world, collision_object);
        return dmGameObject::CREATE_RESULT_OK;
    }

    void CollisionCallback(void* user_data_a, uint16_t group_a, void* user_data_b, uint16_t group_b, void* user_data)
    {
        dmGameObject::HInstance instance_a = (dmGameObject::HInstance)user_data_a;
        dmGameObject::HInstance instance_b = (dmGameObject::HInstance)user_data_b;
        const uint32_t data_size = sizeof(dmPhysicsDDF::CollisionMessage) + 9;
        char data[data_size];
        dmPhysicsDDF::CollisionMessage* ddf = (dmPhysicsDDF::CollisionMessage*)&data;
        ddf->m_OtherGameObjectId = (char*)sizeof(dmPhysicsDDF::CollisionMessage);
        char* id = &data[sizeof(dmPhysicsDDF::CollisionMessage)];
        // Broadcast to A components
        DM_SNPRINTF(id, 9, "%X", dmGameObject::GetIdentifier(instance_b));
        ddf->m_Group = group_b;
        dmGameObject::PostDDFMessageTo(instance_a, 0x0, dmPhysicsDDF::CollisionMessage::m_DDFDescriptor, data);
        // Broadcast to B components
        DM_SNPRINTF(id, 9, "%X", dmGameObject::GetIdentifier(instance_a));
        ddf->m_Group = group_a;
        dmGameObject::PostDDFMessageTo(instance_b, 0x0, dmPhysicsDDF::CollisionMessage::m_DDFDescriptor, data);
    }

    void ContactPointCallback(const dmPhysics::ContactPoint& contact_point, void* user_data)
    {
        dmGameObject::HInstance instance_a = (dmGameObject::HInstance)contact_point.m_UserDataA;
        dmGameObject::HInstance instance_b = (dmGameObject::HInstance)contact_point.m_UserDataB;
        char data[sizeof(dmPhysicsDDF::ContactPointMessage) + 9];
        dmPhysicsDDF::ContactPointMessage* ddf = (dmPhysicsDDF::ContactPointMessage*)&data;
        ddf->m_OtherGameObjectId = (char*)sizeof(dmPhysicsDDF::ContactPointMessage);
        char* id = &data[sizeof(dmPhysicsDDF::ContactPointMessage)];
        float mass_a = dmMath::Select(-contact_point.m_InvMassA, 0.0f, 1.0f / contact_point.m_InvMassA);
        float mass_b = dmMath::Select(-contact_point.m_InvMassB, 0.0f, 1.0f / contact_point.m_InvMassB);
        // Broadcast to A components
        ddf->m_Position = contact_point.m_PositionA;
        ddf->m_Normal = -contact_point.m_Normal;
        ddf->m_RelativeVelocity = -contact_point.m_RelativeVelocity;
        ddf->m_Distance = contact_point.m_Distance;
        ddf->m_AppliedImpulse = contact_point.m_AppliedImpulse;
        ddf->m_Mass = mass_a;
        ddf->m_OtherMass = mass_b;
        DM_SNPRINTF(id, 9, "%X", dmGameObject::GetIdentifier(instance_b));
        ddf->m_Group = contact_point.m_GroupB;
        dmGameObject::PostDDFMessageTo(instance_a, 0x0, dmPhysicsDDF::ContactPointMessage::m_DDFDescriptor, data);
        // Broadcast to B components
        ddf->m_Position = contact_point.m_PositionB;
        ddf->m_Normal = contact_point.m_Normal;
        ddf->m_RelativeVelocity = contact_point.m_RelativeVelocity;
        ddf->m_Distance = contact_point.m_Distance;
        ddf->m_AppliedImpulse = contact_point.m_AppliedImpulse;
        ddf->m_Mass = mass_b;
        ddf->m_OtherMass = mass_a;
        DM_SNPRINTF(id, 9, "%X", dmGameObject::GetIdentifier(instance_a));
        ddf->m_Group = contact_point.m_GroupA;
        ddf->m_LifeTime = contact_point.m_LifeTime;
        dmGameObject::PostDDFMessageTo(instance_b, 0x0, dmPhysicsDDF::ContactPointMessage::m_DDFDescriptor, data);
    }

    dmGameObject::UpdateResult CompCollisionObjectUpdate(dmGameObject::HCollection collection,
                         const dmGameObject::UpdateContext* update_context,
                         void* world,
                         void* context)
    {
        dmPhysics::HWorld physics_world = (dmPhysics::HWorld) world;
        dmPhysics::StepWorld(physics_world, update_context->m_DT);
        dmPhysics::ForEachCollision(physics_world, CollisionCallback, 0x0, ContactPointCallback, 0x0);
        PhysicsContext* physics_context = (PhysicsContext*)context;
        if (physics_context->m_Debug)
            dmPhysics::DebugRender(physics_world);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompCollisionObjectOnMessage(dmGameObject::HInstance instance,
            const dmGameObject::InstanceMessageData* message_data,
            void* context,
            uintptr_t* user_data)
    {
        if (message_data->m_MessageId == dmHashString32(dmPhysicsDDF::ApplyForceMessage::m_DDFDescriptor->m_ScriptName))
        {
            dmPhysicsDDF::ApplyForceMessage* af = (dmPhysicsDDF::ApplyForceMessage*) message_data->m_Buffer;
            dmPhysics::HCollisionObject collision_object = (dmPhysics::HCollisionObject) *user_data;
            dmPhysics::ApplyForce(collision_object, af->m_Force, af->m_Position);
        }
        if (message_data->m_MessageId == dmHashString32(dmPhysicsDDF::VelocityRequest::m_DDFDescriptor->m_ScriptName))
        {
            dmPhysicsDDF::VelocityRequest* request = (dmPhysicsDDF::VelocityRequest*)message_data->m_Buffer;
            request->m_ClientId = (const char*)((uintptr_t)request + (uintptr_t)request->m_ClientId);
            uint32_t id;
            if (sscanf(request->m_ClientId, "%X", &id) > 0)
            {
                dmGameObject::HInstance client = dmGameObject::GetInstanceFromIdentifier(dmGameObject::GetCollection(instance), id);
                dmPhysicsDDF::VelocityResponse response;
                dmPhysics::HCollisionObject collision_object = (dmPhysics::HCollisionObject)*user_data;
                response.m_LinearVelocity = dmPhysics::GetLinearVelocity(collision_object);
                response.m_AngularVelocity = dmPhysics::GetAngularVelocity(collision_object);
                dmGameObject::PostDDFMessageTo(client, 0x0, dmPhysicsDDF::VelocityResponse::m_DDFDescriptor, (char*)&response);
            }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }
}
