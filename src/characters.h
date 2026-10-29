#pragma once

#include "game.h"
#include "raylib.h"

#include <array>
#include <string>
#include <vector>

class Characters {
public:
    bool load(const std::string& filename);
    void unload();
    void update(const turnforge::Game& game, float dt);
    void draw(const turnforge::Game& game, Rectangle bounds, bool highlights) const;
    bool busy() const;
    bool valid() const;
    bool selfCheck() const;

private:
    struct Actor {
        Vector2 position{};
        turnforge::Position target{};
        std::vector<turnforge::Position> route;
        float travel = 0;
        float attack = 0;
        float hit = 0;
        float death = 0;
        Vector2 attackDirection{};
        int facing = 1;
        int hp = 0;
        int damage = 0;
    };
    Texture2D atlas{};
    std::array<Rectangle, 3> frames{};
    std::array<Actor, 3> actors{};
    bool initialized = false;
    turnforge::Phase lastPhase = turnforge::Phase::Title;
    int lastRound = 1;
    int lastActive = -1;
    float time = 0;

    void reset(const turnforge::Game& game);
};
