#pragma once
#include "xx_ptr.h"
#include "xx_prim.h"
#include "xx_gl.h"

namespace xx {
	
    // for bmfont
    struct FontFrame {
        GLuint texId;
        UVRect uvRect;
    };


    // base sprite frame
    struct TinyFrame {
        Shared<GLTexture> tex;
        UVRect uvRect{};

        TinyFrame() = default;
        TinyFrame(TinyFrame const&) = default;
        TinyFrame(TinyFrame&&) = default;
        TinyFrame& operator=(TinyFrame const&) = default;
        TinyFrame& operator=(TinyFrame&&) = default;

        operator GLuint () const {
            return tex->id;
        }

        operator UVRect () const {
            return uvRect;
        }

        XY Size() const {
            return { uvRect.w, uvRect.h };
        }

        TinyFrame(Shared<GLTexture> t) {
            operator=(std::move(t));
        }

        template<typename T>
        TinyFrame(Shared<GLTexture> t, T x, T y, T w, T h) {
            tex = std::move(t);
            uvRect = { (uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h };
        }

        void operator=(Shared<GLTexture> t) {
            tex = std::move(t);
            uvRect = tex->Rect();
        }
    };


    // sprite frame with anchor point
    struct Frame {
        Shared<GLTexture> tex;
        UVRect uvRect{};
        XY anchor{};
        ptrdiff_t userData{};

        Frame() = default;
        Frame(Frame const&) = default;
        Frame(Frame&&) = default;
        Frame& operator=(Frame const&) = default;
        Frame& operator=(Frame&&) = default;

        operator TinyFrame& () const {
            return *(TinyFrame*)this;
        }
        operator TinyFrame* () const {
            return (TinyFrame*)this;
        }

        operator GLuint () const {
            return tex->id;
        }

        operator UVRect () const {
            return uvRect;
        }

        operator XY const& () const {
            return anchor;
        }

        XY Size() const {
            return { uvRect.w, uvRect.h };
        }

        Frame(Shared<GLTexture> t) {
            operator=(std::move(t));
        }

        template<typename T>
        Frame(Shared<GLTexture> t, T x, T y, T w, T h, XY anchor_ = 0.5f) {
            tex = std::move(t);
            uvRect = { (uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h };
            anchor = anchor_;
        }

        void operator=(Shared<GLTexture> t) {
            tex = std::move(t);
            uvRect = tex->Rect();
            anchor = 0.5f;
        }
    };

}
