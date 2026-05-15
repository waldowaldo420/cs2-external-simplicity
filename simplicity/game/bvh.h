#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <dwmapi.h>
#include <windowsx.h>

#include <array>
#include <vector>
#include <numbers>
#include <algorithm>
#include <format>
#include <shared_mutex>
#include <unordered_set>

#include "../utils/math.h"
#include "../memory/memory.h"
#include "../game/game.h"

#undef min
#undef max

namespace game { 
	class bvh
	{
	public:
		struct surface_info
		{
			float penetration{};
			std::uint16_t surface_type{};
			std::uint8_t global_index{ 255 };
		};

		struct global_surface_entry
		{
			float unk_00{};
			float unk_04{};
			float penetration_mod{};
			float unk_0C{};
			float unk_10{};
			std::uint16_t surface_type{};
			std::uint16_t pad{};
			std::uint8_t pad2[8]{};
		};

		struct triangle
		{
			math::Vector3 v0{};
			math::Vector3 v1{};
			math::Vector3 v2{};
			surface_info surface{};
		};

		struct trace_result
		{
			bool hit{};
			float fraction{};
			float distance{};
			math::Vector3 end_pos{};
			math::Vector3 normal{};
			surface_info surface{};
			std::int32_t triangle_index{ -1 };
		};

		struct hit_entry
		{
			float distance{};
			float fraction{};
			math::Vector3 position{};
			math::Vector3 normal{};
			surface_info surface{};
			std::int32_t triangle_index{ -1 };
			bool is_enter{ true };
		};

		struct penetration_segment
		{
			float enter_fraction{};
			float exit_fraction{};
			float enter_distance{};
			float exit_distance{};
			math::Vector3 enter_pos{};
			math::Vector3 exit_pos{};
			surface_info enter_surface{};
			surface_info exit_surface{};
			float thickness{};
			float min_pen_mod{};
		};

		void parse();
		void clear();

		[[nodiscard]] trace_result trace_ray(const math::Vector3& start, const math::Vector3& end, std::int32_t exclude_tri = -1) const;
		[[nodiscard]] std::vector<hit_entry> trace_ray_all(const math::Vector3& start, const math::Vector3& end) const;
		[[nodiscard]] std::vector<penetration_segment> build_segments(const std::vector<hit_entry>& hits, float ray_length) const;

		[[nodiscard]] bool CanPenetrate(const math::Vector3& eye,
			const math::Vector3& target,
			float weaponDamage = 100.f,
			float weaponPen = 1.f,
			float minDamage = 1.f) const;

		struct pen_result
		{
			float damage = 0.f;
			bool  penetrated = false; 
		};

		[[nodiscard]] pen_result CalcPenetratedDamage(const math::Vector3& eye,
			const math::Vector3& target,
			float weaponDamage = 100.f,
			float weaponPen = 1.f,
			float rangeModifier = 0.97f,
			float maxRange = 8192.f) const;

		static float ScaleDamage(float damage, int hitgroup, int armor, bool hasHelmet,
			float armorRatio, float headshotMultiplier) noexcept;

		[[nodiscard]] const std::vector<triangle>& triangles() const;
		[[nodiscard]] std::size_t count() const;
		[[nodiscard]] bool valid() const;
		[[nodiscard]] bool valid_unsafe() const noexcept { return !m_nodes.empty(); }

	private:
		struct aabb
		{
			float mins[3]{ 1e12f, 1e12f, 1e12f };
			float maxs[3]{ -1e12f, -1e12f, -1e12f };

			void expand(const math::Vector3& p);
			void expand(const aabb& o);
			[[nodiscard]] int longest_axis() const;
			[[nodiscard]] bool intersects_ray(const float origin[3], const float inv_dir[3], float max_t) const;
		};

		struct bvh_node
		{
			aabb bounds{};
			std::int32_t left{ -1 };
			std::int32_t right{ -1 };
			std::int32_t tri_start{};
			std::int32_t tri_count{};
		};

		void rebuild_accel();
		std::int32_t build_recursive(std::int32_t start, std::int32_t end, std::int32_t depth);

		std::vector<triangle> m_triangles{};
		mutable std::shared_mutex  m_mutex{};

		std::vector<bvh_node> m_nodes{};
		std::vector<std::int32_t> m_indices{};
		std::vector<aabb> m_tri_bounds{};
		std::vector<float> m_centroids{};

		static constexpr auto k_max_leaf_tris{ 8 };
		static constexpr auto k_max_depth{ 48 };
	};
}