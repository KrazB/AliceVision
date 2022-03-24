// This file is part of the AliceVision project.
// Copyright (c) 2022 AliceVision contributors.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "TileParams.hpp"

namespace aliceVision {
namespace mvsUtils {

void getTileDimensions(const TileParams& tileParams, int maxWidth, int maxHeight, int& tileWidth, int& tileHeight) 
{
    tileWidth  = (tileParams.width  > 0) ? tileParams.width  : (tileParams.height > 0) ? tileParams.height : maxWidth;
    tileHeight = (tileParams.height > 0) ? tileParams.height : (tileParams.width > 0)  ? tileParams.width  : maxHeight;

    if(tileParams.padding >= std::min(tileWidth, tileHeight))
      ALICEVISION_THROW_ERROR("Unable to compute tile dimensions, tile padding size is too large.");
}

void getTileRoiList(const TileParams& tileParams, int originalWidth, int originalHeight, std::vector<ROI>& out_tileList)
{
    int tileWidth;
    int tileHeight;

    // compute tile dimensions
    getTileDimensions(tileParams, originalWidth, originalHeight, tileWidth, tileHeight);

    // compute nb of tiles per side
    const int nbTileSideX = 1 + int(std::ceil(float(originalWidth  - tileWidth)  / float(tileWidth  - tileParams.padding)));
    const int nbTileSideY = 1 + int(std::ceil(float(originalHeight - tileHeight) / float(tileHeight - tileParams.padding)));

    out_tileList.resize(nbTileSideX * nbTileSideY);

    // compute each tile 2d region of interest
    for(int i = 0; i < nbTileSideX; ++i)
    {
        const int startX = i * (tileWidth - tileParams.padding);
        const int endX = std::min(startX + tileWidth, originalWidth);

        for(int j = 0; j < nbTileSideY; ++j)
        {
            const int startY = j * (tileHeight - tileParams.padding);
            const int endY = std::min(startY + tileHeight, originalHeight);

            out_tileList.at(i * nbTileSideY + j) = ROI(startX, endX, startY, endY);
        }
    }
}

} // namespace mvsUtils
} // namespace aliceVision
