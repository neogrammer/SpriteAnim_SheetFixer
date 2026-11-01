// SpriteSheet.h
// Loads a .cfg written by the Sprite Sheet Processor, owns the texture, and
// hands out animation data. Also holds Animator (plays one animation) and
// SpriteLibrary (keeps every loaded sheet at a stable address).
//
// File format, one animation per row of the image:
//
//   image <file next to this config>
//   facing left | right              which way the artwork faces (default right)
//   anim <name> <frames> <frameWidth> <frameHeight> <rowY> <secondsPerFrame>
//     loop | once                    repeat (default) or stop on the last frame
//     next <anim>                    animation to play after a "once" animation ends
//     hurt <x> <y> <w> <h>           body box, cell coordinates, one per animation
//     hit <i> <x> <y> <w> <h>        damage box on frame i, cell coordinates
//
// Lines after an "anim" line belong to that animation. Frame i of an
// animation is at x = i * frameWidth, y = rowY. Boxes are converted to
// anchor-relative coordinates on load: the anchor is the bottom centre of
// the cell, so a box's y is negative above the feet. Facing is handled by
// mirroring around that anchor, so no flipped frames are stored anywhere.
#pragma once

#include <SFML/Graphics.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

struct AnimationData
{
    std::string name;
    std::vector<sf::IntRect> frames;        // texture rects, one per frame
    float secondsPerFrame = 0.1f;
    bool loop = true;
    std::string next;                       // empty: stop on the last frame

    // Anchor-relative boxes (see the header comment).
    sf::IntRect hurt;                               // the body; default is the whole cell
    std::vector<std::optional<sf::IntRect>> hits;   // one slot per frame; empty slot = no hit box
};

// Cell coordinates (0,0 = top left of the cell) to anchor-relative.
inline sf::IntRect toAnchorRelative(sf::IntRect box, int cellWidth, int cellHeight)
{
    box.position.x -= cellWidth / 2;
    box.position.y -= cellHeight;
    return box;
}

class SpriteSheet
{
public:
    sf::Texture texture;
    std::filesystem::path imagePath;
    bool facesLeft = false;

    // std::map: nodes never move, so pointers into it stay valid.
    std::map<std::string, AnimationData> animations;

    bool load(const std::filesystem::path& configPath);

    bool has(const std::string& name) const { return animations.count(name) != 0; }

    const AnimationData* find(const std::string& name) const
    {
        const auto it = animations.find(name);
        return it == animations.end() ? nullptr : &it->second;
    }

    // Throws std::out_of_range if the name is not in the file.
    const AnimationData& get(const std::string& name) const { return animations.at(name); }

    // Only valid after a successful load (load fails on a sheet with no animations).
    const AnimationData& first() const { return animations.begin()->second; }
};

inline bool SpriteSheet::load(const std::filesystem::path& configPath)
{
    std::ifstream in(configPath);

    if (!in)
    {
        std::cerr << "Could not open " << configPath << '\n';
        return false;
    }

    animations.clear();
    imagePath.clear();
    facesLeft = false;

    AnimationData* current = nullptr;   // the animation the following lines belong to
    int cellWidth = 0;
    int cellHeight = 0;

    std::string line;
    int lineNumber = 0;

    auto fail = [&](const char* what)
    {
        std::cerr << configPath << " line " << lineNumber << ": " << what << '\n';
        return false;
    };

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

            if (!texture.loadFromFile(imagePath))
            {
                std::cerr << "Could not load " << imagePath << '\n';
                return false;
            }
        }
        else if (keyword == "facing")
        {
            std::string side;
            words >> side;
            facesLeft = side == "left";
        }
        else if (keyword == "anim")
        {
            AnimationData data;
            int count = 0;
            int rowY = 0;

            if (!(words >> data.name >> count >> cellWidth >> cellHeight >> rowY))
                return fail("expected anim <name> <frames> <w> <h> <rowY> [secondsPerFrame]");

            words >> data.secondsPerFrame;   // optional

            for (int i = 0; i < count; ++i)
                data.frames.push_back({ { i * cellWidth, rowY }, { cellWidth, cellHeight } });

            data.hits.assign(data.frames.size(), std::nullopt);
            data.hurt = toAnchorRelative({ { 0, 0 }, { cellWidth, cellHeight } }, cellWidth, cellHeight);

            const std::string name = data.name;
            current = &animations[name];
            *current = std::move(data);
        }
        else if (!current)
        {
            return fail("this line has to come after an anim line");
        }
        else if (keyword == "loop")
        {
            current->loop = true;
        }
        else if (keyword == "once")
        {
            current->loop = false;
        }
        else if (keyword == "next")
        {
            if (!(words >> current->next))
                return fail("expected next <animation>");
        }
        else if (keyword == "hurt")
        {
            int x = 0, y = 0, w = 0, h = 0;

            if (!(words >> x >> y >> w >> h))
                return fail("expected hurt <x> <y> <w> <h>");

            current->hurt = toAnchorRelative({ { x, y }, { w, h } }, cellWidth, cellHeight);
        }
        else if (keyword == "hit")
        {
            int frame = 0, x = 0, y = 0, w = 0, h = 0;

            if (!(words >> frame >> x >> y >> w >> h))
                return fail("expected hit <frame> <x> <y> <w> <h>");

            if (frame < 0 || frame >= static_cast<int>(current->hits.size()))
                return fail("hit frame number is out of range for this animation");

            current->hits[static_cast<std::size_t>(frame)] =
                toAnchorRelative({ { x, y }, { w, h } }, cellWidth, cellHeight);
        }
        else
        {
            std::cerr << configPath << " line " << lineNumber << ": unknown keyword '"
                << keyword << "' ignored\n";
        }
    }

    if (imagePath.empty())
    {
        std::cerr << configPath << " has no image line\n";
        return false;
    }

    if (animations.empty())
    {
        std::cerr << configPath << " has no animations\n";
        return false;
    }

    for (const auto& [name, data] : animations)
    {
        if (!data.next.empty() && !has(data.next))
            std::cerr << configPath << ": animation '" << name << "' names next '"
                << data.next << "', which does not exist\n";
    }

    return true;
}

// Plays one animation. Call update() every step, then use frame().
class Animator
{
public:
    // Starts data from its first frame, even when it is already playing.
    void play(const AnimationData& data, bool loop)
    {
        m_data = &data;
        m_loop = loop;
        m_index = 0;
        m_time = 0.f;
        m_done = false;
    }

    void play(const AnimationData& data) { play(data, data.loop); }

    void update(float dt)
    {
        if (!m_data || m_data->frames.empty() || m_data->secondsPerFrame <= 0.f || m_done)
            return;

        m_time += dt;

        while (m_time >= m_data->secondsPerFrame)
        {
            m_time -= m_data->secondsPerFrame;

            if (m_index + 1 < m_data->frames.size())
            {
                ++m_index;
            }
            else if (m_loop)
            {
                m_index = 0;
            }
            else
            {
                // The last frame has been shown for its full duration.
                m_done = true;
                m_time = 0.f;
                break;
            }
        }
    }

    bool playing() const { return m_data != nullptr; }

    // Only valid when playing(). Actor guarantees that by playing something in its constructor.
    const AnimationData& data() const { return *m_data; }

    std::size_t index() const { return m_index; }

    sf::IntRect frame() const
    {
        return m_data && !m_data->frames.empty() ? m_data->frames[m_index] : sf::IntRect{};
    }

    // True once a non-looping animation has shown its last frame for the full frame time.
    bool finished() const { return m_done; }

private:
    const AnimationData* m_data = nullptr;
    std::size_t m_index = 0;
    float m_time = 0.f;
    bool m_loop = true;
    bool m_done = false;
};

// Owns every loaded sheet. Sheets never move once loaded, so the pointers
// Actor and Animator keep into them stay valid for the life of the library.
class SpriteLibrary
{
public:
    // Loads a .cfg and keys it by the file's stem: "assets/alucard.cfg" -> "alucard".
    // Returns nullptr when loading fails.
    const SpriteSheet* load(const std::filesystem::path& configPath)
    {
        const std::string key = configPath.stem().string();
        SpriteSheet& sheet = m_sheets[key];

        if (!sheet.load(configPath))
        {
            m_sheets.erase(key);
            return nullptr;
        }

        return &sheet;
    }

    const SpriteSheet* find(const std::string& key) const
    {
        const auto it = m_sheets.find(key);
        return it == m_sheets.end() ? nullptr : &it->second;
    }

    // Throws std::out_of_range if the key was never loaded.
    const SpriteSheet& get(const std::string& key) const { return m_sheets.at(key); }

private:
    std::map<std::string, SpriteSheet> m_sheets;
};
