#include "core/math_utils.hpp"

#include <cmath>

namespace transformer
{

	Vec4 multiply(const Mat4& matrix, const Vec4& vector)
	{
		// Column-major mat4 * vec4.
		Vec4 result{};
		result.x = matrix.m[0] * vector.x + matrix.m[4] * vector.y + matrix.m[8] * vector.z + matrix.m[12] * vector.w;
		result.y = matrix.m[1] * vector.x + matrix.m[5] * vector.y + matrix.m[9] * vector.z + matrix.m[13] * vector.w;
		result.z = matrix.m[2] * vector.x + matrix.m[6] * vector.y + matrix.m[10] * vector.z + matrix.m[14] * vector.w;
		result.w = matrix.m[3] * vector.x + matrix.m[7] * vector.y + matrix.m[11] * vector.z + matrix.m[15] * vector.w;
		return result;
	}

	Mat4 multiply(const Mat4& a, const Mat4& b)
	{
		Mat4 result{};

		for (int row = 0; row < 4; ++row)
		{
			for (int col = 0; col < 4; ++col)
			{
				float sum = 0.0F;
				for (int k = 0; k < 4; ++k)
				{
					const float a_row_k = a.m[k * 4 + row];
					const float b_k_col = b.m[col * 4 + k];
					sum += a_row_k * b_k_col;
				}
				result.m[col * 4 + row] = sum;
			}
		}

		return result;
	}

	Vec3 divide_by_w(const Vec4& vector)
	{
		constexpr float epsilon = 1.0e-6F;
		if (std::fabs(vector.w) < epsilon)
		{
			return Vec3{vector.x, vector.y, vector.z};
		}

		return Vec3{vector.x / vector.w, vector.y / vector.w, vector.z / vector.w};
	}

}  // namespace transformer
