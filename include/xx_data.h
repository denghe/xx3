#pragma once
#include "xx_utils.h"

namespace xx {

    // binary data referrer( buf + len ) / reader( buf + len + offset ) / container( buf + len + cap + offset ) C++20 std::span likely

    template<typename T> concept Has_GetBuf = requires(T t) { t.GetBuf(); };
    template<typename T> concept Has_GetLen = requires(T t) { t.GetLen(); };

    // referrer( buf + len )
    struct Span {
        uint8_t* buf;
        size_t len;

        Span()
            : buf(nullptr), len(0) {
        }
        Span(Span const& o) = default;
        Span& operator=(Span const& o) = default;

        Span(void const* buf, size_t len)
            : buf((uint8_t*)buf), len(len) {
        }

        template<typename T, typename = std::enable_if_t<std::is_class_v<T>>>
        explicit Span(T const& d)
            : buf((uint8_t*)d.buf), len(d.len) {
        }

        void Reset(void const* buf_, size_t len_) {
            buf = (uint8_t*)buf_;
            len = len_;
        }

        template<typename T, typename = std::enable_if_t<std::is_class_v<T>>>
        void Reset(T const& d, size_t offset_ = 0) {
            Reset(d.buf, d.len, offset_);
        }

        template<typename T, typename = std::enable_if_t<std::is_class_v<T>>>
        Span& operator=(T const& o) {
            Reset(o.buf, o.len);
            return *this;
        }

        bool operator==(Span const& o) const {
            if (&o == this) return true;
            if (len != o.len) return false;
            return 0 == memcmp(buf, o.buf, len);
        }

        bool operator!=(Span const& o) const {
            return !this->operator==(o);
        }

        uint8_t& operator[](size_t idx) const {
            assert(idx < len);
            return (uint8_t&)buf[idx];
        }

        operator bool() const {
            return len != 0;
        }

        // for easy use
        operator std::string_view() const {
            return { (char*)buf, len };
        }
    };

    // mem moveable tag
    template<>
    struct IsPod<Span, void> : std::true_type {};

    // SerDe base template
    template<typename T, typename ENABLED>
    struct DataFuncs;

    // reader( buf + len + offset )
    struct Data_r : Span {
        size_t offset;

        Data_r()
            : offset(0) {
        }
        Data_r(Data_r const& o) = default;
        Data_r& operator=(Data_r const& o) = default;

        Data_r(void const* buf, size_t len, size_t offset = 0) {
            Reset(buf, len, offset);
        }

        template<typename T, typename = std::enable_if_t<std::is_class_v<T>>>
        Data_r(T const& d, size_t offset = 0) {
            if constexpr (std::is_same_v<std::string_view, std::decay_t<T>>) {
                Reset(d.data(), d.size(), offset);
            }
            else if constexpr (Has_GetBuf<T> && Has_GetLen<T>) {
                Reset(d.GetBuf(), d.GetLen(), offset);
            }
            else {
                Reset(d.buf, d.len, offset);
            }
        }

        void Reset(void const* buf_, size_t len_, size_t offset_ = 0) {
            this->Span::Reset(buf_, len_);
            offset = offset_;
        }

        template<typename T, typename = std::enable_if_t<std::is_class_v<T>>>
        void Reset(T const& d, size_t offset_ = 0) {
            Reset(d.buf, d.len, offset_);
        }

        template<typename T, typename = std::enable_if_t<std::is_class_v<T>>>
        Data_r& operator=(T const& o) {
            Reset(o.buf, o.len);
            return *this;
        }

        bool operator==(Data_r const& o) {
            return this->Span::operator==(o);
        }

        bool operator!=(Data_r const& o) {
            return !this->operator==(o);
        }

        uint8_t* GetBuf() const {
            return buf;
        }

        size_t GetLen() const {
            return len;
        }

        bool HasLeft() const {
            return len > offset;
        }

        size_t LeftLen() const {
            return len - offset;
        }

        Span LeftSpan() const {
            return Span(buf + offset, len - offset);
        }

        Data_r LeftData_r(size_t offset_ = 0) const {
            return Data_r(buf + offset, len - offset, offset_);
        }

        /***************************************************************************************************************************/
        // return !0 mean read fail.

        // return left buf + len( do not change offset )
        std::pair<uint8_t*, size_t> GetLeftBuf() {
            return { buf + offset, len - offset };
        }

        // dr.buf & len = left this.buf + len
        int ReadLeftBuf(Data_r& dr) {
            if (offset == len) return __LINE__;
            dr.Reset(buf + offset, len - offset);
            offset = len;
            return 0;
        }

        // skip siz bytes
        int ReadJump(size_t siz) {
            assert(siz);
            if (offset + siz > len) return __LINE__;
            offset += siz;
            return 0;
        }

        // memcpy fixed siz data( from offset ) to tar
        int ReadBuf(void* tar, size_t siz) {
            assert(tar);
            if (offset + siz > len) return __LINE__;
            memcpy(tar, buf + offset, siz);
            offset += siz;
            return 0;
        }

        // memcpy fixed siz data( from specific idx ) to tar
        int ReadBufAt(size_t idx, void* tar, size_t siz) const {
            assert(tar);
            if (idx + siz > len) return __LINE__;
            memcpy(tar, buf + idx, siz);
            return 0;
        }

        // return pointer( fixed len, from offset ) for memcpy
        void* ReadBuf(size_t siz) {
            if (offset + siz > len) return nullptr;
            auto bak = offset;
            offset += siz;
            return buf + bak;
        }

        // return pointer( fixed len, from specific idx ) for read / memcpy
        void* ReadBufAt(size_t idx, size_t siz) const {
            if (idx + siz > len) return nullptr;
            return buf + idx;
        }

        // read fixed len little endian number
        template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>>>
        int ReadFixed(T& v) {
            if (offset + sizeof(T) > len) return __LINE__;
            memcpy(&v, buf + offset, sizeof(T));
            if constexpr (std::endian::native == std::endian::big) {
                v = BSwap(v);
            }
            offset += sizeof(T);
            return 0;
        }

        // read fixed len little endian number from specific idx ( do not change offset )
        template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>>>
        int ReadFixedAt(size_t idx, T& v) {
            if (idx + sizeof(T) > len) return __LINE__;
            memcpy(&v, buf + idx, sizeof(T));
            if constexpr (std::endian::native == std::endian::big) {
                v = BSwap(v);
            }
            return 0;
        }

        // read fixed len big endian number
        template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>>>
        int ReadFixedBE(T& v) {
            if (offset + sizeof(T) > len) return __LINE__;
            memcpy(&v, buf + offset, sizeof(T));
            if constexpr (std::endian::native == std::endian::little) {
                v = BSwap(v);
            }
            offset += sizeof(T);
            return 0;
        }

        // read fixed len big endian number from specific idx ( do not change offset )
        template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>>>
        int ReadFixedBEAt(size_t idx, T& v) {
            if (idx + sizeof(T) >= len) return __LINE__;
            memcpy(&v, buf + idx, sizeof(T));
            if constexpr (std::endian::native == std::endian::little) {
                v = BSwap(v);
            }
            return 0;
        }

        // read fixed len little endian number array
        template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>>>
        int ReadFixedArray(T* tar, size_t siz) {
            assert(tar);
            if (offset + sizeof(T) * siz > len) return __LINE__;
            if constexpr (std::endian::native == std::endian::big) {
                auto p = buf + offset;
                T v;
                for (size_t i = 0; i < siz; ++i) {
                    memcpy(&v, p + i * sizeof(T), sizeof(T));
                    tar[i] = BSwap(v);
                }
            }
            else {
                memcpy(tar, buf + offset, sizeof(T) * siz);
            }
            offset += sizeof(T) * siz;
            return 0;
        }


        // read variable length integer
        template<typename T>
        int ReadVarInteger(T& v) {
            using UT = std::make_unsigned_t<T>;
            UT u(0);
            for (size_t shift = 0; shift < sizeof(T) * 8; shift += 7) {
                if (offset == len) return __LINE__;
                auto b = (UT)buf[offset++];
                u |= UT((b & 0x7Fu) << shift);
                if ((b & 0x80) == 0) {
                    if constexpr (std::is_signed_v<T>) {
                        if constexpr (sizeof(T) <= 4) v = ZigZagDecode(uint32_t(u));
                        else v = ZigZagDecode(uint64_t(u));
                    }
                    else {
                        v = u;
                    }
                    return 0;
                }
            }
            return __LINE__;
        }

        // read buf + len to sv
        int ReadSV(std::string_view& sv, size_t siz) {
            if (offset + siz >= len) return __LINE__;
            auto s = (char*)buf + offset;
            offset += siz;
            sv = { s, siz };
            return 0;
        }

        // read "c string\0" to sv
        int ReadCStr(std::string_view& sv) {
            return ReadSV(sv, strlen((char*)buf + offset));
        }
        int ReadCStr(std::string& s) {
            std::string_view sv;
            if (int r = ReadCStr(sv)) return r;
            s = sv;
            return 0;
        }

    };

    // mem moveable tag
    template<>
    struct IsPod<Data_r, void> : std::true_type {};


    // container( buf + len + cap + offset )
    struct Data : Data_r {
        size_t cap;

        Data()
            : cap(0) {
        }

        // unsafe: override values
        void Reset(void const* buf_ = nullptr, size_t len_ = 0, size_t offset_ = 0, size_t cap_ = 0) {
            this->Data_r::Reset(buf_, len_, offset_);
            cap = cap_;
        }

        // cap: reserve len
        explicit Data(size_t cap)
            : cap(cap) {
            assert(cap);
            auto siz = Round2n(cap);
            buf = (new uint8_t[siz]);
            this->cap = siz;
        }

        // memcpy( offset = 0 )
        Data(Span const& s)
            : cap(0) {
            WriteBuf(s.buf, s.len);
        }

        // memcpy + set offset
        Data(void const* ptr, size_t siz, size_t offset_ = 0)
            : cap(0) {
            WriteBuf(ptr, siz);
            offset = offset_;
        }

        // memcpy( offset = 0 )
        Data(Data const& o)
            : cap(0) {
            operator=(o);
        }

        // memcpy( offset = 0 )
        Data& operator=(Data const& o) {
            return operator=<Data>(o);
        }

        // memcpy( offset = 0 )( o have field: buf + len )
        template<typename T, typename = std::enable_if_t<std::is_class_v<T>>>
        Data& operator=(T const& o) {
            if (this == &o) return *this;
            Clear();
            WriteBuf(o.buf, o.len);
            return *this;
        }

        // move o's memory to this
        Data(Data&& o) noexcept {
            ::memcpy((void*)this, &o, sizeof(Data));
            ::memset((void*)&o, 0, sizeof(Data));
        }

        // swap
        Data& operator=(Data&& o) noexcept {
            std::swap(buf, o.buf);
            std::swap(len, o.len);
            std::swap(cap, o.cap);
            std::swap(offset, o.offset);
            return *this;
        }

        // memcmp data( ignore offset, cap )
        bool operator==(Data const& o) const {
            return this->Span::operator==(o);
        }

        bool operator!=(Data const& o) const {
            return !this->operator==(o);
        }

        // ensure free space is enough( round2n == false usually for big file data, cap == len )
        template<bool checkCap = true, bool round2n = true>
        XX_NOINLINE void Reserve(size_t newCap) {
            if constexpr (checkCap) {
                if (newCap <= cap) return;
            }

            size_t siz;
            if constexpr (round2n) {
                siz = Round2n(newCap);
            }
            else {
                siz = newCap;
            }

            auto newBuf = new uint8_t[siz];
            if (len) {
                ::memcpy(newBuf, buf, len);
            }

            // check cap for gcc issue
            if (cap) {
                delete[](buf);
            }
            buf = newBuf;
            cap = siz;
        }

        // buf cap resize to len
        void Shrink() {
            if (!len) {
                Clear(true);
            }
            else if (cap > len * 2) {
                auto newBuf = new uint8_t[len];
                ::memcpy(newBuf, buf, len);
                delete[](buf);
                buf = newBuf;
                cap = len;
            }
        }

        // make a copy ( len == cap ) ( do not copy header )
        Data ShrinkCopy() {
            Data rtv;
            if (len) {
                rtv.buf = new uint8_t[len];
                ::memcpy(rtv.buf, buf, len);
                rtv.cap = rtv.len = len;
            }
            return rtv;
        }

        // resize & return old len
        size_t Resize(size_t newLen) {
            if (newLen > cap) {
                Reserve<false>(newLen);
            }
            auto rtv = len;
            len = newLen;
            return rtv;
        }

        // fill data by initializer list. will Clear. example: d.Fill({ 1,2,3. ....})
        template<typename T = int32_t, typename = std::enable_if_t<std::is_convertible_v<T, uint8_t>>>
        void Fill(std::initializer_list<T> const& bytes) {
            Clear();
            Reserve(bytes.size());
            for (auto&& b : bytes) {
                buf[len++] = (uint8_t)b;
            }
        }

        // fill likely. make instance
        template<typename T = int32_t, typename = std::enable_if_t<std::is_convertible_v<T, uint8_t>>>
        static Data From(std::initializer_list<T> const& bytes) {
            Data d;
            d.Fill(bytes);
            return d;
        }

        template<typename T = int32_t, typename = std::enable_if_t<std::is_convertible_v<T, std::string_view>>>
        static Data From(T const& sv) {
            Data d;
            d.WriteBuf(sv);
            return d;
        }

        // erase data from head
        void RemoveFront(size_t siz) {
            assert(siz <= len);
            if (!siz) return;
            len -= siz;
            if (len) {
                memmove(buf, buf + siz, len);
            }
        }

        // for fill read data
        Span GetFreeRange() const {
            return { buf + len, cap - len };
        }

        // append data
        template<bool needReserve = true>
        void WriteBuf(void const* ptr, size_t siz) {
            if constexpr (needReserve) {
                if (len + siz > cap) {
                    Reserve<false>(len + siz);
                }
            }
            memcpy(buf + len, ptr, siz);
            len += siz;
        }

        // support literal string[_view] for easy use
        template<bool needReserve = true>
        void WriteBuf(std::string const& sv) {
            WriteBuf<needReserve>(sv.data(), sv.size());
        }

        template<bool needReserve = true>
        void WriteBuf(std::string_view const& sv) {
            WriteBuf<needReserve>(sv.data(), sv.size());
        }

        template<bool needReserve = true, size_t n>
        void WriteBuf(char const(&s)[n]) {
            WriteBuf<needReserve>(s, n - 1);
        }

        // write data to specific idx
        void WriteBufAt(size_t idx, void const* ptr, size_t siz) {
            if (idx + siz > len) {
                Resize(idx + siz);
            }
            memcpy(buf + idx, ptr, siz);
        }

        // append write float / double / integer ( fixed size Little Endian )
        template<bool needReserve = true, typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>>>
        void WriteFixed(T v) {
            if constexpr (needReserve) {
                if (len + sizeof(T) > cap) {
                    Reserve<false>(len + sizeof(T));
                }
            }
            if constexpr (std::endian::native == std::endian::big) {
                v = BSwap(v);
            }
            memcpy(buf + len, &v, sizeof(T));
            len += sizeof(T);
        }

        // write float / double / integer ( fixed size Little Endian ) to specific index
        template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>>>
        void WriteFixedAt(size_t idx, T v) {
            if (idx + sizeof(T) > len) {
                Resize(sizeof(T) + idx);
            }
            if constexpr (std::endian::native == std::endian::big) {
                v = BSwap(v);
            }
            memcpy(buf + idx, &v, sizeof(T));
        }

        // append write float / double / integer ( fixed size Big Endian )
        template<bool needReserve = true, typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>>>
        void WriteFixedBE(T v) {
            if constexpr (needReserve) {
                if (len + sizeof(T) > cap) {
                    Reserve<false>(len + sizeof(T));
                }
            }
            if constexpr (std::endian::native == std::endian::little) {
                v = BSwap(v);
            }
            memcpy(buf + len, &v, sizeof(T));
            len += sizeof(T);
        }

        // write float / double / integer ( fixed size Big Endian ) to specific index
        template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>>>
        void WriteFixedBEAt(size_t idx, T v) {
            if (idx + sizeof(T) > len) {
                Resize(sizeof(T) + idx);
            }
            if constexpr (std::endian::native == std::endian::little) {
                v = BSwap(v);
            }
            memcpy(buf + idx, &v, sizeof(T));
        }

        // append write float / double / integer ( fixed size Little Endian ) array
        template<bool needReserve = true, typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>>>
        void WriteFixedArray(T const* ptr, size_t siz) {
            assert(ptr);
            if constexpr (needReserve) {
                if (len + sizeof(T) * siz > cap) {
                    Reserve<false>(len + sizeof(T) * siz);
                }
            }
            if constexpr (std::endian::native == std::endian::big) {
                auto p = buf + len;
                T v;
                for (size_t i = 0; i < siz; ++i) {
                    v = BSwap(ptr[i]);
                    memcpy(p + i * sizeof(T), &v, sizeof(T));
                }
            }
            else {
                memcpy(buf + len, ptr, sizeof(T) * siz);
            }
            len += sizeof(T) * siz;
        }

        // append write variable length integer( 7bit format )
        template<bool needReserve = true, typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
        void WriteVarInteger(T const& v) {
            using UT = std::make_unsigned_t<T>;
            UT u(v);
            if constexpr (std::is_signed_v<T>) {
                if constexpr (sizeof(T) <= 4) u = ZigZagEncode(int32_t(v));
                else u = ZigZagEncode(int64_t(v));
            }
            if constexpr (needReserve) {
                if (len + sizeof(T) + 2 > cap) {
                    Reserve<false>(len + sizeof(T) + 2);
                }
            }
            while (u >= 1 << 7) {
                buf[len++] = uint8_t((u & 0x7fu) | 0x80u);
                u = UT(u >> 7);
            }
            buf[len++] = uint8_t(u);
        }

        // skip specific size space. backup len and return
        template<bool needReserve = true>
        size_t WriteJump(size_t siz) {
            auto bak = len;
            if constexpr (needReserve) {
                if (len + siz > cap) {
                    Reserve<false>(len + siz);
                }
            }
            len += siz;
            return bak;
        }

        // skip specific size space. backup pointer and return
        template<bool needReserve = true>
        uint8_t* WriteSpace(size_t siz) {
            return buf + WriteJump<needReserve>(siz);
        }

        // TS is base of Span. write buf only, do not write length
        template<bool needReserve = true, typename ...TS>
        void WriteBufSpans(TS const& ...vs) {
            (WriteBuf(vs.buf, vs.len), ...);
        }


        /***************************************************************************************************************************/

        ~Data() {
            Clear(true);
        }

        // set len & offset = 0, support free buf( cap = 0 )
        void Clear(bool freeBuf = false) {
            if (freeBuf && cap) {
                delete[](buf);
                buf = nullptr;
                cap = 0;
            }
            len = 0;
            offset = 0;
        }
    };

    // mem moveable tag
    template<>
    struct IsPod<Data, void> : std::true_type {};

}
