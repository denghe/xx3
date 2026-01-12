#pragma once
#include "xx_utils.h"

namespace xx {
	
	template<typename T>
    concept HasField_XY = requires { T::x; T::y; };

    template<typename T>
    concept IsArithmetic = std::is_arithmetic_v<T>;

    template<typename T>
    struct X_Y {
        using ElementType = T;
        T x, y;

        constexpr X_Y() = default;
        constexpr X_Y(X_Y const&) = default;
        constexpr X_Y& operator=(X_Y const&) = default;

        constexpr X_Y(HasField_XY auto const& v) : x(T(v.x)), y(T(v.y)) {}
        constexpr X_Y(IsArithmetic auto x_, IsArithmetic auto y_) : x(T(x_)), y(T(y_)) {}
        constexpr X_Y(IsArithmetic auto v) : x(T(v)), y(T(v)) {}

        constexpr X_Y& operator=(HasField_XY auto const& v) { x = T(v.x); y = T(v.y); return *this; }
        constexpr X_Y& operator=(IsArithmetic auto v) { x = T(v); y = T(v); return *this; }

        constexpr X_Y operator-() const { return { -x, -y }; }

        constexpr X_Y operator+(HasField_XY auto const& v) const { return { T(x + v.x), T(y + v.y) }; }
        constexpr X_Y operator-(HasField_XY auto const& v) const { return { T(x - v.x), T(y - v.y) }; }
        constexpr X_Y operator*(HasField_XY auto const& v) const { return { T(x * v.x), T(y * v.y) }; }
        constexpr X_Y operator/(HasField_XY auto const& v) const { return { T(x / v.x), T(y / v.y) }; }

        constexpr X_Y operator+(IsArithmetic auto v) const { return { T(x + v), T(y + v) }; }
        constexpr X_Y operator-(IsArithmetic auto v) const { return { T(x - v), T(y - v) }; }
        constexpr X_Y operator*(IsArithmetic auto v) const { return { T(x * v), T(y * v) }; }
        constexpr X_Y operator/(IsArithmetic auto v) const { return { T(x / v), T(y / v) }; }

        constexpr X_Y& operator+=(HasField_XY auto const& v) { x = T(x + v.x); y = T(y + v.y); return *this; }
        constexpr X_Y& operator-=(HasField_XY auto const& v) { x = T(x - v.x); y = T(y - v.y); return *this; }
        constexpr X_Y& operator*=(HasField_XY auto const& v) { x = T(x * v.x); y = T(y * v.y); return *this; }
        constexpr X_Y& operator/=(HasField_XY auto const& v) { x = T(x / v.x); y = T(y / v.y); return *this; }

        constexpr X_Y& operator+=(IsArithmetic auto v) { x = T(x + v); y = T(y + v); return *this; }
        constexpr X_Y& operator-=(IsArithmetic auto v) { x = T(x - v); y = T(y - v); return *this; }
        constexpr X_Y& operator*=(IsArithmetic auto v) { x = T(x * v); y = T(y * v); return *this; }
        constexpr X_Y& operator/=(IsArithmetic auto v) { x = T(x / v); y = T(y / v); return *this; }

        constexpr bool operator==(HasField_XY auto const& v) const { return ::memcmp(this, &v, sizeof(v)) == 0; }
        constexpr bool operator!=(HasField_XY auto const& v) const { return ::memcmp(this, &v, sizeof(v)) != 0; }

        constexpr X_Y Add(IsArithmetic auto v) const { return { T(x + v), T(y + v) }; }
        constexpr X_Y Add(IsArithmetic auto vx, IsArithmetic auto vy) const { return { T(x + vx), T(y + vy) }; }
        constexpr X_Y AddX(IsArithmetic auto v) const { return { T(x + v), y }; }
        constexpr X_Y AddY(IsArithmetic auto v) const { return { x, T(y + v) }; }
        // ...
        constexpr X_Y FlipY() const { return { x, -y }; }
        constexpr X_Y FlipX() const { return { -x, y }; }
        constexpr X_Y OneMinusX() const { return { T{1} - x, y }; }
        constexpr X_Y OneMinusY() const { return { x, T{1} - y }; }
        // ...

        constexpr void Reset() {
			x = {};
			y = {};
        }

        template<typename U>
        constexpr auto As() const -> X_Y<U> {
			return { (U)x, (U)y };
        }

        constexpr bool IsZero() const {
            if constexpr (std::is_floating_point_v<T>) {
                return (std::abs(x) < std::numeric_limits<T>::epsilon())
                    && (std::abs(y) < std::numeric_limits<T>::epsilon());
            }
            else return x == T{} && y == T{};
        }

        constexpr bool IsZeroSimple() const {
            return x == T{} && y == T{};
        }

        constexpr bool IsOutOfEdge(HasField_XY auto const& edge) const {
            return x < 0 || y < 0 || x >= edge.x || y >= edge.y;
        }

        template<typename R = T>
        constexpr auto Floor() const -> X_Y<R> requires std::is_same_v<float, T> {
            return { std::floorf(x), std::floorf(y) };
        }

    };


    template<typename T> inline X_Y<T> operator+(IsArithmetic auto const& v, X_Y<T> const& o) { return X_Y<T>{ v + o.x, v + o.y }; }
    template<typename T> inline X_Y<T> operator-(IsArithmetic auto const& v, X_Y<T> const& o) { return X_Y<T>{ v - o.x, v - o.y }; }
    template<typename T> inline X_Y<T> operator*(IsArithmetic auto const& v, X_Y<T> const& o) { return X_Y<T>{ v * o.x, v * o.y }; }
    template<typename T> inline X_Y<T> operator/(IsArithmetic auto const& v, X_Y<T> const& o) { return X_Y<T>{ v / o.x, v / o.y }; }

    using XYi = X_Y<int32_t>;
    using XYu = X_Y<uint32_t>;
    using XYf = X_Y<float>;
    using XYd = X_Y<double>;
    using XY = XYf;


    /***********************************************************************************/

    // pos + size for texture uv mapping
    union UVRect {
        struct {
            uint16_t x, y, w, h;
        };
        uint64_t data;
    };


    /***********************************************************************************/

    // color with alpha
    struct RGBA8 {
        uint8_t r, g, b, a;
        bool operator==(RGBA8 const&) const = default;
        bool operator!=(RGBA8 const&) const = default;
    };
    constexpr static RGBA8 RGBA8_Zero{ 0,0,0,0 };
    constexpr static RGBA8 RGBA8_Red{ 255,0,0,255 };
    constexpr static RGBA8 RGBA8_Green{ 0,255,0,255 };
    constexpr static RGBA8 RGBA8_Blue{ 0,0,255,255 };
    constexpr static RGBA8 RGBA8_White{ 255,255,255,255 };
    constexpr static RGBA8 RGBA8_Gray{ 127,127,127,255 };
    constexpr static RGBA8 RGBA8_Black{ 0,0,0,255 };
    constexpr static RGBA8 RGBA8_Yellow{ 255,255,0,255 };

}
