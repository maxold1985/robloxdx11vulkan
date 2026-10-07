#pragma once

#include <cmath>

namespace DirectX
{
	struct XMFLOAT3
	{
		float x;
		float y;
		float z;

		constexpr XMFLOAT3()
			: x(0.0f),
			  y(0.0f),
			  z(0.0f)
		{
		}

		constexpr XMFLOAT3(
			float xValue,
			float yValue,
			float zValue
		)
			: x(xValue),
			  y(yValue),
			  z(zValue)
		{
		}
	};

	struct XMFLOAT4
	{
		float x;
		float y;
		float z;
		float w;

		constexpr XMFLOAT4()
			: x(0.0f),
			  y(0.0f),
			  z(0.0f),
			  w(0.0f)
		{
		}

		constexpr XMFLOAT4(
			float xValue,
			float yValue,
			float zValue,
			float wValue
		)
			: x(xValue),
			  y(yValue),
			  z(zValue),
			  w(wValue)
		{
		}
	};

	struct XMFLOAT4X4
	{
		float m[4][4];

		constexpr XMFLOAT4X4()
			: m{
				{0.0f, 0.0f, 0.0f, 0.0f},
				{0.0f, 0.0f, 0.0f, 0.0f},
				{0.0f, 0.0f, 0.0f, 0.0f},
				{0.0f, 0.0f, 0.0f, 0.0f}
			}
		{
		}
	};

	struct XMVECTOR
	{
		float x;
		float y;
		float z;
		float w;

		constexpr XMVECTOR()
			: x(0.0f),
			  y(0.0f),
			  z(0.0f),
			  w(0.0f)
		{
		}

		constexpr XMVECTOR(
			float xValue,
			float yValue,
			float zValue,
			float wValue
		)
			: x(xValue),
			  y(yValue),
			  z(zValue),
			  w(wValue)
		{
		}
	};

	struct XMMATRIX
	{
		float m[4][4];

		constexpr XMMATRIX()
			: m{
				{0.0f, 0.0f, 0.0f, 0.0f},
				{0.0f, 0.0f, 0.0f, 0.0f},
				{0.0f, 0.0f, 0.0f, 0.0f},
				{0.0f, 0.0f, 0.0f, 0.0f}
			}
		{
		}
	};

	inline constexpr float XM_PI =
		3.14159265358979323846f;

	inline float XMConvertToRadians(
		float degrees
	)
	{
		return
			degrees *
			(XM_PI / 180.0f);
	}

	inline XMVECTOR XMVectorSet(
		float x,
		float y,
		float z,
		float w
	)
	{
		return XMVECTOR(
			x,
			y,
			z,
			w
		);
	}

	inline float XMVector3Dot(
		const XMVECTOR& a,
		const XMVECTOR& b
	)
	{
		return
			a.x * b.x +
			a.y * b.y +
			a.z * b.z;
	}

	inline XMVECTOR XMVector3Cross(
		const XMVECTOR& a,
		const XMVECTOR& b
	)
	{
		return XMVECTOR(
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x,
			0.0f
		);
	}

	inline XMVECTOR XMVector3Subtract(
		const XMVECTOR& a,
		const XMVECTOR& b
	)
	{
		return XMVECTOR(
			a.x - b.x,
			a.y - b.y,
			a.z - b.z,
			0.0f
		);
	}

	inline XMVECTOR XMVector3Normalize(
		const XMVECTOR& value
	)
	{
		const float lengthSquared =
			value.x * value.x +
			value.y * value.y +
			value.z * value.z;

		if (lengthSquared <= 1.0e-20f)
		{
			return XMVECTOR(
				0.0f,
				0.0f,
				0.0f,
				value.w
			);
		}

		const float inverseLength =
			1.0f /
			std::sqrt(lengthSquared);

		return XMVECTOR(
			value.x * inverseLength,
			value.y * inverseLength,
			value.z * inverseLength,
			value.w
		);
	}

	inline XMMATRIX XMMatrixIdentity()
	{
		XMMATRIX result;

		result.m[0][0] = 1.0f;
		result.m[1][1] = 1.0f;
		result.m[2][2] = 1.0f;
		result.m[3][3] = 1.0f;

		return result;
	}

	inline XMMATRIX XMMatrixScaling(
		float scaleX,
		float scaleY,
		float scaleZ
	)
	{
		XMMATRIX result;

		result.m[0][0] = scaleX;
		result.m[1][1] = scaleY;
		result.m[2][2] = scaleZ;
		result.m[3][3] = 1.0f;

		return result;
	}

	inline XMMATRIX XMMatrixTranslation(
		float translationX,
		float translationY,
		float translationZ
	)
	{
		XMMATRIX result =
			XMMatrixIdentity();

		// DirectX/HLSL row-vector convention:
		// mul(float4(position, 1), matrix)
		result.m[3][0] = translationX;
		result.m[3][1] = translationY;
		result.m[3][2] = translationZ;

		return result;
	}

	inline XMMATRIX operator*(
		const XMMATRIX& a,
		const XMMATRIX& b
	)
	{
		XMMATRIX result;

		for (int row = 0; row < 4; ++row)
		{
			for (
				int column = 0;
				column < 4;
				++column
			)
			{
				float value = 0.0f;

				for (
					int index = 0;
					index < 4;
					++index
				)
				{
					value +=
						a.m[row][index] *
						b.m[index][column];
				}

				result.m[row][column] =
					value;
			}
		}

		return result;
	}

	inline XMMATRIX XMMatrixLookAtLH(
		const XMVECTOR& eyePosition,
		const XMVECTOR& focusPosition,
		const XMVECTOR& upDirection
	)
	{
		const XMVECTOR zAxis =
			XMVector3Normalize(
				XMVector3Subtract(
					focusPosition,
					eyePosition
				)
			);

		const XMVECTOR xAxis =
			XMVector3Normalize(
				XMVector3Cross(
					upDirection,
					zAxis
				)
			);

		const XMVECTOR yAxis =
			XMVector3Cross(
				zAxis,
				xAxis
			);

		XMMATRIX result;

		result.m[0][0] = xAxis.x;
		result.m[0][1] = yAxis.x;
		result.m[0][2] = zAxis.x;
		result.m[0][3] = 0.0f;

		result.m[1][0] = xAxis.y;
		result.m[1][1] = yAxis.y;
		result.m[1][2] = zAxis.y;
		result.m[1][3] = 0.0f;

		result.m[2][0] = xAxis.z;
		result.m[2][1] = yAxis.z;
		result.m[2][2] = zAxis.z;
		result.m[2][3] = 0.0f;

		result.m[3][0] =
			-XMVector3Dot(
				xAxis,
				eyePosition
			);

		result.m[3][1] =
			-XMVector3Dot(
				yAxis,
				eyePosition
			);

		result.m[3][2] =
			-XMVector3Dot(
				zAxis,
				eyePosition
			);

		result.m[3][3] = 1.0f;

		return result;
	}

	inline XMMATRIX XMMatrixPerspectiveFovLH(
		float fieldOfViewY,
		float aspectRatio,
		float nearZ,
		float farZ
	)
	{
		if (aspectRatio <= 0.0f)
			aspectRatio = 1.0f;

		if (nearZ <= 0.0f)
			nearZ = 0.001f;

		if (farZ <= nearZ)
			farZ = nearZ + 1.0f;

		const float yScale =
			1.0f /
			std::tan(
				fieldOfViewY * 0.5f
			);

		const float xScale =
			yScale / aspectRatio;

		const float range =
			farZ /
			(farZ - nearZ);

		XMMATRIX result;

		result.m[0][0] = xScale;
		result.m[1][1] = yScale;
		result.m[2][2] = range;
		result.m[2][3] = 1.0f;
		result.m[3][2] =
			-nearZ * range;

		return result;
	}

	inline void XMStoreFloat4x4(
		XMFLOAT4X4* destination,
		const XMMATRIX& source
	)
	{
		if (!destination)
			return;

		for (int row = 0; row < 4; ++row)
		{
			for (
				int column = 0;
				column < 4;
				++column
			)
			{
				destination
					->m[row][column] =
					source.m[row][column];
			}
		}
	}
}
