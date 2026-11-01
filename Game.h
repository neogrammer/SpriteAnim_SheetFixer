// Game.h
#pragma once

#include <filesystem>

// Opens the window and runs the demo until Escape or the window closes.
// Returns the process exit code. Both actors may use the same config.
int runGame(const std::filesystem::path& playerConfig, const std::filesystem::path& enemyConfig);
