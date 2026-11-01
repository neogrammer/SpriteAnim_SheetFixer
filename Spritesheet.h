// SpriteSheet.hpp
// Loads sprite_sheet.cfg (written by the Sprite Sheet Processor) and gives
// you every animation's frame rectangles, ready for sf::Sprite::setTextureRect.
//
// Usage:
//     SpriteSheet sheet;
//     if (!sheet.load("assets/sprite_sheet.cfg")) { ... }
//     sf::Texture tex{ sheet.imagePath };
//     sf::Sprite spr{ tex };
//
//     Animator anim{ sheet.get("walk") };
//     // each frame:
//     anim.update(dt);
//     spr.setTextureRect(anim.frame());
#pragma once

#include <SFML/Graphics.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct AnimationData
{
    std::string name;
    std::vector<sf::IntRect> frames;
    float secondsPerFrame = 0.1f;
};

struct SpriteSheet
{
    // Full path to the image, resolved relative to the .cfg file.
    std::filesystem::path imagePath;

    std::map<std::string, AnimationData> animations;

    bool load(const std::filesystem::path& configPath)
    {
        std::ifstream in(configPath);

        if (!in)
        {
            std::cerr << "Could not open " << configPath << '\n';
            return false;
        }

        animations.clear();
        imagePath.clear();

        std::string line;
        int lineNumber = 0;

        while (std::getline(in, line))
        {
            ++lineNumber;

            std::istringstream words(line);
            std::string keyword;

            if (!(words >> keyword) || keyword[0] == '#')
                continue;

            if (keyword == "image")
            {
                std::string file;
                std::getline(words >> std::ws, file);
                imagePath = configPath.parent_path() / file;
            }
            else if (keyword == "anim")
            {
                AnimationData data;
                int count = 0, width = 0, height = 0, rowY = 0;

                if (!(words >> data.name >> count >> width >> height >> rowY))
                {
                    std::cerr << configPath << " line " << lineNumber
                        << ": expected anim <name> <frames> <w> <h> <rowY>\n";
                    return false;
                }

                // The frame time is optional.
                words >> data.secondsPerFrame;

                for (int i = 0; i < count; ++i)
                    data.frames.push_back(sf::IntRect{ { i * width, rowY }, { width, height } });

                animations[data.name] = std::move(data);
            }
        }

        if (imagePath.empty())
        {
            std::cerr << configPath << " has no image line\n";
            return false;
        }

        return true;
    }

    bool has(const std::string& name) const
    {
        return animations.count(name) != 0;
    }

    // Throws std::out_of_range if the name is not in the file.
    const AnimationData& get(const std::string& name) const
    {
        return animations.at(name);
    }
};

// Plays one animation: call update() every frame, then use frame().
class Animator
{
public:
    Animator() = default;
    explicit Animator(const AnimationData& data, bool loop = true)
    {
        play(data, loop);
    }

    // Switches animation. Calling it with the one already playing does not
    // restart it, so it is safe to call every frame.
    void play(const AnimationData& data, bool loop = true)
    {
        if (m_data == &data)
            return;

        m_data = &data;
        m_loop = loop;
        m_index = 0;
        m_time = 0.f;
    }

    void update(float dt)
    {
        if (!m_data || m_data->frames.empty() || m_data->secondsPerFrame <= 0.f)
            return;

        m_time += dt;

        while (m_time >= m_data->secondsPerFrame)
        {
            m_time -= m_data->secondsPerFrame;

            if (m_index + 1 < m_data->frames.size())
                ++m_index;
            else if (m_loop)
                m_index = 0;
        }
    }

    sf::IntRect frame() const
    {
        return m_data && !m_data->frames.empty() ? m_data->frames[m_index] : sf::IntRect{};
    }

    bool finished() const
    {
        return m_data && !m_loop && m_index + 1 == m_data->frames.size();
    }

private:
    const AnimationData* m_data = nullptr;
    std::size_t m_index = 0;
    float m_time = 0.f;
    bool m_loop = true;
};