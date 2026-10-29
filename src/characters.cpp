#include "characters.h"

#include <algorithm>
#include <cmath>

using turnforge::Game;
using turnforge::Phase;
using turnforge::Position;

namespace {
const turnforge::Unit& unit(const Game& game, int i) { return i == 0 ? game.player : game.enemies[i - 1]; }
Vector2 point(Position p) { return {static_cast<float>(p.x), static_cast<float>(p.y)}; }
Vector2 direction(Vector2 from, Vector2 to) {
    Vector2 delta{to.x - from.x, to.y - from.y};
    const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    return length > 0 ? Vector2{delta.x / length, delta.y / length} : Vector2{0, 0};
}
}

bool Characters::load(const std::string& filename) {
    Image image = LoadImage(filename.c_str());
    if (!IsImageValid(image) || image.width < 3) {
        UnloadImage(image);
        return false;
    }
    // Alpha bounds isolate each equal-width atlas cell without changing the generated artwork.
    for (int i = 0; i < 3; ++i) {
        const int left = image.width * i / 3;
        const int right = image.width * (i + 1) / 3;
        int minX = right, minY = image.height, maxX = left, maxY = 0;
        for (int y = 0; y < image.height; ++y) {
            for (int x = left; x < right; ++x) {
                if (GetImageColor(image, x, y).a > 24) {
                    minX = std::min(minX, x); maxX = std::max(maxX, x);
                    minY = std::min(minY, y); maxY = std::max(maxY, y);
                }
            }
        }
        if (maxX <= minX || maxY <= minY || minX <= left || maxX >= right - 1) {
            UnloadImage(image);
            return false;
        }
        frames[i] = {static_cast<float>(minX), static_cast<float>(minY), static_cast<float>(maxX - minX + 1), static_cast<float>(maxY - minY + 1)};
    }
    atlas = LoadTextureFromImage(image);
    UnloadImage(image);
    if (!IsTextureValid(atlas)) return false;
    SetTextureFilter(atlas, TEXTURE_FILTER_BILINEAR);
    return valid();
}

void Characters::unload() {
    if (IsTextureValid(atlas)) UnloadTexture(atlas);
    atlas = {};
}

bool Characters::valid() const {
    if (!IsTextureValid(atlas)) return false;
    for (const auto& frame : frames)
        if (frame.width <= 0 || frame.height <= 0 || frame.x < 0 || frame.y < 0 ||
            frame.x + frame.width > atlas.width || frame.y + frame.height > atlas.height) return false;
    for (const auto& actor : actors)
        if (!std::isfinite(actor.position.x) || !std::isfinite(actor.position.y)) return false;
    return true;
}

void Characters::reset(const Game& game) {
    for (int i = 0; i < 3; ++i) {
        actors[i] = Actor{};
        actors[i].target = unit(game, i).position;
        actors[i].position = point(unit(game, i).position);
        actors[i].hp = unit(game, i).hp;
        actors[i].facing = i == 0 ? 1 : -1;
    }
    initialized = true;
    lastPhase = game.phase;
    lastRound = game.round;
    lastActive = game.activeEnemy();
}

void Characters::update(const Game& game, float dt) {
    dt = std::clamp(dt, 0.0f, 0.1f);
    time += dt;
    bool newSession = !initialized || game.round < lastRound ||
        (game.phase == Phase::Title && lastPhase != Phase::Title) ||
        (lastPhase == Phase::Title && game.phase == Phase::PlayerTurn);
    for (int i = 0; i < 3; ++i) newSession = newSession || unit(game, i).hp > actors[i].hp;
    if (newSession) reset(game);

    auto startAttack = [&](int attacker, int defender) {
        auto& a = actors[attacker];
        a.attack = 0.28f;
        a.attackDirection = direction(point(unit(game, attacker).position), point(unit(game, defender).position));
        if (a.attackDirection.x != 0) a.facing = a.attackDirection.x > 0 ? 1 : -1;
    };
    for (int i = 0; i < 3; ++i) {
        auto& a = actors[i];
        const auto& current = unit(game, i);
        a.attack = std::max(0.0f, a.attack - dt);
        a.hit = std::max(0.0f, a.hit - dt);
        a.death = std::max(0.0f, a.death - dt);
        if (current.position != a.target) {
            Game routing = game;
            if (i == 0) routing.player.hp = 0;
            else routing.enemies[i - 1].hp = 0;
            a.route = routing.path(a.target, current.position);
            a.travel = 0;
            a.target = current.position;
            if (a.route.empty()) a.position = point(current.position);
        }
        if (a.route.size() > 1) {
            a.travel += dt / 0.115f;
            const int segments = static_cast<int>(a.route.size()) - 1;
            const int step = std::min(static_cast<int>(a.travel), segments - 1);
            const float t = std::min(1.0f, a.travel - step);
            const auto from = point(a.route[step]);
            const auto to = point(a.route[step + 1]);
            a.position = {from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t};
            if (to.x != from.x) a.facing = to.x > from.x ? 1 : -1;
            if (a.travel >= segments) { a.position = point(a.target); a.route.clear(); }
        }
        if (current.hp < a.hp) {
            a.damage = a.hp - current.hp;
            a.hit = 0.32f;
            if (!current.alive()) a.death = 0.38f;
            if (i > 0) startAttack(0, i);
            else if (lastActive >= 0) startAttack(lastActive + 1, 0);
        }
        a.hp = current.hp;
    }
    lastPhase = game.phase;
    lastRound = game.round;
    lastActive = game.activeEnemy();
}

bool Characters::busy() const {
    for (const auto& a : actors)
        if (a.route.size() > 1 || a.attack > 0 || a.death > 0) return true;
    return false;
}

bool Characters::selfCheck() const {
    Characters probe = *this;
    Game game;
    game.start();
    game.walls.fill(false);
    game.walls[2 * 8 + 2] = true;
    game.player.position = {1, 2};
    probe.reset(game);
    if (!game.tryMove({3, 1})) return false;
    probe.update(game, 0);
    if (!probe.busy()) return false;
    for (int i = 0; i < 40; ++i) {
        probe.update(game, 0.03f);
        const auto p = probe.actors[0].position;
        const Position tile{static_cast<int>(std::round(p.x)), static_cast<int>(std::round(p.y))};
        if (!turnforge::inBounds(tile) || game.wallAt(tile) || !probe.valid()) return false;
    }
    if (probe.busy() || probe.actors[0].position.x != 3 || probe.actors[0].position.y != 1) return false;
    game.moved = false;
    game.enemies[0] = {{3, 0}, 6};
    game.enemies[1].hp = 0;
    probe.reset(game);
    if (!game.tryAttack(0)) return false;
    probe.update(game, 0.016f);
    if (probe.actors[0].attack <= 0 || probe.actors[1].hit <= 0 || !probe.busy()) return false;
    for (int i = 0; i < 40; ++i) probe.update(game, 0.016f);
    if (probe.busy()) return false;
    game.endTurn(); game.stepEnemy();
    if (!game.tryAttack(0)) return false;
    probe.update(game, 0.016f);
    if (probe.actors[1].death <= 0 || !probe.busy()) return false;
    for (int i = 0; i < 40; ++i) probe.update(game, 0.016f);
    if (probe.busy()) return false;
    game.start(); probe.update(game, 0.016f);
    if (probe.busy() || probe.actors[0].position.x != 1 || probe.actors[0].position.y != 6) return false;
    game.enemies[0] = {{1, 5}, 6}; game.enemies[1].hp = 0;
    probe.reset(game);
    if (!game.tryGuard()) return false;
    game.endTurn(); game.stepEnemy(); probe.update(game, 0.016f);
    if (probe.actors[0].damage != 1 || game.guarding) return false;
    game.start(); probe.update(game, 0.016f);
    game.tryMove({2, 6}); probe.update(game, 0.016f);
    game.toTitle(); probe.update(game, 0.016f);
    return !probe.busy() && probe.valid() && probe.actors[0].position.x == 1;
}

void Characters::draw(const Game& game, Rectangle bounds, bool highlights) const {
    const float tile = bounds.width / 8;
    const Color teal{110, 220, 191, 255};
    const Color orange{245, 170, 108, 255};
    const Color gold{237, 207, 137, 255};
    std::array<int, 3> order{{0, 1, 2}};
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return actors[a].position.y < actors[b].position.y; });
    for (int i : order) {
        const auto& a = actors[i];
        if (a.hp <= 0 && a.death <= 0) continue;
        Vector2 feet{bounds.x + (a.position.x + 0.5f) * tile, bounds.y + (a.position.y + 0.82f) * tile};
        const bool target = highlights && i > 0 && game.canAttack(i - 1);
        const bool active = highlights && (i == 0 ? game.phase == Phase::PlayerTurn : game.activeEnemy() == i - 1);
        const float alpha = a.hp > 0 ? 1.0f : a.death / 0.38f;
        DrawEllipse(static_cast<int>(feet.x), static_cast<int>(feet.y - 3), tile * 0.28f, tile * 0.09f, Fade(BLACK, 0.28f * alpha));
        if (active || target) DrawEllipseLines(static_cast<int>(feet.x), static_cast<int>(feet.y - 3), tile * 0.36f, tile * 0.14f, target ? gold : (i == 0 ? teal : orange));
        if (a.attack > 0) {
            const float lunge = std::sin(a.attack / 0.28f * 3.14159265f) * tile * 0.19f;
            feet.x += a.attackDirection.x * lunge;
            feet.y += a.attackDirection.y * lunge;
        }
        const float bob = a.route.size() > 1 ? std::sin(a.travel * 6.2831853f) * 2.0f : std::sin(time * 2.8f + i) * 0.9f;
        const float height = tile * 1.05f;
        const float scale = std::min(height / frames[i].height, tile * 0.98f / frames[i].width);
        const float width = frames[i].width * scale;
        const float drawnHeight = frames[i].height * scale;
        Rectangle source = frames[i];
        if (a.facing < 0) source.width = -source.width;
        const Rectangle destination{feet.x - width / 2, feet.y - drawnHeight - bob, width, drawnHeight};
        const Color tint = a.hit > 0 && static_cast<int>(a.hit * 30) % 2 == 0 ? Color{255, 150, 150, static_cast<unsigned char>(255 * alpha)} : Fade(WHITE, alpha);
        DrawTexturePro(atlas, source, destination, {0, 0}, 0, tint);
        if (i == 0 && (game.guarding || (a.hit > 0 && a.damage == 1)) && a.hp > 0) {
            const Color ward{135, 202, 250, 220};
            const Vector2 shield{feet.x + tile * 0.29f, feet.y - tile * 0.43f};
            DrawCircleLines(static_cast<int>(feet.x), static_cast<int>(feet.y - tile * 0.43f), tile * 0.4f, ward);
            DrawPoly(shield, 5, tile * 0.14f, -18, ward);
            DrawLineEx({shield.x, shield.y - tile * 0.08f}, {shield.x, shield.y + tile * 0.08f}, 2, {28, 58, 84, 255});
        }
        if (a.attack > 0) {
            const Vector2 slash{feet.x + a.attackDirection.x * tile * 0.26f, feet.y - tile * 0.4f + a.attackDirection.y * tile * 0.2f};
            DrawRing(slash, tile * 0.22f, tile * 0.27f, -70, 60, 12, Fade(i == 0 ? gold : orange, a.attack / 0.28f));
        }
        if (a.hp > 0) {
            const Rectangle bar{feet.x - tile * 0.28f, bounds.y + (a.position.y + 0.94f) * tile, tile * 0.56f, 4};
            DrawRectangleRec(bar, {47, 63, 80, 255});
            DrawRectangleRec({bar.x, bar.y, bar.width * a.hp / (i == 0 ? 12.0f : 6.0f), bar.height}, i == 0 ? teal : orange);
        }
        if (a.hit > 0) {
            const std::string damage = "-" + std::to_string(a.damage);
            DrawText(damage.c_str(), static_cast<int>(feet.x + tile * 0.2f), static_cast<int>(feet.y - drawnHeight - 12 - (0.32f - a.hit) * 35), 18, i == 0 ? orange : gold);
        }
    }
}
