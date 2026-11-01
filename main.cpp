#include "Game.h"

#include <SFML/Graphics.hpp>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Tuning
// ---------------------------------------------------------------------------

// A pixel belongs to a sprite when its alpha is at least this.
constexpr std::uint8_t kAlphaThreshold = 1;

// Regions with fewer pixels than this (after merging) are ignored as noise.
constexpr int kMinSpritePixels = 2;

// A region with at most this many pixels is a fragment (a spark, a detached
// particle, a glint) and is merged into any neighbour that lies within
// kFragmentGapX / kFragmentGapY pixels of it. Larger regions are only merged
// when one lies completely inside the other's bounds.
constexpr int kFragmentMaxPixels = 64;
constexpr int kFragmentGapX = 6;
constexpr int kFragmentGapY = 8;

// A sprite joins a row when at least this percentage of its height overlaps
// the row's band.
constexpr int kRowOverlapPercent = 50;

// Sprites taller than this many times the median sprite height are treated
// as artwork (portraits, posters) and put on a row of their own.
constexpr int kOversizedFactor = 2;

// Frame duration written to the .cfg for every animation. Edit the numbers
// in the .cfg afterwards to give animations their own speeds.
constexpr float kDefaultSecondsPerFrame = 0.1f;

// On attack-like animations, a frame whose artwork reaches at least this
// many pixels past the body box gets a hit box for the part that sticks out.
constexpr int kHitProtrusion = 4;

// Animations whose name contains one of these get hit boxes and play once.
const char* const kAttackWords[] = {
    "attack", "slash", "swing", "punch", "kick", "stab", "strike", "shoot"
};

// Animations whose name contains one of these play once (no hit boxes).
const char* const kOnceWords[] = {
    "hurt", "hit", "flinch", "damage", "dead", "die", "death", "jump", "land"
};

// ---------------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------------

struct Box
{
    // Inclusive pixel bounds.
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;

    int width() const { return right - left + 1; }
    int height() const { return bottom - top + 1; }

    bool contains(const Box& other) const
    {
        return other.left >= left && other.right <= right &&
            other.top >= top && other.bottom <= bottom;
    }

    bool overlaps(const Box& other, int gapX, int gapY) const
    {
        return !(right + gapX < other.left || other.right + gapX < left ||
            bottom + gapY < other.top || other.bottom + gapY < top);
    }

    void include(const Box& other)
    {
        left = std::min(left, other.left);
        top = std::min(top, other.top);
        right = std::max(right, other.right);
        bottom = std::max(bottom, other.bottom);
    }
};

struct Sprite
{
    Box box;
    int pixelCount = 0;

    // Labels of the pixel regions that make up this sprite.
    std::vector<int> regions;
};

struct Row
{
    int top = 0;
    int bottom = 0;
    bool oversized = false;
    std::vector<Sprite> sprites;
};

struct Animation
{
    std::string name;
    std::vector<Sprite> frames;

    // Size of one cell in the output sheet.
    unsigned int frameWidth = 0;
    unsigned int frameHeight = 0;

    // Bottom edge the frames are lined up on, in original sheet coordinates.
    int baseline = 0;

    // Collision boxes in cell coordinates (see measureHurtBox / measureHitBoxes).
    Box hurt;
    std::vector<std::optional<Box>> hits;   // one slot per frame
    bool once = false;
};

// Which region each pixel of the original belongs to (-1 = transparent).
struct RegionMap
{
    int width = 0;
    int height = 0;
    std::vector<int> labels;

    int at(int x, int y) const
    {
        return labels[static_cast<std::size_t>(y) * width + x];
    }

    int& at(int x, int y)
    {
        return labels[static_cast<std::size_t>(y) * width + x];
    }
};

// ---------------------------------------------------------------------------
// Input helpers
// ---------------------------------------------------------------------------

void exitIfInputClosed()
{
    if (!std::cin.eof())
        return;

    std::cout << "\nInput closed. Exiting.\n";
    std::exit(1);
}

std::string askString(const std::string& prompt)
{
    std::string result;

    std::cout << prompt;
    std::getline(std::cin >> std::ws, result);

    exitIfInputClosed();

    return result;
}

std::string toLower(std::string text)
{
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    return text;
}

bool nameContainsAny(const std::string& name, const char* const* words, std::size_t count)
{
    const std::string lower = toLower(name);

    for (std::size_t i = 0; i < count; ++i)
    {
        if (lower.find(words[i]) != std::string::npos)
            return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Sprite detection
// ---------------------------------------------------------------------------

// Labels every 8-connected region of visible pixels. Touching frames that do
// not share a pixel stay separate even when their columns overlap.
std::vector<Sprite> findRegions(const sf::Image& image, RegionMap& map)
{
    const int width = static_cast<int>(image.getSize().x);
    const int height = static_cast<int>(image.getSize().y);

    map.width = width;
    map.height = height;
    map.labels.assign(static_cast<std::size_t>(width) * height, -1);

    std::vector<std::uint8_t> visible(map.labels.size(), 0);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const auto pixel = image.getPixel({
                static_cast<unsigned int>(x),
                static_cast<unsigned int>(y) });

            visible[static_cast<std::size_t>(y) * width + x] =
                pixel.a >= kAlphaThreshold ? 1 : 0;
        }
    }

    std::vector<Sprite> sprites;
    std::vector<std::pair<int, int>> stack;

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            if (!visible[static_cast<std::size_t>(y) * width + x] ||
                map.at(x, y) != -1)
                continue;

            const int label = static_cast<int>(sprites.size());

            Sprite sprite;
            sprite.box = { x, y, x, y };
            sprite.regions.push_back(label);

            map.at(x, y) = label;
            stack.push_back({ x, y });

            while (!stack.empty())
            {
                const auto [px, py] = stack.back();
                stack.pop_back();

                ++sprite.pixelCount;
                sprite.box.include({ px, py, px, py });

                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        const int nx = px + dx;
                        const int ny = py + dy;

                        if (nx < 0 || ny < 0 || nx >= width || ny >= height)
                            continue;

                        if (!visible[static_cast<std::size_t>(ny) * width + nx] ||
                            map.at(nx, ny) != -1)
                            continue;

                        map.at(nx, ny) = label;
                        stack.push_back({ nx, ny });
                    }
                }
            }

            sprites.push_back(std::move(sprite));
        }
    }

    return sprites;
}

bool shouldMerge(const Sprite& a, const Sprite& b)
{
    if (a.box.contains(b.box) || b.box.contains(a.box))
        return true;

    if (std::min(a.pixelCount, b.pixelCount) > kFragmentMaxPixels)
        return false;

    return a.box.overlaps(b.box, kFragmentGapX, kFragmentGapY);
}

// Joins detached fragments (sparks, particles, glints) to the sprite they
// belong to. Repeats until nothing changes so chains of pieces merge too.
void mergeFragments(std::vector<Sprite>& sprites)
{
    bool changed = true;

    while (changed)
    {
        changed = false;

        for (std::size_t i = 0; i < sprites.size(); ++i)
        {
            for (std::size_t j = i + 1; j < sprites.size();)
            {
                if (!shouldMerge(sprites[i], sprites[j]))
                {
                    ++j;
                    continue;
                }

                sprites[i].box.include(sprites[j].box);
                sprites[i].pixelCount += sprites[j].pixelCount;
                sprites[i].regions.insert(
                    sprites[i].regions.end(),
                    sprites[j].regions.begin(),
                    sprites[j].regions.end());

                sprites.erase(sprites.begin() + static_cast<std::ptrdiff_t>(j));
                changed = true;
            }
        }
    }
}

void removeNoise(std::vector<Sprite>& sprites)
{
    sprites.erase(
        std::remove_if(sprites.begin(), sprites.end(),
            [](const Sprite& sprite)
            {
                return sprite.pixelCount < kMinSpritePixels;
            }),
        sprites.end());
}

// Groups sprites into rows by vertical overlap. Oversized sprites (artwork)
// get a row of their own so they do not swallow the rows they span.
std::vector<Row> groupIntoRows(std::vector<Sprite> sprites)
{
    std::sort(sprites.begin(), sprites.end(),
        [](const Sprite& a, const Sprite& b)
        {
            if (a.box.top != b.box.top)
                return a.box.top < b.box.top;
            return a.box.left < b.box.left;
        });

    std::vector<int> heights;
    for (const auto& sprite : sprites)
        heights.push_back(sprite.box.height());

    int medianHeight = 0;

    if (!heights.empty())
    {
        std::nth_element(heights.begin(),
            heights.begin() + static_cast<std::ptrdiff_t>(heights.size() / 2),
            heights.end());
        medianHeight = heights[heights.size() / 2];
    }

    std::vector<Row> rows;

    for (auto& sprite : sprites)
    {
        const int height = sprite.box.height();
        const bool oversized = height > kOversizedFactor * medianHeight;

        bool placed = false;

        if (!oversized)
        {
            for (auto& row : rows)
            {
                if (row.oversized)
                    continue;

                const int overlap =
                    std::min(sprite.box.bottom, row.bottom) -
                    std::max(sprite.box.top, row.top) + 1;

                if (overlap * 100 >= height * kRowOverlapPercent)
                {
                    row.top = std::min(row.top, sprite.box.top);
                    row.bottom = std::max(row.bottom, sprite.box.bottom);
                    row.sprites.push_back(std::move(sprite));
                    placed = true;
                    break;
                }
            }
        }

        if (!placed)
        {
            Row row;
            row.top = sprite.box.top;
            row.bottom = sprite.box.bottom;
            row.oversized = oversized;
            row.sprites.push_back(std::move(sprite));
            rows.push_back(std::move(row));
        }
    }

    std::sort(rows.begin(), rows.end(),
        [](const Row& a, const Row& b) { return a.top < b.top; });

    for (auto& row : rows)
    {
        std::sort(row.sprites.begin(), row.sprites.end(),
            [](const Sprite& a, const Sprite& b)
            {
                return a.box.left < b.box.left;
            });
    }

    return rows;
}

// Saves a copy of the sheet with a box drawn around every detected sprite,
// coloured by row, so you can count frames before answering the prompts.
bool savePreview(
    const sf::Image& original,
    const std::vector<Row>& rows,
    const fs::path& path)
{
    static const sf::Color colors[] = {
        sf::Color(255, 0, 0), sf::Color(0, 200, 0), sf::Color(0, 90, 255),
        sf::Color(255, 160, 0), sf::Color(220, 0, 220), sf::Color(0, 200, 200)
    };

    sf::Image preview = original;

    const int width = static_cast<int>(preview.getSize().x);
    const int height = static_cast<int>(preview.getSize().y);

    auto plot = [&](int x, int y, sf::Color color)
        {
            if (x >= 0 && y >= 0 && x < width && y < height)
                preview.setPixel({ static_cast<unsigned int>(x),
                                   static_cast<unsigned int>(y) }, color);
        };

    for (std::size_t r = 0; r < rows.size(); ++r)
    {
        const sf::Color color = colors[r % std::size(colors)];

        for (const auto& sprite : rows[r].sprites)
        {
            const Box& b = sprite.box;

            for (int x = b.left - 1; x <= b.right + 1; ++x)
            {
                plot(x, b.top - 1, color);
                plot(x, b.bottom + 1, color);
            }

            for (int y = b.top - 1; y <= b.bottom + 1; ++y)
            {
                plot(b.left - 1, y, color);
                plot(b.right + 1, y, color);
            }
        }
    }

    return preview.saveToFile(path);
}

// ---------------------------------------------------------------------------
// Choosing animations
// ---------------------------------------------------------------------------

struct LayoutItem
{
    int count = 0;
    bool skip = false;
};

// Parses answers like "3 6 x1 8": a number is an animation with that many
// frames, "xN" skips the next N sprites.
bool parseLayout(const std::string& text, std::vector<LayoutItem>& items)
{
    std::istringstream in(text);
    std::string token;

    while (in >> token)
    {
        LayoutItem item;

        if (token[0] == 'x')
        {
            item.skip = true;
            token.erase(0, 1);
            if (token.empty())
                token = "1";
        }

        if (token.empty() || token.size() > 6 ||
            !std::all_of(token.begin(), token.end(),
                [](unsigned char c) { return std::isdigit(c); }))
            return false;

        item.count = std::stoi(token);

        if (item.count <= 0)
            return false;

        items.push_back(item);
    }

    return !items.empty();
}

// Asks for an animation name. Pressing Enter keeps the default. Spaces are
// turned into underscores so every name is one word in the .cfg file.
std::string askAnimationName(const std::string& defaultName, int frameCount)
{
    std::cout << "    Name for the " << frameCount << "-frame animation ["
        << defaultName << "]: ";

    std::string name;
    std::getline(std::cin, name);

    exitIfInputClosed();

    // Trim and replace inner spaces.
    const auto first = name.find_first_not_of(" \t\r");
    const auto last = name.find_last_not_of(" \t\r");

    if (first == std::string::npos)
        return defaultName;

    name = name.substr(first, last - first + 1);
    std::replace(name.begin(), name.end(), ' ', '_');
    std::replace(name.begin(), name.end(), '\t', '_');

    return name;
}

std::vector<Animation> askAnimationsForRow(
    const Row& row,
    std::size_t rowNumber,
    int& animationNumber)
{
    const int spriteCount = static_cast<int>(row.sprites.size());

    std::cout << "\nRow " << rowNumber << ": " << spriteCount
        << (spriteCount == 1 ? " sprite" : " sprites")
        << ", y " << row.top << ".." << row.bottom
        << (row.oversized ? " (oversized, probably artwork)" : "")
        << "\n  sizes:";

    for (const auto& sprite : row.sprites)
        std::cout << ' ' << sprite.box.width() << 'x' << sprite.box.height();

    std::cout << '\n';

    while (true)
    {
        const std::string answer = toLower(askString(
            "  Frames per animation (e.g. \"3 6 8\"; \"x1\" skips a sprite; "
            "\"all\" or \"skip\"): "));

        if (answer == "skip" || answer == "s")
            return {};

        std::vector<LayoutItem> items;

        if (answer == "all" || answer == "a")
        {
            items.push_back({ spriteCount, false });
        }
        else if (!parseLayout(answer, items))
        {
            std::cout << "  Could not read that. Use numbers separated by "
                "spaces, e.g. \"3 6 8\".\n";
            continue;
        }

        int total = 0;
        for (const auto& item : items)
            total += item.count;

        if (total > spriteCount)
        {
            std::cout << "  That uses " << total
                << " sprites but the row only has " << spriteCount << ".\n";
            continue;
        }

        if (total < spriteCount)
        {
            std::cout << "  Note: the last " << spriteCount - total
                << " sprite(s) of this row are left out.\n";
        }

        std::vector<Animation> animations;
        int next = 0;

        for (const auto& item : items)
        {
            if (item.skip)
            {
                next += item.count;
                continue;
            }

            Animation animation;

            for (int i = 0; i < item.count; ++i)
                animation.frames.push_back(row.sprites[static_cast<std::size_t>(next++)]);

            animation.name = askAnimationName(
                "anim" + std::to_string(animationNumber++),
                static_cast<int>(animation.frames.size()));

            animations.push_back(std::move(animation));
        }

        return animations;
    }
}

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

// Where a frame's artwork sits inside its cell (inclusive, cell coordinates):
// centred horizontally, lined up on the animation's baseline.
Box cellBounds(const Animation& animation, const Sprite& frame)
{
    const int offsetX =
        (static_cast<int>(animation.frameWidth) - frame.box.width()) / 2;

    const int rise = animation.baseline - frame.box.bottom;

    const int offsetY =
        static_cast<int>(animation.frameHeight) - frame.box.height() - rise;

    return { offsetX, offsetY,
             offsetX + frame.box.width() - 1,
             offsetY + frame.box.height() - 1 };
}

// The body box: the part of the cell that every frame covers. Swinging
// limbs and weapons move from frame to frame, so they fall outside it.
// If the frames vary so much that the common part is tiny (a spin, a long
// lunge), the union of all frames is used instead. Edit the hurt line in
// the .cfg when the default is off for a particular animation.
void measureHurtBox(Animation& animation)
{
    Box common = cellBounds(animation, animation.frames.front());
    Box all = common;

    for (const auto& frame : animation.frames)
    {
        const Box b = cellBounds(animation, frame);

        common.left = std::max(common.left, b.left);
        common.top = std::max(common.top, b.top);
        common.right = std::min(common.right, b.right);
        common.bottom = std::min(common.bottom, b.bottom);

        all.include(b);
    }

    const bool usable =
        common.right >= common.left && common.bottom >= common.top &&
        common.width() * 2 >= all.width() && common.height() * 2 >= all.height();

    animation.hurt = usable ? common : all;
}

// On attack-like animations, the part of a frame that reaches past the body
// box (the weapon) becomes that frame's hit box. Other animations get none,
// so a stretched foot in a walk cycle never hurts anyone.
void measureHitBoxes(Animation& animation)
{
    animation.hits.assign(animation.frames.size(), std::nullopt);

    if (!nameContainsAny(animation.name, kAttackWords, std::size(kAttackWords)))
        return;

    for (std::size_t i = 0; i < animation.frames.size(); ++i)
    {
        const Box b = cellBounds(animation, animation.frames[i]);

        const int rightReach = b.right - animation.hurt.right;
        const int leftReach = animation.hurt.left - b.left;

        if (std::max(rightReach, leftReach) < kHitProtrusion)
            continue;

        Box hit = b;

        if (rightReach >= leftReach)
            hit.left = animation.hurt.right + 1;
        else
            hit.right = animation.hurt.left - 1;

        animation.hits[i] = hit;
    }
}

// Every frame of an animation gets the same cell: as wide as the widest
// frame, and tall enough to keep each frame at its original height above
// the animation's baseline (so feet stay planted instead of jittering).
void measureAnimation(Animation& animation)
{
    animation.baseline = 0;
    for (const auto& frame : animation.frames)
        animation.baseline = std::max(animation.baseline, frame.box.bottom);

    animation.frameWidth = 0;
    animation.frameHeight = 0;

    for (const auto& frame : animation.frames)
    {
        const int rise = animation.baseline - frame.box.bottom;

        animation.frameWidth = std::max(
            animation.frameWidth,
            static_cast<unsigned int>(frame.box.width()));

        animation.frameHeight = std::max(
            animation.frameHeight,
            static_cast<unsigned int>(frame.box.height() + rise));
    }

    measureHurtBox(animation);
    measureHitBoxes(animation);

    animation.once =
        nameContainsAny(animation.name, kAttackWords, std::size(kAttackWords)) ||
        nameContainsAny(animation.name, kOnceWords, std::size(kOnceWords));
}

// Copies one sprite into the output. Only pixels that belong to the sprite's
// own regions are copied, so a neighbour whose bounds overlap is left out.
void copySprite(
    const sf::Image& original,
    const RegionMap& map,
    const Sprite& sprite,
    sf::Image& output,
    unsigned int destX,
    unsigned int destY)
{
    for (int y = sprite.box.top; y <= sprite.box.bottom; ++y)
    {
        for (int x = sprite.box.left; x <= sprite.box.right; ++x)
        {
            const int label = map.at(x, y);

            if (label < 0 ||
                std::find(sprite.regions.begin(), sprite.regions.end(), label) ==
                sprite.regions.end())
                continue;

            output.setPixel(
                { destX + static_cast<unsigned int>(x - sprite.box.left),
                  destY + static_cast<unsigned int>(y - sprite.box.top) },
                original.getPixel({ static_cast<unsigned int>(x),
                                    static_cast<unsigned int>(y) }));
        }
    }
}

// Creates a new spritesheet with each animation on its own row and each
// frame in an equally sized cell, centred horizontally and lined up on the
// animation's baseline.
bool createFixedSpritesheet(
    const sf::Image& original,
    const RegionMap& map,
    std::vector<Animation>& animations,
    const fs::path& outputPath)
{
    unsigned int outputWidth = 0;
    unsigned int outputHeight = 0;

    for (auto& animation : animations)
    {
        measureAnimation(animation);

        outputWidth = std::max(
            outputWidth,
            animation.frameWidth *
            static_cast<unsigned int>(animation.frames.size()));

        outputHeight += animation.frameHeight;
    }

    if (outputWidth == 0 || outputHeight == 0)
    {
        std::cerr << "No animations were chosen.\n";
        return false;
    }

    sf::Image output({ outputWidth, outputHeight }, sf::Color::Transparent);

    unsigned int cellY = 0;

    for (const auto& animation : animations)
    {
        unsigned int cellX = 0;

        for (const auto& frame : animation.frames)
        {
            const Box cell = cellBounds(animation, frame);

            copySprite(original, map, frame, output,
                cellX + static_cast<unsigned int>(cell.left),
                cellY + static_cast<unsigned int>(cell.top));

            cellX += animation.frameWidth;
        }

        cellY += animation.frameHeight;
    }

    return output.saveToFile(outputPath);
}

// Writes the .cfg describing the output sheet so the game can rebuild every
// frame rectangle and collision box. SpriteSheet.h documents the format.
bool writeConfig(
    const std::vector<Animation>& animations,
    const fs::path& imagePath,
    const fs::path& configPath,
    float secondsPerFrame)
{
    std::ofstream out(configPath);

    if (!out)
        return false;

    out << "# Sprite sheet config written by Sprite Sheet Processor\n"
        << "# image <file next to this config>\n"
        << "# facing left | right            which way the artwork faces (default right)\n"
        << "# anim <name> <frames> <frameWidth> <frameHeight> <rowY> <secondsPerFrame>\n"
        << "#   frame i of an animation is at x = i * frameWidth, y = rowY\n"
        << "#   the lines after an anim line belong to it:\n"
        << "#   loop | once                  repeat (default) or stop on the last frame\n"
        << "#   next <anim>                  animation to play after a once animation ends\n"
        << "#   hurt <x> <y> <w> <h>         body box in cell coordinates (auto: area every frame covers)\n"
        << "#   hit <i> <x> <y> <w> <h>      damage box on frame i (auto on attack-like names: the part past the body)\n\n"
        << "image " << imagePath.filename().string() << "\n"
        << "facing right\n\n";

    unsigned int rowTop = 0;

    for (const auto& animation : animations)
    {
        out << "anim " << animation.name << ' '
            << animation.frames.size() << ' '
            << animation.frameWidth << ' '
            << animation.frameHeight << ' '
            << rowTop << ' '
            << secondsPerFrame << '\n';

        if (animation.once)
            out << "once\n";

        out << "hurt " << animation.hurt.left << ' ' << animation.hurt.top << ' '
            << animation.hurt.width() << ' ' << animation.hurt.height() << '\n';

        for (std::size_t i = 0; i < animation.hits.size(); ++i)
        {
            if (!animation.hits[i])
                continue;

            const Box& hit = *animation.hits[i];

            out << "hit " << i << ' ' << hit.left << ' ' << hit.top << ' '
                << hit.width() << ' ' << hit.height() << '\n';
        }

        out << '\n';

        rowTop += animation.frameHeight;
    }

    return static_cast<bool>(out);
}

// ---------------------------------------------------------------------------

int main()
{
    const std::string fixSheet = askString("Fix a Spritesheet(y/n)?  ");

    if (fixSheet == "y")
    {
        std::cout << "=== Sprite Sheet Processor ===\n\n";

        const std::string filename = askString("Enter spritesheet filename: ");

        // Accept either a path or a file name inside the assets folder.
        fs::path imagePath = filename;

        if (!fs::exists(imagePath))
            imagePath = fs::path("assets") / filename;

        sf::Image image;

        if (!image.loadFromFile(imagePath))
        {
            std::cerr << "Failed to load: " << fs::absolute(imagePath) << '\n';
            return 1;
        }

        std::cout << "\nLoaded spritesheet: " << image.getSize().x << " x "
            << image.getSize().y << "\n";

        const fs::path directory = imagePath.parent_path();
        const std::string stem = imagePath.stem().string();

        // 1. Detect every sprite.
        RegionMap map;
        std::vector<Sprite> sprites = findRegions(image, map);

        mergeFragments(sprites);
        removeNoise(sprites);

        if (sprites.empty())
        {
            std::cerr << "No visible pixels found. Is the background transparent?\n";
            return 1;
        }

        // 2. Arrange them into rows.
        const std::vector<Row> rows = groupIntoRows(std::move(sprites));

        std::size_t total = 0;
        for (const auto& row : rows)
            total += row.sprites.size();

        std::cout << "Found " << total << " sprites in " << rows.size() << " rows.\n";

        const fs::path previewPath = directory / (stem + "_detected.png");

        if (savePreview(image, rows, previewPath))
        {
            std::cout << "Preview with boxes around each sprite: "
                << fs::absolute(previewPath) << "\n"
                << "Open it to count frames before answering below.\n"
                << "The game looks for animations named idle, walk, attack, hurt, "
                "defend, defend_hit and dead.\n";
        }

        // 3. Ask how each row splits into animations.
        std::vector<Animation> animations;
        int animationNumber = 1;

        for (std::size_t r = 0; r < rows.size(); ++r)
        {
            std::vector<Animation> chosen =
                askAnimationsForRow(rows[r], r + 1, animationNumber);

            for (auto& animation : chosen)
                animations.push_back(std::move(animation));
        }

        // 4. Build the new sheet.
        std::cout << "\nCreating fixed spritesheet...\n";

        const fs::path outputPath = directory / (stem + "_fixed.png");

        if (!createFixedSpritesheet(image, map, animations, outputPath))
        {
            std::cerr << "Failed to create output spritesheet.\n";
            return 1;
        }

        // One config per sheet, named after it: alucard.png -> alucard.cfg.
        const fs::path configPath = directory / (stem + ".cfg");

        if (!writeConfig(animations, outputPath, configPath, kDefaultSecondsPerFrame))
        {
            std::cerr << "Failed to write " << configPath << '\n';
            return 1;
        }

        std::cout << "\nFinished!\n";
        std::cout << "Output:   " << fs::absolute(outputPath) << '\n';
        std::cout << "Config:   " << fs::absolute(configPath) << "\n\n";

        unsigned int rowTop = 0;

        for (const auto& animation : animations)
        {
            std::size_t hitFrames = 0;
            for (const auto& hit : animation.hits)
                hitFrames += hit ? 1 : 0;

            std::cout << animation.name << ": " << animation.frames.size()
                << " frames of " << animation.frameWidth << "x"
                << animation.frameHeight << ", output row starts at Y="
                << rowTop << ", hurt box " << animation.hurt.width() << "x"
                << animation.hurt.height();

            if (hitFrames > 0)
                std::cout << ", hit boxes on " << hitFrames << " frame(s)";

            std::cout << '\n';

            rowTop += animation.frameHeight;
        }

        return 0;
    }

    const std::string config = askString(
        "Sprite config to run (e.g. assets/alucard.cfg): ");

    return runGame(config, config);
}
