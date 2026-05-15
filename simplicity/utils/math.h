#pragma once
#include <cmath>
#include <array>

namespace math
{
    struct Vector2
    {
        float x = 0.f;
        float y = 0.f;

        Vector2 operator+(const Vector2& o) const { return { x + o.x, y + o.y }; }
        Vector2 operator-(const Vector2& o) const { return { x - o.x, y - o.y }; }
        Vector2 operator*(float s) const { return { x * s,   y * s }; }

        float length() const { return std::sqrtf(x * x + y * y); }
        float length_sq() const { return x * x + y * y; }
    };

    struct Vector3
    {
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;

        Vector3 operator+(const Vector3& o) const { return { x + o.x, y + o.y, z + o.z }; }
        Vector3 operator-(const Vector3& o) const { return { x - o.x, y - o.y, z - o.z }; }
        Vector3 operator*(float s) const { return { x * s,   y * s,   z * s }; }

        float length() const { return std::sqrtf(x * x + y * y + z * z); }
        float length_sq() const { return x * x + y * y + z * z; }
        float length_2d() const { return std::sqrtf(x * x + y * y); }

        Vector3& normalize()
        {
            const float len = length();
            if (len > 1e-6f) { x /= len; y /= len; z /= len; }
            return *this;
        }

        float dot(const Vector3& o)   const { return x * o.x + y * o.y + z * o.z; }

        Vector3 cross(const Vector3& o) const
        {
            return {
                y * o.z - z * o.y,
                z * o.x - x * o.z,
                x * o.y - y * o.x
            };
        }

        float dist_to(const Vector3& o) const
        {
            return (*this - o).length();
        }

        bool is_zero() const
        {
            return x == 0.f && y == 0.f && z == 0.f;
        }
    };

    struct Matrix4x4
    {
        float m[4][4]{};

        float& at(int r, int c) { return m[r][c]; }
        float  at(int r, int c) const { return m[r][c]; }
    };

    inline bool world_to_screen(
        const Vector3& world,
        const Matrix4x4& view_matrix,
        int screen_w,
        int screen_h,
        Vector2& screen_out)
    {
        const float* m = &view_matrix.m[0][0];

        const float clip_x = m[0] * world.x + m[1] * world.y + m[2] * world.z + m[3];
        const float clip_y = m[4] * world.x + m[5] * world.y + m[6] * world.z + m[7];
        const float clip_w = m[12] * world.x + m[13] * world.y + m[14] * world.z + m[15];

        if (clip_w <= 0.1f) return false;

        const float ndc_x = clip_x / clip_w;
        const float ndc_y = -clip_y / clip_w;

        screen_out.x = (screen_w * 0.5f) + (ndc_x * screen_w * 0.5f);
        screen_out.y = (screen_h * 0.5f) + (ndc_y * screen_h * 0.5f);

        return true;
    }

    inline Vector3 calc_angle(const Vector3& src, const Vector3& dst)
    {
        const Vector3 delta = dst - src;
        const float dist_2d = delta.length_2d();

        const float yaw = atan2f(delta.y, delta.x) * (180.f / 3.14159265f);
        const float pitch = -atan2f(delta.z, dist_2d) * (180.f / 3.14159265f);

        return { pitch, yaw, 0.f };
    }

    inline float normalize_angle(float a)
    {
        while (a > 180.f) a -= 360.f;
        while (a < -180.f) a += 360.f;
        return a;
    }

    template<typename T>
    inline T clamp(T val, T lo, T hi)
    {
        return val < lo ? lo : val > hi ? hi : val;
    }

    inline float lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }
}