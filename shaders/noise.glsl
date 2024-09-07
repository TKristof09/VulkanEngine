
// quasi-random sequence indexed by a hilbert curve
// http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
vec2 SpatialNoiseR2(ivec2 pixelPos, image2D hilbertLUT)
{
    float idx = imageLoad(hilbertLUT, pixelPos % imageSize(hilbertLUT)).r;
    return vec2(fract(0.5 + idx * vec2(0.7548776662466927, 0.5698402909980532)));
}

float SpatialNoiseR1(ivec2 pixelPos, image2D hilbertLUT)
{
    float idx = imageLoad(hilbertLUT, pixelPos % imageSize(hilbertLUT)).r;
    return fract(0.5 + idx * 0.6180339887498948);
}
