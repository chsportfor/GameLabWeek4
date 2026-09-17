#pragma once

#include "ThirdParty/Json/json.hpp"
#include "Core/Math/Vector.h"
#include "Core/Math/Rotator.h"
#include "Core/enum.h"

json::JSON FVectorToJson(const FVector& Vector);
json::JSON FRotatorToJson(const FRotator& Rotator);
json::JSON EPrimitiveToJson(const EPrimitive& Primitive);

FVector FVectorFromJson(const json::JSON& json);
FRotator FRotatorFromJson(const json::JSON& json);
EPrimitive EPrimitiveFromJson(const json::JSON& json);
