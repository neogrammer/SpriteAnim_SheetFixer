// Game.cpp
#include "Game.h"

#include "SpriteSheet.h"
#include "World.h"

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <iostream>
#include <optional>

int runGame(const std::filesystem::path& playerConfig, const std::filesystem::path& enemyConfig)
{
    SpriteLibrary library;

    const SpriteSheet* playerSheet = library.load(playerConfig);
    const SpriteSheet* enemySheet =
        playerConfig == enemyConfig ? playerSheet : library.load(enemyConfig);

    if (!playerSheet || !enemySheet)
        return 1;

    // The world is 400x225 pixels drawn 4x into a 1600x900 window, so the
    // sprites keep their pixel-art size relationships.
    const sf::Vector2f worldSize{ 400.f, 225.f };

    sf::RenderWindow window(sf::VideoMode({ 1600u, 900u }), "Sprites");
    window.setView(sf::View(sf::FloatRect({ 0.f, 0.f }, worldSize)));

    World world(*playerSheet, *enemySheet, worldSize);

    std::cout << "\nLeft/Right or A/D: walk   Space or J: attack   Shift or S: guard\n"
        "F1: show hurt (blue) and hit (red) boxes   Escape: quit\n\n";

    constexpr float step = 1.f / 60.f;
    sf::Clock clock;
    float accumulator = 0.f;
    bool debugBoxes = false;

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->scancode == sf::Keyboard::Scancode::Escape)
                    window.close();
                else if (key->scancode == sf::Keyboard::Scancode::F1)
                    debugBoxes = !debugBoxes;
            }
        }

        // Fixed timestep: the world always advances in whole 1/60 s steps,
        // however long the frame took. The cap stops a long stall (a window
        // drag, a breakpoint) from being replayed as a burst of steps.
        accumulator += clock.restart().asSeconds();
        accumulator = std::min(accumulator, 0.25f);

        while (accumulator >= step)
        {
            world.readInput();
            world.update(step);
            accumulator -= step;
        }

        window.clear(sf::Color(24, 24, 32));
        world.draw(window, debugBoxes);
        window.display();
    }

    return 0;
}
