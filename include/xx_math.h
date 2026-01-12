#pragma once
#include "xx_prim.h"
#include "xx_rnd.h"

namespace xx {
	
    bool IsIntersect_BoxBoxF(XY b1minXY, XY b1maxXY, XY b2minXY, XY b2maxXY);

    bool IsIntersect_BoxPointF(XY b1minXY, XY b1maxXY, XY p);

    float CalcBounce(float x);

    XY GetRndPosDoughnut(Rnd& rnd_, float maxRadius_, float safeRadius_ = 0.f, float radiansFrom_ = -M_PI, float radiansTo_ = M_PI);

}
