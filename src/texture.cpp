#include "texture.h"
#include "CGL/color.h"

#include <algorithm>
#include <cmath>

namespace CGL {

Color Texture::sample(const SampleParams &sp) {
  // TODO: Task 6: Fill this in.

    // if mapping falls outside of texmap set pixel to zero
    if (sp.p_uv.x > 1 || sp.p_uv.x < 0 || sp.p_uv.y > 1 || sp.p_uv.y < 0) {
        return Color(1,1,1);
    }
    // Get mipmap level
    float level = get_level(sp);

    switch (sp.lsm) {
    case L_ZERO:
        // Set level to zero
        level = 0;

        if (sp.psm == P_NEAREST) {
            return sample_nearest(sp.p_uv, level);
        }
        else if (sp.psm == P_LINEAR) {
            return sample_bilinear(sp.p_uv, level);
        }
    break;

    case L_NEAREST:
        // Round to nearest mipmap level
        level = round(level);
        // level boundary checks
        level = (level < 0) ? 0 : level;
        level = (level > mipmap.size()) ? mipmap.size() : level;
        
        if (sp.psm == P_NEAREST) {
            return sample_nearest(sp.p_uv, level);
        }
        else if (sp.psm == P_LINEAR) {
            return sample_bilinear(sp.p_uv, level);
        }
    break;

    case L_LINEAR:
        // find nearest levels to float for interpolation
        int levelA = floor(level);
        int levelB = ceil(level);

        // level boundary checks
        levelA = (levelA < 0) ? 0 : levelA;
        levelA = (levelA > mipmap.size()) ? mipmap.size() : levelA;
        levelB = (levelB < 0) ? 0 : levelB;
        levelB = (levelB > mipmap.size()) ? mipmap.size() : levelB;

        if (sp.psm == P_NEAREST) {
            return (sample_nearest(sp.p_uv, levelA) + sample_nearest(sp.p_uv, levelB))*0.5;
        }
        else if (sp.psm == P_LINEAR) {
            return (sample_bilinear(sp.p_uv, levelA)+ sample_bilinear(sp.p_uv, levelB))*0.5;
        }
    break;   
    }
}

float Texture::get_level(const SampleParams &sp) {
    // TODO: Task 6: Fill this in.
    //Calculate the appropriate mipmap level from the differential elements
    //Rescale normalized uv coordinates to tx,ty coordinates with largest mipmap level dimensions
    Vector2D p_tx_uv = Vector2D(sp.p_dx_uv.x * (width-1), sp.p_dx_uv.y * (height - 1));
    Vector2D p_ty_uv = Vector2D(sp.p_dy_uv.x * (width - 1), sp.p_dy_uv.y * (height - 1));
    float L = std::max(p_tx_uv.norm(), p_ty_uv.norm());
    float level = log2(L);
    return level;
}

Color MipLevel::get_texel(int tx, int ty) {
  return Color(&texels[tx * 3 + ty * width * 3]);
}

Color Texture::sample_nearest(Vector2D uv, int level) {
  // TODO: Task 5: Fill this in.

    int tx = round(uv.x * (mipmap[level].width-1));
    int ty = round(uv.y * (mipmap[level].height-1));
    Color c = mipmap[level].get_texel(tx, ty);
    return c;
}

Color Texture::sample_bilinear(Vector2D uv, int level) {
  // TODO: Task 5: Fill this in.

    // nearest points for bilinear filtering orientation
    //      c2---c3
    //       |   |
    //      c1---c4
        float tx = uv.x * (mipmap[level] .width-1);
        float ty = uv.y * (mipmap[level].height-1);
        float s = tx - floor(tx);
        float t = ty - floor(ty);

        Color c1 = mipmap[level].get_texel(floor(tx), floor(ty));
        Color c2 = mipmap[level].get_texel(floor(tx), ceil(ty));
        Color c3 = mipmap[level].get_texel(ceil(tx), ceil(ty));
        Color c4 = mipmap[level].get_texel(ceil(tx), floor(ty));
            
        // lerp horizontally
        Color h1 = c1 + s * (c4 + (-1 * c1));
        Color h2 = c2 + s * (c3 + (-1 * c2));

        // lerp vertically
        Color v = h1 + t * (h2 + (-1 * h1));
        return v;
}

/****************************************************************************/

// Helpers

inline void uint8_to_float(float dst[3], unsigned char *src) {
  uint8_t *src_uint8 = (uint8_t *)src;
  dst[0] = src_uint8[0] / 255.f;
  dst[1] = src_uint8[1] / 255.f;
  dst[2] = src_uint8[2] / 255.f;
}

inline void float_to_uint8(unsigned char *dst, float src[3]) {
  uint8_t *dst_uint8 = (uint8_t *)dst;
  dst_uint8[0] = (uint8_t)(255.f * max(0.0f, min(1.0f, src[0])));
  dst_uint8[1] = (uint8_t)(255.f * max(0.0f, min(1.0f, src[1])));
  dst_uint8[2] = (uint8_t)(255.f * max(0.0f, min(1.0f, src[2])));
}

void Texture::generate_mips(int startLevel) {

  // make sure there's a valid texture
  if (startLevel >= mipmap.size()) {
    std::cerr << "Invalid start level";
  }

  // allocate sublevels
  int baseWidth = mipmap[startLevel].width;
  int baseHeight = mipmap[startLevel].height;
  int numSubLevels = (int)(log2f((float)max(baseWidth, baseHeight)));

  numSubLevels = min(numSubLevels, kMaxMipLevels - startLevel - 1);
  mipmap.resize(startLevel + numSubLevels + 1);

  int width = baseWidth;
  int height = baseHeight;
  for (int i = 1; i <= numSubLevels; i++) {

    MipLevel &level = mipmap[startLevel + i];

    // handle odd size texture by rounding down
    width = max(1, width / 2);
    // assert (width > 0);
    height = max(1, height / 2);
    // assert (height > 0);

    level.width = width;
    level.height = height;
    level.texels = vector<unsigned char>(3 * width * height);
  }

  // create mips
  int subLevels = numSubLevels - (startLevel + 1);
  for (int mipLevel = startLevel + 1; mipLevel < startLevel + subLevels + 1;
       mipLevel++) {

    MipLevel &prevLevel = mipmap[mipLevel - 1];
    MipLevel &currLevel = mipmap[mipLevel];

    int prevLevelPitch = prevLevel.width * 3; // 32 bit RGB
    int currLevelPitch = currLevel.width * 3; // 32 bit RGB

    unsigned char *prevLevelMem;
    unsigned char *currLevelMem;

    currLevelMem = (unsigned char *)&currLevel.texels[0];
    prevLevelMem = (unsigned char *)&prevLevel.texels[0];

    float wDecimal, wNorm, wWeight[3];
    int wSupport;
    float hDecimal, hNorm, hWeight[3];
    int hSupport;

    float result[3];
    float input[3];

    // conditional differentiates no rounding case from round down case
    if (prevLevel.width & 1) {
      wSupport = 3;
      wDecimal = 1.0f / (float)currLevel.width;
    } else {
      wSupport = 2;
      wDecimal = 0.0f;
    }

    // conditional differentiates no rounding case from round down case
    if (prevLevel.height & 1) {
      hSupport = 3;
      hDecimal = 1.0f / (float)currLevel.height;
    } else {
      hSupport = 2;
      hDecimal = 0.0f;
    }

    wNorm = 1.0f / (2.0f + wDecimal);
    hNorm = 1.0f / (2.0f + hDecimal);

    // case 1: reduction only in horizontal size (vertical size is 1)
    if (currLevel.height == prevLevel.height) {
      // assert (currLevel.height == 1);

      for (int i = 0; i < currLevel.width; i++) {
        wWeight[0] = wNorm * (1.0f - wDecimal * i);
        wWeight[1] = wNorm * 1.0f;
        wWeight[2] = wNorm * wDecimal * (i + 1);

        result[0] = result[1] = result[2] = 0.0f;

        for (int ii = 0; ii < wSupport; ii++) {
          uint8_to_float(input, prevLevelMem + 3 * (2 * i + ii));
          result[0] += wWeight[ii] * input[0];
          result[1] += wWeight[ii] * input[1];
          result[2] += wWeight[ii] * input[2];
        }

        // convert back to format of the texture
        float_to_uint8(currLevelMem + (3 * i), result);
      }

      // case 2: reduction only in vertical size (horizontal size is 1)
    } else if (currLevel.width == prevLevel.width) {
      // assert (currLevel.width == 1);

      for (int j = 0; j < currLevel.height; j++) {
        hWeight[0] = hNorm * (1.0f - hDecimal * j);
        hWeight[1] = hNorm;
        hWeight[2] = hNorm * hDecimal * (j + 1);

        result[0] = result[1] = result[2] = 0.0f;
        for (int jj = 0; jj < hSupport; jj++) {
          uint8_to_float(input, prevLevelMem + prevLevelPitch * (2 * j + jj));
          result[0] += hWeight[jj] * input[0];
          result[1] += hWeight[jj] * input[1];
          result[2] += hWeight[jj] * input[2];
        }

        // convert back to format of the texture
        float_to_uint8(currLevelMem + (currLevelPitch * j), result);
      }

      // case 3: reduction in both horizontal and vertical size
    } else {

      for (int j = 0; j < currLevel.height; j++) {
        hWeight[0] = hNorm * (1.0f - hDecimal * j);
        hWeight[1] = hNorm;
        hWeight[2] = hNorm * hDecimal * (j + 1);

        for (int i = 0; i < currLevel.width; i++) {
          wWeight[0] = wNorm * (1.0f - wDecimal * i);
          wWeight[1] = wNorm * 1.0f;
          wWeight[2] = wNorm * wDecimal * (i + 1);

          result[0] = result[1] = result[2] = 0.0f;

          // convolve source image with a trapezoidal filter.
          // in the case of no rounding this is just a box filter of width 2.
          // in the general case, the support region is 3x3.
          for (int jj = 0; jj < hSupport; jj++)
            for (int ii = 0; ii < wSupport; ii++) {
              float weight = hWeight[jj] * wWeight[ii];
              uint8_to_float(input, prevLevelMem +
                                        prevLevelPitch * (2 * j + jj) +
                                        3 * (2 * i + ii));
              result[0] += weight * input[0];
              result[1] += weight * input[1];
              result[2] += weight * input[2];
            }

          // convert back to format of the texture
          float_to_uint8(currLevelMem + currLevelPitch * j + 3 * i, result);
        }
      }
    }
  }
}

} // namespace CGL
