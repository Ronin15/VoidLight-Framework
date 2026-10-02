/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "MockPlayer.hpp"
#include <sstream>

// Implementation using BinarySerializer macros for clean, maintainable code
BEGIN_SERIALIZE(MockPlayer)
// Create a temporary stream for using BinarySerial::Writer
std::ostringstream tempStream(std::ios::binary);
BinarySerial::Writer writer(tempStream);

// Serialize position using Vector2D specialization
Vector2D pos = getPosition();
SERIALIZE_SERIALIZABLE(writer, pos);

// Serialize velocity using Vector2D specialization
Vector2D vel = getVelocity();
SERIALIZE_SERIALIZABLE(writer, vel);

// Serialize textureID string
const std::string& textureID = getTextureID();
SERIALIZE_STRING(writer, textureID);

// Serialize currentStateName string
SERIALIZE_STRING(writer, m_currentStateName);

// Write the binary data to the original stream
std::string binaryData = tempStream.str();
stream.write(binaryData.data(), binaryData.size());
END_SERIALIZE()

BEGIN_DESERIALIZE(MockPlayer)
// Read all data from stream into a string
std::string binaryData((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

// Create a stringstream from the binary data
std::istringstream tempStream(binaryData, std::ios::binary);
BinarySerial::Reader reader(tempStream);

// Deserialize position using Vector2D specialization
Vector2D pos;
DESERIALIZE_SERIALIZABLE(reader, pos);
setPosition(pos);

// Deserialize velocity using Vector2D specialization
Vector2D vel;
DESERIALIZE_SERIALIZABLE(reader, vel);
setVelocity(vel);

// Deserialize textureID string
std::string textureID;
DESERIALIZE_STRING(reader, textureID);
setTextureID(textureID);

// Deserialize currentStateName string
DESERIALIZE_STRING(reader, m_currentStateName);
END_DESERIALIZE()
