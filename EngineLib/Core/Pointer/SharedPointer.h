#pragma once

#include <memory>
#include <utility>

template <typename T>
using TSharedPtr = std::shared_ptr<T>;

template <typename T, typename... Args>
TSharedPtr<T> MakeShared(Args&&... args)
{
	return std::make_shared<T>(std::forward<Args>(args)...);
}
