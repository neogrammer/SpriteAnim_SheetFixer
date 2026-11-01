// Actor.cpp
#include "Actor.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <utility>

Actor::Actor(const SpriteSheet& sheet, std::string name_)
    : name(std::move(name_))
    , m_sheet(&sheet)
    , m_sprite(sheet.texture)
{
    // Something is always playing, so Animator::data() is always valid.
    play(sheet.has("idle") ? "idle" : sheet.first().name);
}

void Actor::play(const std::string& animation)
{
    const AnimationData* data = m_sheet->find(animation);
    play(animation, data ? data->loop : false);
}

void Actor::play(const std::string& animation, bool loop)
{
    const AnimationData* data = m_sheet->find(animation);

    if (!data)
    {
        static std::set<std::string> warned;

        if (warned.insert(m_sheet->imagePath.string() + ':' + animation).second)
        {
            std::cerr << m_sheet->imagePath.filename() << " has no animation '" << animation
                << "'; add it to the .cfg or rename one. Playing '"
                << m_sheet->first().name << "' instead.\n";
        }

        data = &m_sheet->first();
    }

    m_animationName = animation;
    m_animator.play(*data, loop);
    m_struck.clear();   // a new animation is a new swing
    syncSprite();
}

// Boxes are stored relative to the anchor (bottom centre of the cell), so a
// facing flip is a reflection in x and no flipped copies exist anywhere.
sf::FloatRect Actor::toWorld(const sf::IntRect& box) const
{
    const float left = flipped()
        ? -static_cast<float>(box.position.x + box.size.x)
        : static_cast<float>(box.position.x);

    return { position + sf::Vector2f{ left, static_cast<float>(box.position.y) },
             sf::Vector2f{ static_cast<float>(box.size.x), static_cast<float>(box.size.y) } };
}

sf::FloatRect Actor::hurtbox() const
{
    return toWorld(m_animator.data().hurt);
}

std::optional<sf::FloatRect> Actor::hitbox() const
{
    const auto& hits = m_animator.data().hits;
    const std::size_t i = m_animator.index();

    if (i >= hits.size() || !hits[i])
        return std::nullopt;

    return toWorld(*hits[i]);
}

bool Actor::strike(const Actor& victim)
{
    if (std::find(m_struck.begin(), m_struck.end(), &victim) != m_struck.end())
        return false;

    m_struck.push_back(&victim);
    return true;
}

void Actor::update(float dt)
{
    // A state gets one update in which it can see a finished animation
    // before the sheet's "next" chain is allowed to replace it.
    const bool wasFinished = m_animator.finished();

    m_behavior.update(*this, dt);

    if (wasFinished && m_animator.finished() && !m_animator.data().next.empty())
        play(m_animator.data().next);

    position += velocity * dt;
    m_animator.update(dt);
    syncSprite();

    if (const char* state = stateName(); state != m_lastStateName)
    {
        std::cout << name << ": " << state << '\n';
        m_lastStateName = state;
    }
}

void Actor::syncSprite()
{
    const sf::IntRect rect = m_animator.frame();

    m_sprite.setTextureRect(rect);
    m_sprite.setOrigin({ static_cast<float>(rect.size.x / 2), static_cast<float>(rect.size.y) });
    m_sprite.setScale({ flipped() ? -1.f : 1.f, 1.f });
    m_sprite.setPosition(position);
}

void Actor::draw(sf::RenderTarget& target) const
{
    target.draw(m_sprite);
}

void Actor::drawDebug(sf::RenderTarget& target) const
{
    auto outline = [&](const sf::FloatRect& box, sf::Color color)
    {
        sf::RectangleShape shape(box.size);
        shape.setPosition(box.position);
        shape.setFillColor(sf::Color::Transparent);
        shape.setOutlineColor(color);
        shape.setOutlineThickness(0.5f);
        target.draw(shape);
    };

    outline(hurtbox(), sf::Color(0, 200, 255));

    if (const auto hit = hitbox())
        outline(*hit, sf::Color(255, 60, 60));
}
