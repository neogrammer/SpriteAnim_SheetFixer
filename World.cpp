// World.cpp
#include "World.h"

#include <algorithm>
#include <string>
#include <utility>

namespace
{
    // The factories are the only place that knows what kind of actor
    // something is: same Actor class, different sheet, facts and states.

    Actor makePlayer(const SpriteSheet& sheet, sf::Vector2f at)
    {
        Actor actor(sheet, "player");
        actor.position = at;
        actor.health = 100;
        actor.damage = 20;
        actor.rest = Player{};
        actor.behavior().start<Player>(actor);
        return actor;
    }

    Actor makeEnemy(const SpriteSheet& sheet, std::string name, sf::Vector2f at,
        float homeLeft, float homeRight)
    {
        Actor actor(sheet, std::move(name));
        actor.position = at;
        actor.homeLeft = homeLeft;
        actor.homeRight = homeRight;
        actor.health = 60;
        actor.damage = 10;
        actor.rest = Idle{};
        actor.behavior().start<Patrol>(actor);
        return actor;
    }
}

World::World(const SpriteSheet& playerSheet, const SpriteSheet& enemySheet, sf::Vector2f size)
    : m_size(size)
    , m_ground(size.y - 24.f)
    , m_player(makePlayer(playerSheet, { size.x * 0.25f, m_ground }))
{
    m_enemies.push_back(makeEnemy(enemySheet, "enemy 1",
        { size.x * 0.70f, m_ground }, size.x * 0.55f, size.x * 0.92f));
}

void World::readInput()
{
    using Key = sf::Keyboard::Key;

    Input& in = m_player.input;

    in.left  = sf::Keyboard::isKeyPressed(Key::Left)  || sf::Keyboard::isKeyPressed(Key::A);
    in.right = sf::Keyboard::isKeyPressed(Key::Right) || sf::Keyboard::isKeyPressed(Key::D);
    in.guard = sf::Keyboard::isKeyPressed(Key::LShift) || sf::Keyboard::isKeyPressed(Key::S);

    const bool attackDown =
        sf::Keyboard::isKeyPressed(Key::Space) || sf::Keyboard::isKeyPressed(Key::J);

    in.attack = attackDown && !m_attackWasDown;   // edge: one swing per press
    m_attackWasDown = attackDown;
}

void World::update(float dt)
{
    for (Actor& enemy : m_enemies)
        enemy.target = &m_player;

    m_player.update(dt);

    for (Actor& enemy : m_enemies)
        enemy.update(dt);

    // Keep everyone on screen and on the ground.
    m_player.position.x = std::clamp(m_player.position.x, 8.f, m_size.x - 8.f);
    m_player.position.y = m_ground;

    for (Actor& enemy : m_enemies)
    {
        enemy.position.x = std::clamp(enemy.position.x, 8.f, m_size.x - 8.f);
        enemy.position.y = m_ground;
    }

    resolveHits();
}

// A hit lands when an active hit box overlaps a hurt box. The victim's
// current state decides what that means (flinch, block, ignore).
void World::resolveHits()
{
    auto deliver = [](Actor& attacker, Actor& victim)
    {
        if (!attacker.alive() || !victim.alive())
            return;

        const auto hit = attacker.hitbox();

        if (!hit || !hit->findIntersection(victim.hurtbox()))
            return;

        if (!attacker.strike(victim))
            return;

        victim.receive(HitEvent{ .damage = attacker.damage, .fromX = attacker.position.x });
    };

    for (Actor& enemy : m_enemies)
    {
        deliver(m_player, enemy);
        deliver(enemy, m_player);
    }
}

void World::draw(sf::RenderTarget& target, bool debugBoxes) const
{
    sf::RectangleShape floor({ m_size.x, m_size.y - m_ground });
    floor.setPosition({ 0.f, m_ground });
    floor.setFillColor(sf::Color(60, 60, 80));
    target.draw(floor);

    for (const Actor& enemy : m_enemies)
        enemy.draw(target);

    m_player.draw(target);

    if (debugBoxes)
    {
        for (const Actor& enemy : m_enemies)
            enemy.drawDebug(target);

        m_player.drawDebug(target);
    }
}
