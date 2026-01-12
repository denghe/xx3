#pragma once

#include <chrono>
#include <array>
#include <type_traits>
#include <memory>
#include <utility>
#include <filesystem>
#include <functional>
//#include <bit>
//#include <concepts>
//#include <initializer_list>
//#include <optional>
//#include <variant>
//#include <tuple>
//#include <vector>
//#include <queue>
//#include <deque>
//#include <string>
//#include <sstream>
//#include <string_view>
//#include <charconv>
//#include <unordered_set>
//#include <unordered_map>
//#include <set>
//#include <map>
//#include <list>
//#include <stdexcept>
//#include <algorithm>
//#include <iostream>
//#include <thread>
//#include <mutex>
//#include <condition_variable>
//#include <coroutine>
//#include <span>
//#include <format>


#define _USE_MATH_DEFINES  // needed for M_PI and M_PI2
#include <math.h>          // M_PI
#undef _USE_MATH_DEFINES
#include <cstring>
#include <cstdint>
#include <cassert>
#include <cstdlib>
//#include <cstddef>
//#include <ctime>
//#include <cstdio>
//#include <cerrno>

using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace std::chrono_literals;


#ifdef _WIN32
#ifndef NOMINMAX
#	define NOMINMAX
#endif
//#	define NODRAWTEXT
//#	define NOGDI            // d3d9 need it
#	define NOBITMAP
#	define NOMCX
#	define NOSERVICE
#	define NOHELP
#	define WIN32_LEAN_AND_MEAN
#   include <WinSock2.h>
#   include <process.h>
#	include <Windows.h>
#   include <intrin.h>     // _BitScanReverseXXXX _byteswap_XXXX
#   include <ShlObj.h>
#   include <mmsystem.h>	// timeBeginPeriod
#else
#	include <unistd.h>    // for usleep
#   include <arpa/inet.h>  // __BYTE_ORDER __LITTLE_ENDIAN __BIG_ENDIAN
#endif


#ifndef XX_NOINLINE
#   ifndef NDEBUG
#       define XX_NOINLINE
#       define XX_INLINE inline
#   else
#       ifdef _MSC_VER
#           define XX_NOINLINE __declspec(noinline)
#           define XX_INLINE __forceinline
#       else
#           define XX_NOINLINE __attribute__((noinline))
#           define XX_INLINE __attribute__((always_inline)) inline
#       endif
#   endif
#endif


#ifndef XX_STRINGIFY
#	define XX_STRINGIFY(x)  XX_STRINGIFY_(x)
#	define XX_STRINGIFY_(x)  #x
#endif


#ifndef _countof
template<typename T, size_t N>
constexpr size_t _countof(T const (&arr)[N]) {
    return N;
}
#endif


#ifndef _offsetof
#define _offsetof(s,m) ((size_t)&reinterpret_cast<char const volatile&>((((s*)0)->m)))
#endif


#ifndef container_of
#define container_of(ptr, type, member) \
  ((type *) ((char *) (ptr) - _offsetof(type, member)))
#endif


#ifndef _WIN32
inline void Sleep(int const& ms) {
    usleep(ms * 1000);
}
#endif


#ifdef __GNUC__
//#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif


/************************************************************************************/
// stackless simulate
/* example:

    int lineNumber{};   // struct member
    void Update() {
        XX_BEGIN(lineNumber)
            XX_YIELD(lineNumber)
        XX_END(lineNumber)
    }
	bool Update() {
		XX_BEGIN(lineNumber)
			XX_YIELD_F(lineNumber)
		XX_END(lineNumber)
	}

*/
#define XX_BEGIN(lineNumber)             switch (lineNumber) { case 0:
#define XX_YIELD(lineNumber)             lineNumber = __LINE__; return; case __LINE__:;
#define XX_YIELD_F(lineNumber)           lineNumber = __LINE__; return false; case __LINE__:;
#define XX_YIELD_B(lineNumber)           lineNumber = __LINE__; return false; case __LINE__:;
#define XX_YIELD_I(lineNumber)           lineNumber = __LINE__; return 0; case __LINE__:;
#define XX_YIELD_Z(lineNumber)           lineNumber = __LINE__; return 0; case __LINE__:;
#define XX_YIELD_TO_BEGIN(lineNumber)    lineNumber = 0; return;
#define XX_YIELD_F_TO_BEGIN(lineNumber)  lineNumber = 0; return false;
#define XX_YIELD_B_TO_BEGIN(lineNumber)  lineNumber = 0; return false;
#define XX_YIELD_I_TO_BEGIN(lineNumber)  lineNumber = 0; return 0;
#define XX_YIELD_Z_TO_BEGIN(lineNumber)  lineNumber = 0; return 0;
#define XX_END(lineNumber)               }





#include <gl.h>
#include <GLFW/glfw3.h>
#define GLFW_MOUSE_BUTTON_WHEEL_UP (GLFW_MOUSE_BUTTON_LAST + 1)
#define GLFW_MOUSE_BUTTON_WHEEL_DOWN (GLFW_MOUSE_BUTTON_LAST + 2)
#define GLFW_MOUSE_BUTTON_WHEEL_LEFT (GLFW_MOUSE_BUTTON_LAST + 3)
#define GLFW_MOUSE_BUTTON_WHEEL_RIGHT (GLFW_MOUSE_BUTTON_LAST + 4)
#include <GLFW/glfw3native.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_rect_pack.h>
#ifdef WIN32
#pragma comment (lib ,"imm32.lib")
#endif
