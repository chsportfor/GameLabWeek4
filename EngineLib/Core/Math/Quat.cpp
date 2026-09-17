#include "Quat.h"

#include "Rotator.h"

// Convert this quaternion to a rotator.
// roll		= atan2(2 * (wx + yz), 1 - 2 * (x^2 + y^2))
// pitch	= asin(2 * (wy - zx))
// yaw		= atan2(2 * (wz + xy), 1 - 2 * (y^2 + z^2))
FRotator FQuat::Rotator() const
{
	float roll = std::atan2(2.0f * (w * x + y * z), 1.0f - 2.0f * (x * x + y * y));
	float pitch = std::asin(2.0f * (w * y - z * x));
	float yaw = std::atan2(2.0f * (w * z + x * y), 1.0f - 2.0f * (y * y + z * z));

	// Convert sign of pitch and roll to match Unreal Engine's coordinate system
	// Roll and pitch are turning clockwise, while yaw is turning counter-clockwise.
	return FRotator(-pitch * 180.0f / PI, yaw * 180.0f / PI, -roll * 180.0f / PI);
}
