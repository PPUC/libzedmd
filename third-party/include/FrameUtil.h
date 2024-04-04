/*
 * Portions of this code was derived from DMDExt:
 * https://github.com/freezy/dmd-extensions/blob/master/LibDmd/Common/FrameUtil.cs
 *
 * Some were extracted from libzedmd:
 * https://github.com/PPUC/libzedmd
 */

#pragma once

#define FRAMEUTIL_VERSION_MAJOR 0  // X Digits
#define FRAMEUTIL_VERSION_MINOR 2  // Max 2 Digits
#define FRAMEUTIL_VERSION_PATCH 0  // Max 2 Digits

#define _FRAMEUTIL_STR(x) #x
#define FRAMEUTIL_STR(x) _FRAMEUTIL_STR(x)

#define FRAMEUTIL_VERSION                \
  FRAMEUTIL_STR(FRAMEUTIL_VERSION_MAJOR) \
  "." FRAMEUTIL_STR(FRAMEUTIL_VERSION_MINOR) "." FRAMEUTIL_STR(FRAMEUTIL_VERSION_PATCH)
#define FRAMEUTIL_MINOR_VERSION FRAMEUTIL_STR(FRAMEUTIL_VERSION_MAJOR) "." FRAMEUTIL_STR(FRAMEUTIL_VERSION_MINOR)

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace FrameUtil
{

enum class ColorMatrix
{
  Rgb,
  Rbg
};

class Helper
{
 public:
  static int MapAdafruitIndex(int x, int y, int width, int height, int numLogicalRows);
  static void ConvertToRgb24(uint8_t* pFrameRgb24, uint8_t* pFrame, int size, uint8_t* pPalette);
  static void Split(uint8_t* pPlanes, uint16_t width, uint16_t height, uint8_t bitlen, uint8_t* pFrame);
  static void SplitIntoRgbPlanes(const uint16_t* rgb565, int rgb565Size, int width, int numLogicalRows, uint8_t* dest,
                                 ColorMatrix colorMatrix = ColorMatrix::Rgb);
  static float CalcBrightness(float x);
  static void ScaleDownIndexed(uint8_t* pDestFrame, const uint16_t destWidth, const uint8_t destHeight,
                               const uint8_t* pSrcFrame, const uint16_t srcWidth, const uint8_t srcHeight);
  static void ScaleDownPUP(uint8_t* pDestFrame, const uint16_t destWidth, const uint8_t destHeight,
                           const uint8_t* pSrcFrame, const uint16_t srcWidth, const uint8_t srcHeight);
  static void ScaleDown(uint8_t* pDestFrame, const uint16_t destWidth, const uint8_t destHeight,
                        const uint8_t* pSrcFrame, const uint16_t srcWidth, const uint8_t srcHeight, uint8_t bits);
  static void ScaleUpIndexed(uint8_t* pDestFrame, const uint8_t* pSrcFrame, const uint16_t srcWidth,
                             const uint8_t srcHeight);
  static void ScaleUp(uint8_t* pDestFrame, const uint8_t* pSrcFrame, const uint16_t srcWidth, const uint8_t srcHeight,
                      uint8_t bits);
  static void CenterIndexed(uint8_t* pDestFrame, const uint16_t destWidth, const uint8_t destHeight,
                            const uint8_t* pSrcFrame, const uint16_t srcWidth, const uint8_t srcHeight);
  static void Center(uint8_t* pDestFrame, const uint16_t destWidth, const uint8_t destHeight, const uint8_t* pSrcFrame,
                     const uint16_t srcWidth, const uint8_t srcHeight, uint8_t bits);
};

inline int Helper::MapAdafruitIndex(int x, int y, int width, int height, int numLogicalRows)
{
  int logicalRowLengthPerMatrix = 32 * 32 / 2 / numLogicalRows;
  int logicalRow = y % numLogicalRows;
  int dotPairsPerLogicalRow = width * height / numLogicalRows / 2;
  int widthInMatrices = width / 32;
  int matrixX = x / 32;
  int matrixY = y / 32;
  int totalMatrices = width * height / 1024;
  int matrixNumber = totalMatrices - ((matrixY + 1) * widthInMatrices) + matrixX;
  int indexWithinMatrixRow = x % logicalRowLengthPerMatrix;
  int index = logicalRow * dotPairsPerLogicalRow + matrixNumber * logicalRowLengthPerMatrix + indexWithinMatrixRow;
  return index;
}

inline void Helper::Split(uint8_t* pPlanes, uint16_t width, uint16_t height, uint8_t bitlen, uint8_t* pFrame)
{
  int planeSize = width * height / 8;
  int pos = 0;
  uint8_t* bd = (uint8_t*)malloc(bitlen);

  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x += 8)
    {
      memset(bd, 0, bitlen * sizeof(uint8_t));

      for (int v = 7; v >= 0; v--)
      {
        uint8_t pixel = pFrame[(y * width) + (x + v)];
        for (int i = 0; i < bitlen; i++)
        {
          bd[i] <<= 1;
          if ((pixel & (1 << i)) != 0)
          {
            bd[i] |= 1;
          }
        }
      }

      for (int i = 0; i < bitlen; i++)
      {
        pPlanes[i * planeSize + pos] = bd[i];
      }

      pos++;
    }
  }

  free(bd);
}

inline void Helper::ConvertToRgb24(uint8_t* pFrameRgb24, uint8_t* pFrame, int size, uint8_t* pPalette)
{
  for (int i = 0; i < size; i++)
  {
    memcpy(&pFrameRgb24[i * 3], &pPalette[pFrame[i] * 3], 3);
  }
}

inline void Helper::SplitIntoRgbPlanes(const uint16_t* rgb565, int rgb565Size, int width, int numLogicalRows,
                                       uint8_t* dest, ColorMatrix colorMatrix)
{
  constexpr int pairOffset = 16;
  int height = rgb565Size / width;
  int subframeSize = rgb565Size / 2;

  for (int x = 0; x < width; ++x)
  {
    for (int y = 0; y < height; ++y)
    {
      if (y % (pairOffset * 2) >= pairOffset) continue;

      int inputIndex0 = y * width + x;
      int inputIndex1 = inputIndex0 + pairOffset * width;

      uint16_t color0 = rgb565[inputIndex0];
      uint16_t color1 = rgb565[inputIndex1];

      int r0 = 0, r1 = 0, g0 = 0, g1 = 0, b0 = 0, b1 = 0;
      switch (colorMatrix)
      {
        case ColorMatrix::Rgb:
          r0 = (color0 >> 13) /*& 0x7*/;
          g0 = (color0 >> 8) /*& 0x7*/;
          b0 = (color0 >> 2) /*& 0x7*/;
          r1 = (color1 >> 13) /*& 0x7*/;
          g1 = (color1 >> 8) /*& 0x7*/;
          b1 = (color1 >> 2) /*& 0x7*/;
          break;

        case ColorMatrix::Rbg:
          r0 = (color0 >> 13) /*& 0x7*/;
          b0 = (color0 >> 8) /*& 0x7*/;
          g0 = (color0 >> 2) /*& 0x7*/;
          r1 = (color1 >> 13) /*& 0x7*/;
          b1 = (color1 >> 8) /*& 0x7*/;
          g1 = (color1 >> 2) /*& 0x7*/;
          break;
      }

      for (int subframe = 0; subframe < 3; ++subframe)
      {
        uint8_t dotPair = (r0 & 1) << 5 | (g0 & 1) << 4 | (b0 & 1) << 3 | (r1 & 1) << 2 | (g1 & 1) << 1 | (b1 & 1);
        int indexWithinSubframe = MapAdafruitIndex(x, y, width, height, numLogicalRows);
        int indexWithinOutput = subframe * subframeSize + indexWithinSubframe;
        dest[indexWithinOutput] = dotPair;
        r0 >>= 1;
        g0 >>= 1;
        b0 >>= 1;
        r1 >>= 1;
        g1 >>= 1;
        b1 >>= 1;
      }
    }
  }
}

inline float Helper::CalcBrightness(float x)
{
  // function to improve the brightness with fx=ax²+bc+c, f(0)=0, f(1)=1, f'(1.1)=0
  return (-x * x + 2.1f * x) / 1.1f;
}

inline void Helper::ScaleDownIndexed(uint8_t* pDestFrame, const uint16_t destWidth, const uint8_t destHeight,
                                     const uint8_t* pSrcFrame, const uint16_t srcWidth, const uint8_t srcHeight)
{
  memset(pDestFrame, 0, destWidth * destHeight);
  uint8_t xOffset = (destWidth - (srcWidth / 2)) / 2;
  uint8_t yOffset = (destHeight - (srcHeight / 2)) / 2;

  // for half scaling we take the 4 points and look if there is one color repeated
  for (uint8_t y = 0; y < srcHeight; y += 2)
  {
    std::vector<uint8_t> row;
    row.reserve(srcWidth / 2);

    for (uint16_t x = 0; x < srcWidth; x += 2)
    {
      uint8_t upper_left = pSrcFrame[y * srcWidth + x];
      uint8_t upper_right = pSrcFrame[y * srcWidth + x + 1];
      uint8_t lower_left = pSrcFrame[(y + 1) * srcWidth + x];
      uint8_t lower_right = pSrcFrame[(y + 1) * srcWidth + x + 1];

      if (x < srcWidth / 2)
      {
        if (y < srcHeight / 2)
        {
          if (upper_left == upper_right || upper_left == lower_left || upper_left == lower_right)
            row.push_back(upper_left);
          else if (upper_right == lower_left || upper_right == lower_right)
            row.push_back(upper_right);
          else if (lower_left == lower_right)
            row.push_back(lower_left);
          else
            row.push_back(upper_left);
        }
        else
        {
          if (lower_left == lower_right || lower_left == upper_left || lower_left == upper_right)
            row.push_back(lower_right);
          else if (lower_right == upper_left || lower_right == upper_right)
            row.push_back(lower_right);
          else if (upper_left == upper_right)
            row.push_back(upper_left);
          else
            row.push_back(lower_left);
        }
      }
      else
      {
        if (y < srcHeight / 2)
        {
          if (upper_right == upper_left || upper_right == lower_right || upper_right == lower_left)
            row.push_back(upper_right);
          else if (upper_left == lower_right || upper_left == lower_left)
            row.push_back(upper_left);
          else if (lower_right == lower_left)
            row.push_back(lower_right);
          else
            row.push_back(upper_right);
        }
        else
        {
          if (lower_right == lower_left || lower_right == upper_right || lower_right == upper_left)
            row.push_back(lower_right);
          else if (lower_left == upper_right || lower_left == upper_left)
            row.push_back(lower_left);
          else if (upper_right == upper_left)
            row.push_back(upper_right);
          else
            row.push_back(lower_right);
        }
      }
    }

    memcpy(&pDestFrame[(yOffset + (y / 2)) * destWidth + xOffset], row.data(), srcWidth / 2);
  }
}

inline void Helper::ScaleDownPUP(uint8_t* pDestFrame, const uint16_t destWidth, const uint8_t destHeight,
                                 const uint8_t* pSrcFrame, const uint16_t srcWidth, const uint8_t srcHeight)
{
  memset(pDestFrame, 0, destWidth * destHeight);
  uint8_t xOffset = (destWidth - (srcWidth / 2)) / 2;
  uint8_t yOffset = (destHeight - (srcHeight / 2)) / 2;

  // for half scaling we take the 4 points and look if there is one color repeated
  for (uint8_t y = 0; y < srcHeight; y += 2)
  {
    std::vector<uint8_t> row;
    row.reserve(srcWidth / 2);

    for (uint16_t x = 0; x < srcWidth; x += 2)
    {
      uint8_t pixel1 = pSrcFrame[y * srcWidth + x];
      uint8_t pixel2 = pSrcFrame[y * srcWidth + x + 1];
      uint8_t pixel3 = pSrcFrame[(y + 1) * srcWidth + x];
      uint8_t pixel4 = pSrcFrame[(y + 1) * srcWidth + x + 1];

      if (pixel1 == pixel2 || pixel1 == pixel3 || pixel1 == pixel4)
        row.push_back(pixel1);
      else if (pixel2 == pixel3 || pixel2 == pixel4)
        row.push_back(pixel2);
      else if (pixel3 == pixel4)
        row.push_back(pixel3);
      else
        row.push_back(pixel1);
    }

    memcpy(&pDestFrame[(yOffset + (y / 2)) * destWidth + xOffset], row.data(), srcWidth / 2);
  }
}

inline void Helper::ScaleDown(uint8_t* pDestFrame, const uint16_t destWidth, const uint8_t destHeight,
                              const uint8_t* pSrcFrame, const uint16_t srcWidth, const uint8_t srcHeight, uint8_t bits)
{
  memset(pDestFrame, 0, destWidth * destHeight);
  uint8_t xOffset = (destWidth - (srcWidth / 2)) / 2;
  uint8_t yOffset = (destHeight - (srcHeight / 2)) / 2;
  uint8_t bytes = bits / 8;  // RGB24 (3 byte) or RGB16 (2 byte)

  for (uint8_t y = 0; y < srcHeight; y += 2)
  {
    for (uint16_t x = 0; x < srcWidth; x += 2)
    {
      uint16_t upper_left = y * srcWidth * bytes + x * bytes;
      uint16_t upper_right = upper_left + bytes;
      uint16_t lower_left = upper_left + srcWidth * bytes;
      uint16_t lower_right = lower_left + bytes;
      uint16_t target = (xOffset + (x / 2) + (y / 2) * destHeight) * bytes;

      if (x < srcWidth / 2)
      {
        if (y < srcHeight / 2)
        {
          if (memcmp(&pSrcFrame[upper_left], &pSrcFrame[upper_right], bytes) == 0 ||
              memcmp(&pSrcFrame[upper_left], &pSrcFrame[lower_left], bytes) == 0 ||
              memcmp(&pSrcFrame[upper_left], &pSrcFrame[lower_right], bytes) == 0)
            memcpy(&pDestFrame[target], &pSrcFrame[upper_left], bytes);
          else if (memcmp(&pSrcFrame[upper_right], &pSrcFrame[lower_left], bytes) == 0 ||
                   memcmp(&pSrcFrame[upper_right], &pSrcFrame[lower_right], bytes) == 0)
            memcpy(&pDestFrame[target], &pSrcFrame[upper_right], bytes);
          else if (memcmp(&pSrcFrame[lower_left], &pSrcFrame[lower_right], bytes) == 0)
            memcpy(&pDestFrame[target], &pSrcFrame[lower_left], bytes);
          else
            memcpy(&pDestFrame[target], &pSrcFrame[upper_left], bytes);
        }
        else
        {
          if (memcmp(&pSrcFrame[lower_left], &pSrcFrame[lower_right], bytes) == 0 ||
              memcmp(&pSrcFrame[lower_left], &pSrcFrame[upper_left], bytes) == 0 ||
              memcmp(&pSrcFrame[lower_left], &pSrcFrame[upper_right], bytes) == 0)
            memcpy(&pDestFrame[target], &pSrcFrame[lower_left], bytes);
          else if (memcmp(&pSrcFrame[lower_right], &pSrcFrame[upper_left], bytes) == 0 ||
                   memcmp(&pSrcFrame[lower_right], &pSrcFrame[upper_right], bytes) == 0)
            memcpy(&pDestFrame[target], &pSrcFrame[lower_right], bytes);
          else if (memcmp(&pSrcFrame[upper_left], &pSrcFrame[upper_right], bytes) == 0)
            memcpy(&pDestFrame[target], &pSrcFrame[upper_left], bytes);
          else
            memcpy(&pDestFrame[target], &pSrcFrame[lower_left], bytes);
        }
      }
      else
      {
        if (y < srcHeight / 2)
        {
          if (memcmp(&pSrcFrame[upper_right], &pSrcFrame[upper_left], bytes) == 0 ||
              memcmp(&pSrcFrame[upper_right], &pSrcFrame[lower_right], bytes) == 0 ||
              memcmp(&pSrcFrame[upper_right], &pSrcFrame[lower_left], bytes) == 0)
            memcpy(&pDestFrame[target], &pSrcFrame[upper_right], bytes);
          else if (memcmp(&pSrcFrame[upper_left], &pSrcFrame[lower_right], bytes) == 0 ||
                   memcmp(&pSrcFrame[upper_left], &pSrcFrame[lower_left], bytes) == 0)
            memcpy(&pDestFrame[target], &pSrcFrame[upper_left], bytes);
          else if (memcmp(&pSrcFrame[lower_right], &pSrcFrame[lower_left], bytes) == 0)
            memcpy(&pDestFrame[target], &pSrcFrame[lower_right], bytes);
          else
            memcpy(&pDestFrame[target], &pSrcFrame[upper_right], bytes);
        }
        else
        {
          if (memcmp(&pSrcFrame[lower_right], &pSrcFrame[lower_left], bytes) == 0 ||
              memcmp(&pSrcFrame[lower_right], &pSrcFrame[upper_right], bytes) == 0 ||
              memcmp(&pSrcFrame[lower_right], &pSrcFrame[upper_left], bytes) == 0)
            memcpy(&pDestFrame[target], &pSrcFrame[lower_right], bytes);
          else if (memcmp(&pSrcFrame[lower_left], &pSrcFrame[upper_right], bytes) == 0 ||
                   memcmp(&pSrcFrame[lower_left], &pSrcFrame[upper_left], bytes) == 0)
            memcpy(&pDestFrame[target], &pSrcFrame[lower_left], bytes);
          else if (memcmp(&pSrcFrame[upper_right], &pSrcFrame[upper_left], bytes) == 0)
            memcpy(&pDestFrame[target], &pSrcFrame[upper_right], bytes);
          else
            memcpy(&pDestFrame[target], &pSrcFrame[lower_right], bytes);
        }
      }
    }
  }
}

inline void Helper::ScaleUp(uint8_t* pDestFrame, const uint8_t* pSrcFrame, const uint16_t srcWidth,
                            const uint8_t srcHeight, uint8_t bits)
{
  uint8_t bytes = bits / 8;  // RGB24 (3 byte) or RGB16 (2 byte) or indexed (1 byte)
  uint16_t destWidth = srcWidth * 2;
  memset(pDestFrame, 0, srcWidth * srcHeight * 4 * bytes);

  // we implement scale2x http://www.scale2x.it/algorithm
  uint16_t row = srcWidth * bytes;
  uint8_t* a = (uint8_t*)malloc(bytes);
  uint8_t* b = (uint8_t*)malloc(bytes);
  uint8_t* c = (uint8_t*)malloc(bytes);
  uint8_t* d = (uint8_t*)malloc(bytes);
  uint8_t* e = (uint8_t*)malloc(bytes);
  uint8_t* f = (uint8_t*)malloc(bytes);
  uint8_t* g = (uint8_t*)malloc(bytes);
  uint8_t* h = (uint8_t*)malloc(bytes);
  uint8_t* i = (uint8_t*)malloc(bytes);

  for (uint16_t y = 0; y < srcHeight; y++)
  {
    for (uint16_t x = 0; x < srcWidth; x++)
    {
      for (uint8_t tc = 0; tc < bytes; tc++)
      {
        if (x == 0 && y == 0)
        {
          a[tc] = b[tc] = d[tc] = e[tc] = pSrcFrame[tc];
          c[tc] = f[tc] = pSrcFrame[bytes + tc];
          g[tc] = h[tc] = pSrcFrame[row + tc];
          i[tc] = pSrcFrame[row + bytes + tc];
        }
        else if ((x == 0) && (y == srcHeight - 1))
        {
          a[tc] = b[tc] = pSrcFrame[(y - 1) * row + tc];
          c[tc] = pSrcFrame[(y - 1) * row + bytes + tc];
          d[tc] = g[tc] = h[tc] = e[tc] = pSrcFrame[y * row + tc];
          f[tc] = i[tc] = pSrcFrame[y * row + bytes + tc];
        }
        else if ((x == srcWidth - 1) && (y == 0))
        {
          a[tc] = d[tc] = pSrcFrame[x * bytes - bytes + tc];
          b[tc] = c[tc] = f[tc] = e[tc] = pSrcFrame[x * bytes + tc];
          g[tc] = pSrcFrame[row + x * bytes - bytes + tc];
          h[tc] = i[tc] = pSrcFrame[row + x * bytes + tc];
        }
        else if ((x == srcWidth - 1) && (y == srcHeight - 1))
        {
          a[tc] = pSrcFrame[y * row - 2 * bytes + tc];
          b[tc] = c[tc] = pSrcFrame[y * row - bytes + tc];
          d[tc] = g[tc] = pSrcFrame[srcHeight * row - 2 * bytes + tc];
          e[tc] = f[tc] = h[tc] = i[tc] = pSrcFrame[srcHeight * row - bytes + tc];
        }
        else if (x == 0)
        {
          a[tc] = b[tc] = pSrcFrame[(y - 1) * row + tc];
          c[tc] = pSrcFrame[(y - 1) * row + bytes + tc];
          d[tc] = e[tc] = pSrcFrame[y * row + tc];
          f[tc] = pSrcFrame[y * row + bytes + tc];
          g[tc] = h[tc] = pSrcFrame[(y + 1) * row + tc];
          i[tc] = pSrcFrame[(y + 1) * row + bytes + tc];
        }
        else if (x == srcWidth - 1)
        {
          a[tc] = pSrcFrame[y * row - 2 * bytes + tc];
          b[tc] = c[tc] = pSrcFrame[y * row - bytes + tc];
          d[tc] = pSrcFrame[(y + 1) * row - 2 * bytes + tc];
          e[tc] = f[tc] = pSrcFrame[(y + 1) * row - bytes + tc];
          g[tc] = pSrcFrame[(y + 2) * row - 2 * bytes + tc];
          h[tc] = i[tc] = pSrcFrame[(y + 2) * row - bytes + tc];
        }
        else if (y == 0)
        {
          a[tc] = d[tc] = pSrcFrame[x * bytes - bytes + tc];
          b[tc] = e[tc] = pSrcFrame[x * bytes + tc];
          c[tc] = f[tc] = pSrcFrame[x * bytes + bytes + tc];
          g[tc] = pSrcFrame[row + x * bytes - bytes + tc];
          h[tc] = pSrcFrame[row + x * bytes + tc];
          i[tc] = pSrcFrame[row + x * bytes + bytes + tc];
        }
        else if (y == srcHeight - 1)
        {
          a[tc] = pSrcFrame[(y - 1) * row + x * bytes - bytes + tc];
          b[tc] = pSrcFrame[(y - 1) * row + x * bytes + tc];
          c[tc] = pSrcFrame[(y - 1) * row + x * bytes + bytes + tc];
          d[tc] = g[tc] = pSrcFrame[y * row + x * bytes - bytes + tc];
          e[tc] = h[tc] = pSrcFrame[y * row + x * bytes + tc];
          f[tc] = i[tc] = pSrcFrame[y * row + x * bytes + bytes + tc];
        }
        else
        {
          a[tc] = pSrcFrame[(y - 1) * row + x * bytes - bytes + tc];
          b[tc] = pSrcFrame[(y - 1) * row + x * bytes + tc];
          c[tc] = pSrcFrame[(y - 1) * row + x * bytes + bytes + tc];
          d[tc] = pSrcFrame[y * row + x * bytes - bytes + tc];
          e[tc] = pSrcFrame[y * row + x * bytes + tc];
          f[tc] = pSrcFrame[y * row + x * bytes + bytes + tc];
          g[tc] = pSrcFrame[(y + 1) * row + x * bytes - bytes + tc];
          h[tc] = pSrcFrame[(y + 1) * row + x * bytes + tc];
          i[tc] = pSrcFrame[(y + 1) * row + x * bytes + bytes + tc];
        }
      }

      if (memcmp(b, h, bytes) != 0 && memcmp(d, f, bytes) != 0)
      {
        memcpy(&pDestFrame[(y * 2 * destWidth + x * 2) * bytes], memcmp(d, b, bytes) == 0 ? d : e, bytes);
        memcpy(&pDestFrame[(y * 2 * destWidth + x * 2 + 1) * bytes], memcmp(b, f, bytes) == 0 ? f : e, bytes);
        memcpy(&pDestFrame[((y * 2 + 1) * destWidth + x * 2) * bytes], memcmp(d, h, bytes) == 0 ? d : e, bytes);
        memcpy(&pDestFrame[((y * 2 + 1) * destWidth + x * 2 + 1) * bytes], memcmp(h, f, bytes) == 0 ? f : e, bytes);
      }
      else
      {
        memcpy(&pDestFrame[(y * 2 * destWidth + x * 2) * bytes], e, bytes);
        memcpy(&pDestFrame[(y * 2 * destWidth + x * 2 + 1) * bytes], e, bytes);
        memcpy(&pDestFrame[((y * 2 + 1) * destWidth + x * 2) * bytes], e, bytes);
        memcpy(&pDestFrame[((y * 2 + 1) * destWidth + x * 2 + 1) * bytes], e, bytes);
      }
    }
  }

  free(a);
  free(b);
  free(c);
  free(d);
  free(e);
  free(f);
  free(g);
  free(h);
  free(i);
}

inline void Helper::ScaleUpIndexed(uint8_t* pDestFrame, const uint8_t* pSrcFrame, const uint16_t srcWidth,
                                   const uint8_t srcHeight)
{
  ScaleUp(pDestFrame, pSrcFrame, srcWidth, srcHeight, 8);
}

inline void Helper::Center(uint8_t* pDestFrame, const uint16_t destWidth, const uint8_t destHeight,
                           const uint8_t* pSrcFrame, const uint16_t srcWidth, const uint8_t srcHeight, uint8_t bits)
{
  uint8_t bytes = bits / 8;  // RGB24 (3 byte) or RGB16 (2 byte) or indexed (1 byte)

  memset(pDestFrame, 0, destWidth * destHeight * bytes);
  uint8_t xOffset = (destWidth - srcWidth) / 2;
  uint8_t yOffset = (destHeight - srcHeight) / 2;

  for (uint8_t y = 0; y < srcHeight; y++)
  {
    memcpy(&pDestFrame[((yOffset + y) * destWidth + xOffset) * bytes], &pSrcFrame[y * srcWidth * bytes],
           srcWidth * bytes);
  }
}

inline void Helper::CenterIndexed(uint8_t* pDestFrame, const uint16_t destWidth, const uint8_t destHeight,
                                  const uint8_t* pSrcFrame, const uint16_t srcWidth, const uint8_t srcHeight)
{
  Center(pDestFrame, destWidth, destHeight, pSrcFrame, srcWidth, srcHeight, 8);
}

}  // namespace FrameUtil
