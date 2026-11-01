// Actor.h
// One animated thing in the world. Generic: it does not know what an
// "Alucard" is. A sheet gives it its looks, a rest state and a starting
// state give it its behavior (see the factories in World.cpp).
#pragma once

#include "Behavior.h"
#include "SpriteSheet.h"
#include "States.h"

#include <SFML/Graphics.hpp>

#include <optional>
#include <string>
#include <vector>

class Actor
{
public:
    // The sheet must outlive the actor (keep sheets in a SpriteLibrary).
    explicit Actor(const SpriteSheet& sheet, std::string name = "actor");

    // Facts the states read and write. Anything that must survive a state
    // change lives here, never inside a state.
    std::string name;                // for the console log
    sf::Vector2f position;           // the anchor: bottom centre of the sprite (the feet)
    sf::Vector2f velocity;           // applied by update()
    float homeLeft = 0.f;            // Patrol bounds
    float homeRight = 0.f;
    int health = 100;
    int damage = 10;
    const Actor* target = nullptr;   // set by the world every step; may be null
    Input input;                     // set by the world every step (player only)
    States rest = Idle{};            // where one-shot states (Attack, TakeHit, Defend) return to

    bool facingLeft() const { return m_facingLeft; }
    void face(bool left) { m_facingLeft = left; }
    bool alive() const { return health > 0; }

    // Always restarts the animation, even when it is already playing. States
    // only call these on transitions, which is what makes that safe. A name
    // the sheet does not have plays the sheet's first animation and logs once.
    void play(const std::string& animation);              // loop flag from the .cfg
    void play(const std::string& animation, bool loop);
    bool playing(const std::string& animation) const { return m_animationName == animation; }
    bool animationFinished() const { return m_animator.finished(); }

    // For the world's hit pass. World coordinates, mirrored when facing left.
    sf::FloatRect hurtbox() const;
    std::optional<sf::FloatRect> hitbox() const;   // only on frames that have a hit line
    bool strike(const Actor& victim);              // true the first time this swing reaches victim
    void receive(const HitEvent& hit) { m_behavior.onHit(*this, hit); }

    Behavior<States>& behavior() { return m_behavior; }
    template <typename S> bool is() const { return m_behavior.is<S>(); }
    const char* stateName() const { return m_behavior.name(); }

    void update(float dt);
    void draw(sf::RenderTarget& target) const;
    void drawDebug(sf::RenderTarget& target) const;   // hurt box in blue, hit box in red

private:
    bool flipped() const { return m_facingLeft != m_sheet->facesLeft; }
    sf::FloatRect toWorld(const sf::IntRect& box) const;
    void syncSprite();

    const SpriteSheet* m_sheet;
    sf::Sprite m_sprite;
    Animator m_animator;
    Behavior<States> m_behavior;
    std::string m_animationName;
    const char* m_lastStateName = nullptr;
    bool m_facingLeft = false;
    std::vector<const Actor*> m_struck;   // victims this swing has already reached
};
