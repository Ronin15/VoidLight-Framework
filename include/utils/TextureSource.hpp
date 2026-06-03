/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef TEXTURE_SOURCE_HPP
#define TEXTURE_SOURCE_HPP

#include <string>

struct TextureSource
{
    std::string textureId{};
    int sourceX{0};
    int sourceY{0};
    int sourceW{0};
    int sourceH{0};
    bool useSourceRect{false};

    bool isEmpty() const
    {
        return textureId.empty();
    }
};

#endif // TEXTURE_SOURCE_HPP
