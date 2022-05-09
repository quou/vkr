#include "vkr.hpp"

namespace vkr {
	m4f::m4f() {}
	
	m4f::m4f(f32 d) {
		for (u32 y = 0; y < 4; y++) {
			for (u32 x = 0; x < 4; x++) {
				m[x][y] = 0.0f;
			}
		}

		m[0][0] = d;
		m[1][1] = d;
		m[2][2] = d;
		m[3][3] = d;
	}

	m4f m4f::identity() {
		return m4f(1.0f);
	}

	m4f m4f::screenspace(f32 hw, f32 hh) {
		m4f r(1.0f);

		r.m[0][0] =  hw;
		r.m[0][3] =  hw;
		r.m[1][1] = -hh;
		r.m[1][3] =  hh;

		return r;
	}

	m4f m4f::operator*(const m4f& other) const {
		m4f r(1.0f);

		r.m[0][0] = m[0][0] * other.m[0][0] + m[1][0] * other.m[0][1] + m[2][0] * other.m[0][2] + m[3][0] * other.m[0][3];
		r.m[1][0] = m[0][0] * other.m[1][0] + m[1][0] * other.m[1][1] + m[2][0] * other.m[1][2] + m[3][0] * other.m[1][3];
		r.m[2][0] = m[0][0] * other.m[2][0] + m[1][0] * other.m[2][1] + m[2][0] * other.m[2][2] + m[3][0] * other.m[2][3];
		r.m[3][0] = m[0][0] * other.m[3][0] + m[1][0] * other.m[3][1] + m[2][0] * other.m[3][2] + m[3][0] * other.m[3][3];
		r.m[0][1] = m[0][1] * other.m[0][0] + m[1][1] * other.m[0][1] + m[2][1] * other.m[0][2] + m[3][1] * other.m[0][3];
		r.m[1][1] = m[0][1] * other.m[1][0] + m[1][1] * other.m[1][1] + m[2][1] * other.m[1][2] + m[3][1] * other.m[1][3];
		r.m[2][1] = m[0][1] * other.m[2][0] + m[1][1] * other.m[2][1] + m[2][1] * other.m[2][2] + m[3][1] * other.m[2][3];
		r.m[3][1] = m[0][1] * other.m[3][0] + m[1][1] * other.m[3][1] + m[2][1] * other.m[3][2] + m[3][1] * other.m[3][3];
		r.m[0][2] = m[0][2] * other.m[0][0] + m[1][2] * other.m[0][1] + m[2][2] * other.m[0][2] + m[3][2] * other.m[0][3];
		r.m[1][2] = m[0][2] * other.m[1][0] + m[1][2] * other.m[1][1] + m[2][2] * other.m[1][2] + m[3][2] * other.m[1][3];
		r.m[2][2] = m[0][2] * other.m[2][0] + m[1][2] * other.m[2][1] + m[2][2] * other.m[2][2] + m[3][2] * other.m[2][3];
		r.m[3][2] = m[0][2] * other.m[3][0] + m[1][2] * other.m[3][1] + m[2][2] * other.m[3][2] + m[3][2] * other.m[3][3];
		r.m[0][3] = m[0][3] * other.m[0][0] + m[1][3] * other.m[0][1] + m[2][3] * other.m[0][2] + m[3][3] * other.m[0][3];
		r.m[1][3] = m[0][3] * other.m[1][0] + m[1][3] * other.m[1][1] + m[2][3] * other.m[1][2] + m[3][3] * other.m[1][3];
		r.m[2][3] = m[0][3] * other.m[2][0] + m[1][3] * other.m[2][1] + m[2][3] * other.m[2][2] + m[3][3] * other.m[2][3];
		r.m[3][3] = m[0][3] * other.m[3][0] + m[1][3] * other.m[3][1] + m[2][3] * other.m[3][2] + m[3][3] * other.m[3][3];

		return r;
	}

	v4f m4f::operator*(const v4f& other) const {
		return v4f(
			m[0][0] * other.x + m[1][0] * other.y + m[2][0] * other.z + m[3][0] + other.w,
			m[0][1] * other.x + m[1][1] * other.y + m[2][1] * other.z + m[3][1] + other.w,
			m[0][2] * other.x + m[1][2] * other.y + m[2][2] * other.z + m[3][2] + other.w,
			m[0][3] * other.x + m[1][3] * other.y + m[2][3] * other.z + m[3][3] + other.w);
	}

	m4f m4f::translate(m4f m, v3f v) {
		m4f r(1.0f);

		r.m[3][0] += v.x;
		r.m[3][1] += v.y;
		r.m[3][2] += v.z;

		return m * r;
	}

	m4f m4f::rotate(m4f m, f32 a, v3f v) {
		m4f r(1.0f);

		const f32 c = cosf(a);
		const f32 s = sinf(a);

		const f32 omc = (f32)1 - c;

		const f32 x = v.x;
		const f32 y = v.y;
		const f32 z = v.z;

		r.m[0][0] = x * x * omc + c;
		r.m[0][1] = y * x * omc + z * s;
		r.m[0][2] = x * z * omc - y * s;
		r.m[1][0] = x * y * omc - z * s;
		r.m[1][1] = y * y * omc + c;
		r.m[1][2] = y * z * omc + x * s;
		r.m[2][0] = x * z * omc + y * s;
		r.m[2][1] = y * z * omc - x * s;
		r.m[2][2] = z * z * omc + c;

		return m * r;
	}

	m4f m4f::scale(m4f m, v3f v) {
		m4f r(1.0f);

		r.m[0][0] = v.x;
		r.m[1][1] = v.y;
		r.m[2][2] = v.z;

		return m * r;
	}

	m4f m4f::lookat(v3f c, v3f o, v3f u) {
		m4f r(1.0f);

		const v3f f = v3f::normalised(o - c);
		u = v3f::normalised(u);
		const v3f s = v3f::normalised(v3f::cross(f, u));
		u = v3f::cross(s, f);

		r.m[0][0] = s.x;
		r.m[1][0] = s.y;
		r.m[2][0] = s.z;
		r.m[0][1] = u.x;
		r.m[1][1] = u.y;
		r.m[2][1] = u.z;
		r.m[0][2] = -f.x;
		r.m[1][2] = -f.y;
		r.m[2][2] = -f.z;
		r.m[3][0] = -v3f::dot(s, c);
		r.m[3][1] = -v3f::dot(u, c);
		r.m[3][2] = v3f::dot(f, c);

		return r;
	}

	m4f m4f::pers(f32 fov, f32 asp, f32 n, f32 f) {
		m4f r(0.0f);

		f32 thf = tanf(to_rad(fov) / 2.0f);

		r.m[0][0] = (1.0f / (asp * thf));
		r.m[1][1] = (1.0f / thf);
		r.m[2][2] = -((f + n) / (f - n));
		r.m[2][3] = -1.0f;
		r.m[3][2] = -((2.0f * f * n) / (f - n));

		return r;
	}

	static m4f orth(f32 l, f32 r, f32 b, f32 t, f32 n, f32 f) {
		m4f res(1.0f);

		res.m[0][0] = 2.0f / (r - l);
		res.m[1][1] = 2.0f / (t - b);
		res.m[2][2] = 2.0f / (n - f);

		res.m[3][0] = (l + r) / (l - r);
		res.m[3][1] = (b + t) / (b - t);
		res.m[3][2] = (f + n) / (f - n);

		return res;
	}

	m4f m4f::inverse() {
		const f32* mm = (f32*)m;

		f32 t0 = mm[10] * mm[15];
		f32 t1 = mm[14] * mm[11];
		f32 t2 = mm[6] * mm[15];
		f32 t3 = mm[14] * mm[7];
		f32 t4 = mm[6] * mm[11];
		f32 t5 = mm[10] * mm[7];
		f32 t6 = mm[2] * mm[15];
		f32 t7 = mm[14] * mm[3];
		f32 t8 = mm[2] * mm[11];
		f32 t9 = mm[10] * mm[3];
		f32 t10 = mm[2] * mm[7];
		f32 t11 = mm[6] * mm[3];
		f32 t12 = mm[8] * mm[13];
		f32 t13 = mm[12] * mm[9];
		f32 t14 = mm[4] * mm[13];
		f32 t15 = mm[12] * mm[5];
		f32 t16 = mm[4] * mm[9];
		f32 t17 = mm[8] * mm[5];
		f32 t18 = mm[0] * mm[13];
		f32 t19 = mm[12] * mm[1];
		f32 t20 = mm[0] * mm[9];
		f32 t21 = mm[8] * mm[1];
		f32 t22 = mm[0] * mm[5];
		f32 t23 = mm[4] * mm[1];

		m4f r(1.0f);
		f32* o = (f32*)r.m;

		o[0] = (t0 * mm[5] + t3 * mm[9] + t4 * mm[13]) - (t1 * mm[5] + t2 * mm[9] + t5 * mm[13]);
		o[1] = (t1 * mm[1] + t6 * mm[9] + t9 * mm[13]) - (t0 * mm[1] + t7 * mm[9] + t8 * mm[13]);
		o[2] = (t2 * mm[1] + t7 * mm[5] + t10 * mm[13]) - (t3 * mm[1] + t6 * mm[5] + t11 * mm[13]);
		o[3] = (t5 * mm[1] + t8 * mm[5] + t11 * mm[9]) - (t4 * mm[1] + t9 * mm[5] + t10 * mm[9]);

		f32 d = 1.0f / (mm[0] * o[0] + mm[4] * o[1] + mm[8] * o[2] + mm[12] * o[3]);

		o[0] = d * o[0];
		o[1] = d * o[1];
		o[2] = d * o[2];
		o[3] = d * o[3];
		o[4] = d * ((t1 * mm[4] + t2 * mm[8] + t5 * mm[12]) - (t0 * mm[4] + t3 * mm[8] + t4 * mm[12]));
		o[5] = d * ((t0 * mm[0] + t7 * mm[8] + t8 * mm[12]) - (t1 * mm[0] + t6 * mm[8] + t9 * mm[12]));
		o[6] = d * ((t3 * mm[0] + t6 * mm[4] + t11 * mm[12]) - (t2 * mm[0] + t7 * mm[4] + t10 * mm[12]));
		o[7] = d * ((t4 * mm[0] + t9 * mm[4] + t10 * mm[8]) - (t5 * mm[0] + t8 * mm[4] + t11 * mm[8]));
		o[8] = d * ((t12 * mm[7] + t15 * mm[11] + t16 * mm[15]) - (t13 * mm[7] + t14 * mm[11] + t17 * mm[15]));
		o[9] = d * ((t13 * mm[3] + t18 * mm[11] + t21 * mm[15]) - (t12 * mm[3] + t19 * mm[11] + t20 * mm[15]));
		o[10] = d * ((t14 * mm[3] + t19 * mm[7] + t22 * mm[15]) - (t15 * mm[3] + t18 * mm[7] + t23 * mm[15]));
		o[11] = d * ((t17 * mm[3] + t20 * mm[7] + t23 * mm[11]) - (t16 * mm[3] + t21 * mm[7] + t22 * mm[11]));
		o[12] = d * ((t14 * mm[10] + t17 * mm[14] + t13 * mm[6]) - (t16 * mm[14] + t12 * mm[6] + t15 * mm[10]));
		o[13] = d * ((t20 * mm[14] + t12 * mm[2] + t19 * mm[10]) - (t18 * mm[10] + t21 * mm[14] + t13 * mm[2]));
		o[14] = d * ((t18 * mm[6] + t23 * mm[14] + t15 * mm[2]) - (t22 * mm[14] + t14 * mm[2] + t19 * mm[6]));
		o[15] = d * ((t22 * mm[10] + t16 * mm[2] + t21 * mm[6]) - (t20 * mm[6] + t23 * mm[10] + t17 * mm[2]));

		return r;
	}

	m4f m4f::transposed() {
		m4f r(1.0f);

		r.m[0][0] = m[0][0];
		r.m[1][0] = m[0][1];
		r.m[2][0] = m[0][2];
		r.m[3][0] = m[0][3];
		r.m[0][1] = m[1][0];
		r.m[1][1] = m[1][1];
		r.m[2][1] = m[1][2];
		r.m[3][1] = m[1][3];
		r.m[0][2] = m[2][0];
		r.m[1][2] = m[2][1];
		r.m[2][2] = m[2][2];
		r.m[3][2] = m[2][3];
		r.m[0][3] = m[3][0];
		r.m[1][3] = m[3][1];
		r.m[2][3] = m[3][2];
		r.m[3][3] = m[3][3];

		return r;
	}
}
