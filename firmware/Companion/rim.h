#pragma once

namespace dial {
constexpr int center=180;
constexpr int rimInner=132, rimOuter=180, labelRadius=158;
constexpr int returnRadius=136,returnHalfWidth=22,returnAngle=23;
// Rounded end caps at +/-23 degrees from six o'clock.
inline bool inReturn(int x,int y) {
    int dx=x-center,dy=y-center;
    int ax=dx<0?-dx:dx;
    if (dy<=0) return false;
    if (ax*125<=dy*53) {
        int r=dx*dx+dy*dy;
        return r>=(returnRadius-returnHalfWidth)*(returnRadius-returnHalfWidth) &&
               r<=(returnRadius+returnHalfWidth)*(returnRadius+returnHalfWidth);
    }
    return (ax-53)*(ax-53)+(dy-125)*(dy-125)<=returnHalfWidth*returnHalfWidth;
}
enum class Rim { Top, Bottom };

inline bool inRim(int x,int y,Rim edge) {
    int dx=x-center, dy=y-center;
    int toward=edge==Rim::Top?-dy:dy;
    int squared=dx*dx+dy*dy;
    return toward>=0 && dx>=-toward && dx<=toward &&
           squared>=rimInner*rimInner && squared<=rimOuter*rimOuter;
}
inline bool inHub(int x,int y) {
    int dx=x-center, dy=y-center;
    return dx*dx+dy*dy<=116*116;
}
}
