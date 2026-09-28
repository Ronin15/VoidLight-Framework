/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef ENTITY_STATE_HPP
#define ENTITY_STATE_HPP

class EntityState {
public:
    virtual void enter() = 0;
    virtual void update(float deltaTime) = 0;
    virtual void exit() = 0;
    virtual ~EntityState() = default;
};

#endif // ENTITY_STATE_HPP
