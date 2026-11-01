// World.h
// Owns the actors and runs the loop around them: feed input, set targets,
// update everyone, resolve hits, draw. Everything else is inside Actor.
#pragma once

#include "Actor.h"
#include "SpriteSheet.h"

#include <SFML/Graphics.hpp>

#include <vector>

class World
{
public:
    World(const SpriteSheet& playerSheet, const SpriteSheet& enemySheet, sf::Vector2f size);

    void readInput();                                       // keyboard into the player's Input
    void update(float dt);
    void draw(sf::RenderTarget& target, bool debugBoxes) const;

    const Actor& player() const { return m_player; }

private:
    void resolveHits();

    sf::Vector2f m_size;
    float m_ground;
    Actor m_player;
    std::vector<Actor> m_enemies;   // only changed between steps, so pointers to actors stay valid
    bool m_attackWasDown = false;
};
