#pragma once

#include "core/types.hpp"

namespace transformer
{

Vec4 multiply(const Mat4& matrix, const Vec4& vector);
Mat4 multiply(const Mat4& a, const Mat4& b);
Vec3 divide_by_w(const Vec4& vector);

} // namespace transformer
