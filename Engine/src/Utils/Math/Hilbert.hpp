#pragma once


namespace MathUtils
{
// Return the Hilbert index for the given x and y coordinates
// https://www.shadertoy.com/view/3tB3z3
inline uint32_t HilbertIndex(uint32_t x, uint32_t y, uint32_t level)
{
    uint32_t index = 0;
    for(int n = level - 1; n >= 0; --n)
    {
        uint32_t rX  = (x >> n) & 1;
        uint32_t rY  = (y >> n) & 1;
        index       += ((3 * rX) ^ rY) << (2 * n);
        if(rY == 0)
        {
            if(rX == 1)
            {
                x = (1 << n) - 1 - x;
                y = (1 << n) - 1 - y;
            }
            std::swap(x, y);
        }
    }
    return index;
}

// Generate an LUT for a Hilbert curve of given size (power of 2)
inline std::vector<uint32_t> GenerateHilbertLUT(uint32_t size)
{
    std::vector<uint32_t> lut(size * size);
    uint32_t level = std::log2(size);
    for(uint32_t y = 0; y < size; ++y)
    {
        for(uint32_t x = 0; x < size; ++x)
        {
            lut[y * size + x] = HilbertIndex(x, y, level);
        }
    }
    return lut;
}
}
