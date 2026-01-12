#pragma once
#include "xx_includes.h"

namespace xx {

    /************************************************************************************/
    // std::is_pod like, flag container can memcpy or memmove

    template<typename T, typename ENABLED = void>
    struct IsPod : std::false_type {};
    template<typename T>
    struct IsPod<T, std::enable_if_t<std::is_standard_layout_v<T>&& std::is_trivial_v<T>>> : std::true_type {};
    template<typename T> constexpr bool IsPod_v = IsPod<std::remove_cvref_t<T>>::value;



    /************************************************************************************/
    // check T is literal "xxxxx" strings

    template<typename T, typename = void> struct IsLiteral : std::false_type {};
    template<size_t L> struct IsLiteral<char[L], void> : std::true_type {
        static const size_t len = L;
        static const size_t wide = sizeof(char);
    };
    template<size_t L> struct IsLiteral<char const [L], void> : std::true_type {
        static const size_t len = L;
        static const size_t wide = sizeof(char);
    };
    template<size_t L> struct IsLiteral<char const (&)[L], void> : std::true_type {
        static const size_t len = L;
        static const size_t wide = sizeof(char);
    };
    template<size_t L> struct IsLiteral<char16_t[L], void> : std::true_type {
        static const size_t len = L;
        static const size_t wide = sizeof(char16_t);
    };
    template<size_t L> struct IsLiteral<char16_t const [L], void> : std::true_type {
        static const size_t len = L;
        static const size_t wide = sizeof(char16_t);
    };
    template<size_t L> struct IsLiteral<char16_t const (&)[L], void> : std::true_type {
        static const size_t len = L;
        static const size_t wide = sizeof(char16_t);
    };
    template<size_t L> struct IsLiteral<char32_t[L], void> : std::true_type {
        static const size_t len = L;
        static const size_t wide = sizeof(char32_t);
    };
    template<size_t L> struct IsLiteral<char32_t const [L], void> : std::true_type {
        static const size_t len = L;
        static const size_t wide = sizeof(char32_t);
    };
    template<size_t L> struct IsLiteral<char32_t const (&)[L], void> : std::true_type {
        static const size_t len = L;
        static const size_t wide = sizeof(char32_t);
    };
    template<size_t L> struct IsLiteral<wchar_t[L], void> : std::true_type {
        static const size_t len = L;
        static const size_t wide = sizeof(wchar_t);
    };
    template<size_t L> struct IsLiteral<wchar_t const [L], void> : std::true_type {
        static const size_t len = L;
        static const size_t wide = sizeof(wchar_t);
    };
    template<size_t L> struct IsLiteral<wchar_t const (&)[L], void> : std::true_type {
        static const size_t len = L;
        static const size_t wide = sizeof(wchar_t);
    };
    template<typename T> constexpr bool IsLiteral_v = IsLiteral<T>::value;
    template<typename T> constexpr size_t LiteralLen = IsLiteral<T>::len;
    template<typename T> constexpr size_t LiteralWide = IsLiteral<T>::wide;



    template<typename T>
    struct FromTo {
        T from, to;
        constexpr bool operator==(FromTo const& o) const = default;
        constexpr void Limit(T& v) const {
            if (v < from) v = from;
            else if (v > to) v = to;
        }
        constexpr T Sub() const {
            return to - from;
        }
    };


    /************************************************************************************/
    // scope guards

    template<class F>   // F == lambda
    auto MakeScopeGuard(F&& f) noexcept {
        struct ScopeGuard {
            F f;
            bool canceled;
            explicit ScopeGuard(F&& f) noexcept : f(std::forward<F>(f)), canceled(false) {}
            ~ScopeGuard() noexcept { if (!canceled) { f(); } }
            inline void Cancel() noexcept { canceled = true; }
            inline void operator()(bool canceled_ = false) {
                f();
                canceled = canceled_;
            }
        };
        return ScopeGuard(std::forward<F>(f));
    }

    template<class F>
    auto MakeSimpleScopeGuard(F&& f) noexcept {
        struct SG { F f; SG(F&& f) noexcept : f(std::forward<F>(f)) {} ~SG() { f(); } };
        return SG(std::forward<F>(f));
    }

    // return first bit '1' index
    template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    size_t Calc2n(T n) {
        static_assert(sizeof(T) >= 4);
        return (sizeof(size_t) * 8 - 1) - std::countl_zero(n);
    }

    // return 2^x ( >= n )
    template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    T Round2n(T n) {
        static_assert(sizeof(T) >= 4);
        auto shift = Calc2n(n);
        auto rtv = T(1) << shift;
        if (rtv == n) return n;
        else return rtv << 1;
    }



    /***********************************************************************************/
    // replace std::aligned_storage & std::aligned_union

    template <typename T>
    class MyAlignedStorage {
    private:
        alignas(T) char dummyBuf[sizeof(T)];
    };

    template <typename... Ts>
    class MyAlignedUnion {
    private:
        alignas(MaxAlignof_v<Ts...>) char dummyBuf[MaxSizeof_v<Ts...>];
    };


    /***********************************************************************************/
    // string utils

    inline constexpr std::string_view TrimRight(std::string_view const& s) {
        auto idx = s.find_last_not_of(" \t\n\r\f\v");
        if (idx == std::string_view::npos) return { s.data(), 0 };
        return { s.data(), idx + 1 };
    }

    inline constexpr std::string_view TrimLeft(std::string_view const& s) {
        auto idx = s.find_first_not_of(" \t\n\r\f\v");
        if (idx == std::string_view::npos) return { s.data(), 0 };
        return { s.data() + idx, s.size() - idx };
    }

    inline constexpr std::string_view Trim(std::string_view const& s) {
        return TrimLeft(TrimRight(s));
    }

    template<size_t numDelimiters>
    inline constexpr std::string_view SplitOnce(std::string_view& sv, char const(&delimiters)[numDelimiters]) {
        static_assert(numDelimiters >= 2);
        auto data = sv.data();
        auto siz = sv.size();
        for (size_t i = 0; i != siz; ++i) {
            bool found;
            if constexpr (numDelimiters == 2) {
                found = sv[i] == delimiters[0];
            }
            else {
                found = std::string_view(delimiters).find(sv[i]) != std::string_view::npos;
            }
            if (found) {
                sv = std::string_view(data + i + 1, siz - i - 1);
                return { data, i };
            }
        }
        sv = std::string_view(data + siz, 0);
        return { data, siz };
    }

    template<typename T>
    std::string const& U8AsString(T const& u8s) {
        return (std::string const&)u8s;
    }

    template<typename T>
    std::string U8AsString(T&& u8s) {
        return (std::string&&)u8s;
    }


    template<typename S>
    inline size_t StrLen(S const& s) {
        if constexpr (std::is_pointer_v<S>) {
            if (!s) return 0;
            using C = std::remove_cvref_t<std::remove_pointer_t<S>>;
            return std::basic_string_view<C>(s).size();
        }
        else  if constexpr (IsLiteral_v<S>) {
            return LiteralLen<S> -1;
        }
        else {
            return s.size();
        }
    }

    template<typename S>
    inline auto StrPtr(S const& s) {
        return &s[0];
    }

}
