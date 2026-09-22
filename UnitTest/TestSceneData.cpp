#include "pch.h"
#include "Engine/Serialization/SceneData.h"
#include "ThirdParty/Json/json.hpp"

TEST(TestCameraData, RoundTripPreservesCamera)
{
    FCameraData Source;
    Source.Location = FVector(1.f, 2.f, 3.f);
    Source.Rotation = FRotator(10.f, 20.f, 30.f);
    Source.FOV = 75.f;
    Source.NearClip = 0.25f;
    Source.FarClip = 500.f;
    const FCameraData Restored(json::JSON::Load(Source.ToJsonString().CStr()));
    EXPECT_FLOAT_EQ(Restored.Location.x, Source.Location.x);
    EXPECT_FLOAT_EQ(Restored.Location.y, Source.Location.y);
    EXPECT_FLOAT_EQ(Restored.Location.z, Source.Location.z);
    EXPECT_FLOAT_EQ(Restored.Rotation.Pitch, Source.Rotation.Pitch);
    EXPECT_FLOAT_EQ(Restored.Rotation.Yaw, Source.Rotation.Yaw);
    EXPECT_FLOAT_EQ(Restored.Rotation.Roll, Source.Rotation.Roll);
    EXPECT_FLOAT_EQ(Restored.FOV, Source.FOV);
    EXPECT_FLOAT_EQ(Restored.NearClip, Source.NearClip);
    EXPECT_FLOAT_EQ(Restored.FarClip, Source.FarClip);
}
