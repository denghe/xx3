#pragma once
#include "xx_includes.h"

namespace xx {

    /************************************************************************************/
    // time_point <--> .net DateTime.Now.ToUniversalTime().Ticks converts

    typedef std::chrono::duration<long long, std::ratio<1LL, 10000000LL>> duration_10m;

    inline int64_t TimePointToEpoch10m(std::chrono::system_clock::time_point const& val) {
        return std::chrono::duration_cast<duration_10m>(val.time_since_epoch()).count();
    }

    inline int64_t TimePointToEpoch10m(std::chrono::steady_clock::time_point const& val) {
        return std::chrono::duration_cast<duration_10m>(val.time_since_epoch()).count();
    }

    inline std::chrono::system_clock::time_point Epoch10mToTimePoint(int64_t const& val) {
        return std::chrono::system_clock::time_point(std::chrono::duration_cast<std::chrono::system_clock::duration>(duration_10m(val)));
    }

    inline std::chrono::system_clock::time_point Now() {
        return std::chrono::system_clock::now();
    }

    inline std::chrono::system_clock::time_point NowTimePoint() {
        return std::chrono::system_clock::now();
    }

    inline std::chrono::steady_clock::time_point NowSteadyTimePoint() {
        return std::chrono::steady_clock::now();
    }

    inline int64_t NowEpoch10m() {
        return TimePointToEpoch10m(NowTimePoint());
    }

    inline int64_t NowEpochMicroseconds() {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    inline int64_t NowEpochMilliseconds() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    inline double NowEpochSeconds() {
        return (double)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count() / 1000000.0;
    }

    inline double NowEpochSeconds(double& last) {
        auto now = NowEpochSeconds();
        auto rtv = now - last;
        last = now;
        return rtv;
    }

    inline int64_t Epoch10mToUtcDateTimeTicks(int64_t const& val) {
        return val + 621355968000000000LL;
    }

    inline int64_t UtcDateTimeTicksToEpoch10m(int64_t const& val) {
        return val - 621355968000000000LL;
    }

    inline int32_t TimePointToEpoch(std::chrono::system_clock::time_point const& val) {
        return (int32_t)(val.time_since_epoch().count() / 10000000);
    }

    inline std::chrono::system_clock::time_point EpochToTimePoint(int32_t const& val) {
        return std::chrono::system_clock::time_point(std::chrono::system_clock::time_point::duration((int64_t)val * 10000000));
    }

    inline int64_t NowSteadyEpoch10m() {
        return TimePointToEpoch10m(NowSteadyTimePoint());
    }

    inline int64_t NowSteadyEpochMicroseconds() {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    inline int64_t NowSteadyEpochMilliseconds() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    inline double NowSteadyEpochSeconds() {
        return (double)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() / 1000000.0;
    }

    inline double NowSteadyEpochSeconds(double& last) {
        auto now = NowSteadyEpochSeconds();
        auto rtv = now - last;
        last = now;
        return rtv;
    }

    template<typename T>
    inline void AppendTimePoint_Local(std::string& s, T const& tp) {
        auto&& t = std::chrono::system_clock::to_time_t(tp);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        auto bak = s.size();
        s.resize(s.size() + 30);
        auto len = std::strftime(&s[bak], 30, "%Y-%m-%d %H:%M:%S", &tm);
        s.resize(bak + len);
    }

    template<typename T>
    inline std::string TimePointToString_Local(T const& tp) {
        std::string s;
        AppendTimePoint_Local(s, tp);
        return s;
    }

}
