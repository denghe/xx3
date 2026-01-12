#pragma once
#include "xx_prim.h"

namespace xx {
	
	// AffineTransform without rotation support
    struct SimpleAffineTransform {
        float a{ 1 }, d{ 1 };
        float tx{}, ty{};
        
        void PosScaleAnchorSize(XY const& pos, XY const& scale, XY const& anchorSize);	// anchorSize = anchor * size
        void Identity();
        XY const& Offset() const;
        XY const& Scale() const;
        XY operator()(XY const& point) const;	// apply
        SimpleAffineTransform MakeConcat(SimpleAffineTransform const& t2) const;	// child concat parent
        SimpleAffineTransform MakeInvert() const;
        static SimpleAffineTransform MakeIdentity();
        static SimpleAffineTransform MakePosScaleAnchorSize(XY const& pos, XY const& scale, XY const& anchorSize);
    };

}
