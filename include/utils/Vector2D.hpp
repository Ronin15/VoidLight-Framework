/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef VECTOR_2D_HPP
#define VECTOR_2D_HPP

#include <cmath>

// A simple 2D vector class
class Vector2D {
public:
    // Constructors
    Vector2D() : m_x(0.0f), m_y(0.0f) {}
    Vector2D(float x, float y) : m_x(x), m_y(y) {}

    // Getters and setters
    float getX() const { return m_x; }
    float getY() const { return m_y; }
    void setX(float x) { m_x = x; }
    void setY(float y) { m_y = y; }

    // Vector operations (cache-friendly implementations)
    float length() const { return sqrt(lengthSquared()); }
    float lengthSquared() const { return m_x * m_x + m_y * m_y; }

    // Fast normalized vector (avoids sqrt when possible)
    Vector2D normalized() const {
        float const lenSq = lengthSquared();
        if (lenSq < 0.0001f) return Vector2D(1.0f, 0.0f); // Default direction
        float invLen = 1.0f / sqrt(lenSq);
        return Vector2D(m_x * invLen, m_y * invLen);
    }

    float dot(const Vector2D& v2) const {
        return m_x * v2.m_x + m_y * v2.m_y;
    }

    // Operator overloads
    Vector2D operator+(const Vector2D& v2) const {
        return Vector2D(m_x + v2.m_x, m_y + v2.m_y);
    }

    friend Vector2D& operator+=(Vector2D& v1, const Vector2D& v2) {
        v1.m_x += v2.m_x;
        v1.m_y += v2.m_y;
        return v1;
    }

    Vector2D operator*(float scalar) const {
        return Vector2D(m_x * scalar, m_y * scalar);
    }

    Vector2D& operator*=(float scalar) {
        m_x *= scalar;
        m_y *= scalar;
        return *this;
    }

    Vector2D operator-(const Vector2D& v2) const {
        return Vector2D(m_x - v2.m_x, m_y - v2.m_y);
    }

    friend Vector2D& operator-=(Vector2D& v1, const Vector2D& v2) {
        v1.m_x -= v2.m_x;
        v1.m_y -= v2.m_y;
        return v1;
    }

    Vector2D operator/(float scalar) const {
        return Vector2D(m_x / scalar, m_y / scalar);
    }

    Vector2D& operator/=(float scalar) {
        m_x /= scalar;
        m_y /= scalar;
        return *this;
    }

    // Normalize the vector in place
    void normalize() {
        float const l = length();
        if (l > 0) {
            (*this) *= 1 / l;
        }
    }

    // Cache-friendly static utility functions
    static float distanceSquared(const Vector2D& a, const Vector2D& b) {
        float const dx = a.m_x - b.m_x;
        float const dy = a.m_y - b.m_y;
        return dx * dx + dy * dy;
    }

    static float distance(const Vector2D& a, const Vector2D& b) {
        return sqrt(distanceSquared(a, b));
    }

    // Return a normalized copy of the vector (keeping existing implementation for compatibility)
    Vector2D normalizedLegacy() const {
        Vector2D v = *this;
        float const l = length();
        if (l > 0) {
            v *= 1 / l;
        }
        return v;
    }

private:
    float m_x{0.0f};
    float m_y{0.0f};
};

#endif // VECTOR_2D_HPP
