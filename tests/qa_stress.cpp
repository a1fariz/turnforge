#include "game.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>

using namespace turnforge;
namespace {
int checks = 0;
void require(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
bool sameState(const Game& a, const Game& b) {
    if (a.walls != b.walls || a.phase != b.phase || a.round != b.round || a.moved != b.moved ||
        a.attacked != b.attacked || a.guarding != b.guarding || a.enemyIndex != b.enemyIndex ||
        a.player.position != b.player.position || a.player.hp != b.player.hp || a.log != b.log || a.message != b.message)
        return false;
    for (std::size_t i = 0; i < a.enemies.size(); ++i)
        if (a.enemies[i].position != b.enemies[i].position || a.enemies[i].hp != b.enemies[i].hp) return false;
    return true;
}
void invariants(const Game& game) {
    require(game.player.hp >= 0 && game.player.hp <= PlayerMaxHp, "Player HP outside bounds");
    require(inBounds(game.player.position) && !game.wallAt(game.player.position), "Player outside floor");
    require(game.log.size() <= 5 && game.round >= 1, "Journal or round invariant");
    require(!game.guarding || (game.attacked && game.moved &&
        (game.phase == Phase::PlayerTurn || game.phase == Phase::EnemyTurn)), "Invalid guard lifecycle");
    require(game.phase != Phase::Lost || game.player.hp == 0, "Lost with surviving player");
    require(game.phase != Phase::Won || (!game.enemies[0].alive() && !game.enemies[1].alive()), "Won with surviving enemy");
    for (std::size_t i = 0; i < game.enemies.size(); ++i) {
        const auto& enemy = game.enemies[i];
        require(enemy.hp >= 0 && enemy.hp <= EnemyMaxHp, "Enemy HP outside bounds");
        if (!enemy.alive()) continue;
        require(inBounds(enemy.position) && !game.wallAt(enemy.position), "Enemy outside floor");
        require(enemy.position != game.player.position, "Enemy overlaps player");
        for (std::size_t j = 0; j < i; ++j)
            require(!game.enemies[j].alive() || enemy.position != game.enemies[j].position, "Enemies overlap");
    }
}
void checkPaths(const Game& game) {
    // Relaxation provides an independent shortest-distance oracle for the BFS implementation.
    std::array<int, 64> oracle;
    oracle.fill(1000);
    const int origin = game.player.position.y * GridSize + game.player.position.x;
    oracle[origin] = 0;
    for (int pass = 0; pass < 63; ++pass) {
        bool changed = false;
        for (int from = 0; from < 64; ++from) {
            if (oracle[from] == 1000) continue;
            for (int to = 0; to < 64; ++to) {
                const Position destination{to % 8, to / 8};
                if (distance({from % 8, from / 8}, destination) != 1 ||
                    game.wallAt(destination) || game.occupied(destination)) continue;
                if (oracle[to] > oracle[from] + 1) { oracle[to] = oracle[from] + 1; changed = true; }
            }
        }
        if (!changed) break;
    }
    const auto reachable = game.movementDistances();
    for (int i = 0; i < 64; ++i) {
        const Position destination{i % 8, i / 8};
        const auto route = game.path(game.player.position, destination);
        const int expected = oracle[i] == 1000 ? -1 : oracle[i];
        require(reachable[i] == expected, "Movement distances disagree with independent oracle");
        require((route.empty() ? -1 : static_cast<int>(route.size()) - 1) == expected,
                "Path length disagrees with independent oracle");
        for (std::size_t step = 1; step < route.size(); ++step)
            require(distance(route[step - 1], route[step]) == 1 && !game.wallAt(route[step]) && !game.occupied(route[step]),
                    "Path crosses obstacle, occupied tile or diagonal");
    }
}
} // namespace

int main() {
    constexpr unsigned int Seed = 20261030;
    std::mt19937 random(Seed);
    int actions = 0, wins = 0, losses = 0;
    try {
        for (int match = 0; match < 500; ++match) {
            Game game;
            game.start();
            if (match % 2) {
                for (int i = 0; i < 64; ++i)
                    game.walls[i] = random() % 5 == 0 && !game.occupied({i % 8, i / 8});
            }
            checkPaths(game);
            for (int step = 0; step < 200; ++step) {
                const auto before = game;
                auto replay = game;
                const int action = static_cast<int>(random() % 7);
                Position target{static_cast<int>(random() % 12) - 2, static_cast<int>(random() % 12) - 2};
                if (action == 0 && game.canMove()) {
                    const auto range = game.movementDistances();
                    std::vector<Position> choices;
                    for (int i = 0; i < 64; ++i)
                        if (range[i] > 0 && range[i] <= 3) choices.push_back({i % 8, i / 8});
                    if (!choices.empty()) target = choices[random() % choices.size()];
                }
                int enemy = static_cast<int>(random() % 5) - 1;
                if (action == 2)
                    for (int i = 0; i < 2; ++i) if (game.canAttack(i)) { enemy = i; break; }
                auto apply = [&](Game& state) {
                    if (state.phase == Phase::EnemyTurn) { state.stepEnemy(); return true; }
                    switch (action) {
                        case 0: case 1: return state.tryMove(target);
                        case 2: return state.tryAttack(enemy);
                        case 3: return state.tryGuard();
                        case 4: case 5: return state.endTurn();
                        default: state.toTitle(); state.start(); return true;
                    }
                };
                const bool accepted = apply(game);
                require(apply(replay) == accepted && sameState(game, replay), "Identical actions replay differently");
                if (!accepted) {
                    auto unchanged = game;
                    unchanged.message = before.message;
                    require(sameState(unchanged, before), "Rejected action changed gameplay state");
                }
                invariants(game);
                ++actions;
                if (game.phase == Phase::Won || game.phase == Phase::Lost) {
                    wins += game.phase == Phase::Won;
                    losses += game.phase == Phase::Lost;
                    break;
                }
            }
        }
        std::cout << "PASS stress: 500 seeded sessions, " << actions << " actions, " << checks
                  << " checks, seed " << Seed << ", " << wins << " wins, " << losses << " losses\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL stress after " << actions << " actions: " << error.what() << '\n';
        return 1;
    }
}
