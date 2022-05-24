#pragma once

#include <math.h>

#include "common.hpp"

namespace vkr {
	template <typename T>
	T to_rad(T deg) {
		return deg * (((T)3.14159265358979323846) / 180.0f);
	}

	template <typename T>
	T to_deg(T rad) {
		return rad * (180.0f / ((T)3.14159265358979323846));
	}

	template <typename T>
	struct v2 {
		T x, y;

		v2() : x(0), y(0) {}
		v2(T xy) : x(xy), y(xy) {}
		v2(T x, T y) : x(x), y(y) {}

		v2<T> operator+(const v2<T>& other) const {
			return v2<T>(x + other.x, y + other.y);
		}

		v2<T> operator-(const v2<T>& other) const {
			return v2<T>(x - other.x, y - other.y);
		}

		v2<T> operator*(const v2<T>& other) const {
			return v2<T>(x * other.x, y * other.y);
		}

		v2<T> operator/(const v2<T>& other) const {
			return v2<T>(x / other.x, y / other.y);
		}

		v2<T> operator+(T other) const {
			return v2<T>(x + other, y + other);
		}

		v2<T> operator-(T other) const {
			return v2<T>(x - other, y - other);
		}

		v2<T> operator*(T other) const {
			return v2<T>(x * other, y * other);
		}

		v2<T> operator/(T other) const {
			return v2<T>(x / other, y / other);
		}

		v2<T> operator+=(const v2<T>& other) {
			*this = *this + other;
			return *this;
		}

		v2<T> operator-=(const v2<T>& other) {
			*this = *this - other;
			return *this;
		}

		v2<T> operator*=(const v2<T>& other) {
			*this = *this * other;
			return *this;
		}

		v2<T> operator/=(const v2<T>& other) {
			*this = *this / other;
			return *this;
		}

		v2<T> operator+=(T other) {
			*this = *this + other;
			return *this;
		}

		v2<T> operator-=(T other) {
			*this = *this - other;
			return *this;
		}

		v2<T> operator*=(T other) {
			*this = *this * other;
			return *this;
		}

		v2<T> operator/=(T other) {
			*this = *this / other;
			return *this;
		}

		bool operator==(const v2<T>& other) const {
			return x == other.x && y == other.y;
		}

		bool operator!=(const v2<T>& other) const {
			return !(*this == other);
		}

		static T dot(const v2<T>& a, const v2<T>& b) {
			return a.x * b.x + a.y * b.y;
		}

		static T mag_sqrd(const v2<T>& v) {
			return v2<T>::dot(v, v);
		}

		static T mag(const v2<T>& v) {
			return (T)sqrtf((f32)v2<T>::mag_sqrd(v));
		}

		static v2<T> normalised(const v2<T>& v) {
			const T l = v2<T>::mag(v);
			return v2<T>(v.x / l, v.y / l);
		}
	};

	template <typename T>
	v2<T> operator+(T lhs, v2<T> rhs) {
		return v2<T>(lhs + rhs.x, lhs + rhs.y);
	}

	template <typename T>
	v2<T> operator-(T lhs, v2<T> rhs) {
		return v2<T>(lhs - rhs.x, lhs - rhs.y);
	}

	template <typename T>
	v2<T> operator*(T lhs, v2<T> rhs) {
		return v2<T>(lhs * rhs.x, lhs * rhs.y);
	}

	template <typename T>
	v2<T> operator/(T lhs, v2<T> rhs) {
		return v2<T>(lhs / rhs.x, lhs / rhs.y);
	}

	template <typename T>
	v2<T> operator-(const v2<T>& v) {
		return v2<T>(-v.x, -v.y);
	}

	template <typename T>
	struct v3 {
		T x, y, z;

		v3() : x(0), y(0), z(0) {}
		v3(T xyz) : x(xyz), y(xyz), z(xyz) {}
		v3(v2<T> xy, T z) : x(xy.x), y(xy.y), z(z) {}
		v3(T x, v2<T> yz) : x(x), y(yz.x), z(yz.y) {}
		v3(T x, T y, T z) : x(x), y(y), z(z) {}

		v3<T> operator+(const v3<T>& other) const {
			return v3<T>(x + other.x, y + other.y, z + other.z);
		}

		v3<T> operator-(const v3<T>& other) const {
			return v3<T>(x - other.x, y - other.y, z - other.z);
		}

		v3<T> operator*(const v3<T>& other) const {
			return v3<T>(x * other.x, y * other.y, z * other.z);
		}

		v3<T> operator/(const v3<T>& other) const {
			return v3<T>(x / other.x, y / other.y, z / other.z);
		}

		v3<T> operator+(T other) const {
			return v3<T>(x + other, y + other, z + other);
		}

		v3<T> operator-(T other) const {
			return v3<T>(x - other, y - other, z - other);
		}

		v3<T> operator*(T other) const {
			return v3<T>(x * other, y * other, z * other);
		}

		v3<T> operator/(T other) const {
			return v3<T>(x / other, y / other, z / other);
		}

		v3<T> operator+=(const v3<T>& other) {
			*this = *this + other;
			return *this;
		}

		v3<T> operator-=(const v3<T>& other) {
			*this = *this - other;
			return *this;
		}

		v3<T> operator*=(const v3<T>& other) {
			*this = *this * other;
			return *this;
		}

		v3<T> operator/=(const v3<T>& other) {
			*this = *this / other;
			return *this;
		}

		v3<T> operator+=(T other) {
			*this = *this + other;
			return *this;
		}

		v3<T> operator-=(T other) {
			*this = *this - other;
			return *this;
		}

		v3<T> operator*=(T other) {
			*this = *this * other;
			return *this;
		}

		v3<T> operator/=(T other) {
			*this = *this / other;
			return *this;
		}

		bool operator==(const v3<T>& other) const {
			return x == other.x && y == other.y && z == other.z;
		}

		bool operator!=(const v3<T>& other) const {
			return !(*this == other);
		}

		static T dot(const v3<T>& a, const v3<T>& b) {
			return a.x * b.x + a.y * b.y + a.z * b.z;
		}

		static v3<T> cross(const v3<T>& a, const v3<T>& b) {
			return v3<T>(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
		}

		static T mag_sqrd(const v3<T>& v) {
			return v3<T>::dot(v, v);
		}

		static T mag(const v3<T>& v) {
			return (T)sqrtf((f32)v3<T>::mag_sqrd(v));
		}

		static v3<T> normalised(const v3<T>& v) {
			const T l = v3<T>::mag(v);
			return v3<T>(v.x / l, v.y / l, v.z / l);
		}
	};

	template <typename T>
	v3<T> operator+(T lhs, v3<T> rhs) {
		return v3<T>(lhs + rhs.x, lhs + rhs.y, lhs + rhs.z);
	}

	template <typename T>
	v3<T> operator-(T lhs, v3<T> rhs) {
		return v3<T>(lhs - rhs.x, lhs - rhs.y, lhs - rhs.z);
	}

	template <typename T>
	v3<T> operator*(T lhs, v3<T> rhs) {
		return v3<T>(lhs * rhs.x, lhs * rhs.y, lhs * rhs.z);
	}

	template <typename T>
	v3<T> operator/(T lhs, v3<T> rhs) {
		return v3<T>(lhs / rhs.x, lhs / rhs.y, lhs / rhs.z);
	}

	template <typename T>
	v3<T> operator-(const v3<T>& v) {
		return v3<T>(-v.x, -v.y, -v.z);
	}

	template <typename T>
	struct v4 {
		T x, y, z, w;

		v4() : x(0), y(0), z(0), w(0) {}
		v4(T xyzw) : x(xyzw), y(xyzw), z(xyzw), w(xyzw) {}
		v4(v2<T> xy, T z, T w) : x(xy.x), y(xy.y), z(z), w(w) {}
		v4(v3<T> xyz, T w) : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}
		v4(T x, v3<T> yzw) : x(x), y(yzw.x), z(yzw.y), w(yzw.z) {}
		v4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}

		v4<T> operator+(const v4<T>& other) const {
			return v4<T>(x + other.x, y + other.y, z + other.z, w + other.w);
		}

		v4<T> operator-(const v4<T>& other) const {
			return v4<T>(x - other.x, y - other.y, z - other.z, w - other.w);
		}

		v4<T> operator*(const v4<T>& other) const {
			return v4<T>(x * other.x, y * other.y, z * other.z, w * other.w);
		}

		v4<T> operator/(const v4<T>& other) const {
			return v4<T>(x / other.x, y / other.y, z / other.z, w / other.w);
		}

		v4<T> operator+(T other) const {
			return v4<T>(x + other, y + other, z + other, w + other);
		}

		v4<T> operator-(T other) const {
			return v4<T>(x - other, y - other, z - other, w - other);
		}

		v4<T> operator*(T other) const {
			return v4<T>(x * other, y * other, z * other, w * other);
		}

		v4<T> operator/(T other) const {
			return v4<T>(x / other, y / other, z / other, w / other);
		}

		v4<T> operator+=(const v4<T>& other) {
			*this = *this + other;
			return *this;
		}

		v4<T> operator-=(const v4<T>& other) {
			*this = *this - other;
			return *this;
		}

		v4<T> operator*=(const v4<T>& other) {
			*this = *this * other;
			return *this;
		}

		v4<T> operator/=(const v4<T>& other) {
			*this = *this / other;
			return *this;
		}

		v4<T> operator+=(T other) {
			*this = *this + other;
			return *this;
		}

		v4<T> operator-=(T other) {
			*this = *this - other;
			return *this;
		}

		v4<T> operator*=(T other) {
			*this = *this * other;
			return *this;
		}

		v4<T> operator/=(T other) {
			*this = *this / other;
			return *this;
		}

		bool operator==(const v4<T>& other) const {
			return x == other.x && y == other.y && z == other.z && w == other.w;
		}

		bool operator!=(const v4<T>& other) const {
			return !(*this == other);
		}

		static T dot(const v4<T>& a, const v4<T>& b) {
			return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
		}

		static T mag_sqrd(const v4<T>& v) {
			return v4<T>::dot(v, v);
		}

		static T mag(const v4<T>& v) {
			return (T)sqrtf((f32)v4<T>::mag_sqrd(v));
		}

		static v4<T> normalised(const v4<T>& v) {
			const T l = v4<T>::mag(v);
			return v4<T>(v.x / l, v.y / l, v.z / l, v.w / l);
		}
	};

	template <typename T>
	v4<T> operator+(T lhs, v4<T> rhs) {
		return v4<T>(lhs + rhs.x, lhs + rhs.y, lhs + rhs.z, lhs + rhs.w);
	}

	template <typename T>
	v4<T> operator-(T lhs, v4<T> rhs) {
		return v4<T>(lhs - rhs.x, lhs - rhs.y, lhs - rhs.z, lhs - rhs.w);
	}

	template <typename T>
	v4<T> operator*(T lhs, v4<T> rhs) {
		return v4<T>(lhs * rhs.x, lhs * rhs.y, lhs * rhs.z, lhs - rhs.w);
	}

	template <typename T>
	v4<T> operator/(T lhs, v4<T> rhs) {
		return v4<T>(lhs / rhs.x, lhs / rhs.y, lhs / rhs.z, lhs - rhs.w);
	}

	template <typename T>
	v4<T> operator-(const v4<T>& v) {
		return v4<T>(-v.x, -v.y, -v.z, -v.w);
	}

	typedef v2<i32> v2i;
	typedef v2<f32> v2f;
	typedef v2<f64> v2d;
	typedef v3<i32> v3i;
	typedef v3<f32> v3f;
	typedef v3<f64> v3d;
	typedef v4<i32> v4i;
	typedef v4<f32> v4f;
	typedef v4<f64> v4d;

	template struct VKR_API v2<i32>;
	template struct VKR_API v2<f32>;
	template struct VKR_API v2<f64>;
	template struct VKR_API v3<i32>;
	template struct VKR_API v3<f32>;
	template struct VKR_API v3<f64>;
	template struct VKR_API v4<i32>;
	template struct VKR_API v4<f32>;
	template struct VKR_API v4<f64>;

	struct VKR_API m4f {
		f32 m[4][4];

		m4f();
		m4f(f32 d);

		static m4f identity();
		static m4f screenspace(f32 hw, f32 hh);
		
		m4f operator*(const m4f& other) const;
		v4f operator*(const v4f& other) const;

		static m4f translate(m4f m, v3f v);
		static m4f rotate(m4f m, f32 a, v3f v);
		static m4f scale(m4f m, v3f v);

		static m4f lookat(v3f c, v3f o, v3f u);
		static m4f pers(f32 fov, f32 asp, f32 n, f32 f);
		static m4f orth(f32 l, f32 r, f32 b, f32 t, f32 n, f32 f);

		m4f inverse();
		m4f transposed();
	};
}
