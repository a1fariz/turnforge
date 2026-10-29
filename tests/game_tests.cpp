#include "game.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace turnforge;
namespace {
int checks = 0;
void expect(bool condition, const std::string& message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
Game emptyArena() {
    Game g;
    g.start();
    g.walls.fill(false);
    g.enemies[0].hp = 0;
    g.enemies[1].hp = 0;
    return g;
}
void resolveEnemies(Game& g) {
    int steps = 0;
    while (g.phase == Phase::EnemyTurn && steps++ < 3) g.stepEnemy();
    expect(g.phase != Phase::EnemyTurn, "Enemy phase must terminate");
}
void expectNoOverlap(const Game& g) {
    for (std::size_t i = 0; i < g.enemies.size(); ++i) {
        const auto& e = g.enemies[i];
        if (!e.alive()) continue;
        expect(inBounds(e.position) && !g.wallAt(e.position), "Enemy must stand on a floor tile");
        expect(!g.player.alive() || e.position != g.player.position, "Enemy overlaps player");
        for (std::size_t j = 0; j < i; ++j)
            expect(!g.enemies[j].alive() || e.position != g.enemies[j].position, "Enemies overlap");
    }
}
void initialLayout() {
    Game g;
    expect(g.phase == Phase::Title, "Game should open on title");
    expect(g.validInitialLayout(), "Initial map/spawns must be valid and reachable");
    expect(std::count(g.walls.begin(), g.walls.end(), true) == 4, "Initial wall count");
    expect(g.player.position == Position{1, 6} && g.player.hp == 12, "Player spawn/stats");
    expect(g.enemies[0].position == Position{6, 1} && g.enemies[1].position == Position{6, 5}, "Enemy spawns");
    g.start();
    expect(g.phase == Phase::PlayerTurn && g.round == 1 && g.canMove(), "First player turn");
    expectNoOverlap(g);
}
void shortestPath() {
    auto g = emptyArena();
    g.player.position = {0, 0};
    g.walls[1] = true;
    const auto route = g.path({0, 0}, {2, 0});
    expect(route.size() == 5, "BFS must find four-step detour around wall");
    for (std::size_t i = 1; i < route.size(); ++i) {
        expect(distance(route[i - 1], route[i]) == 1, "Path must be orthogonal");
        expect(!g.wallAt(route[i]), "Path crosses wall");
    }
    expect(g.path({-1, 0}, {2, 0}).empty(), "Invalid origin");
    expect(g.path({0, 0}, {8, 0}).empty(), "Invalid destination");
    expect(g.path({1, 0}, {2, 0}).empty(), "Wall origin");
    g.walls[2 + 8] = true;
    g.walls[3] = true;
    expect(g.path({0, 0}, {2, 0}).empty(), "Enclosed destination is unreachable");
}
void movementGuards() {
    Game g;
    g.start();
    const auto original = g.player.position;
    for (Position p : {Position{-1, 0}, Position{8, 7}, Position{2, 2}, Position{6, 1}, Position{5, 6}, original}) {
        expect(!g.tryMove(p), "Invalid move accepted");
        expect(g.player.position == original && !g.moved && !g.attacked, "Invalid move consumed an action");
    }
    const auto range = g.movementDistances();
    expect(range[6 * 8 + 4] == 3, "Three-step reachable distance");
    expect(g.tryMove({4, 6}), "Three-step move should work");
    expect(!g.tryMove({4, 7}) && g.player.position == Position{4, 6}, "Second move accepted");
    expect(g.movementDistances()[6 * 8 + 4] == -1, "Range should disappear after movement");
}
void occupiedAndDead() {
    auto g = emptyArena();
    g.enemies[0] = {{2, 6}, 6};
    expect(g.path(g.player.position, {2, 6}).empty(), "Cannot path onto living enemy");
    expect(!g.tryMove({2, 6}), "Cannot move onto living enemy");
    g.enemies[0].hp = 0;
    expect(!g.occupied({2, 6}), "Dead enemy still occupies tile");
    expect(g.tryMove({2, 6}), "Can move onto dead enemy tile");
}
void invalidAttacks() {
    auto g = emptyArena();
    g.enemies[0] = {{2, 5}, 6};
    expect(!g.tryAttack(0), "Diagonal attack accepted");
    expect(!g.tryAttack(-1) && !g.tryAttack(2), "Invalid enemy index accepted");
    g.enemies[0].position = {7, 0};
    expect(!g.tryAttack(0), "Distant attack accepted");
    g.enemies[0] = {{1, 5}, 0};
    expect(!g.tryAttack(0), "Dead target accepted");
    expect(!g.moved && !g.attacked && g.canMove(), "Rejected attacks consumed actions");
}
void combatAndActionOrder() {
    auto g = emptyArena();
    g.enemies[0] = {{1, 5}, 6};
    expect(g.tryAttack(0), "Adjacent attack failed");
    expect(g.enemies[0].hp == 3, "Wrong damage");
    expect(!g.canMove() && !g.tryMove({2, 6}), "Movement after attack accepted");
    expect(!g.tryAttack(0) && g.enemies[0].hp == 3, "Second attack applied damage");
    g.start();
    g.enemies[0] = {{3, 6}, 6};
    g.enemies[1].hp = 0;
    expect(g.tryMove({2, 6}) && g.tryAttack(0), "Move then attack should work");
}
void winningStopsActions() {
    auto g = emptyArena();
    g.enemies[0] = {{1, 5}, 1};
    expect(g.tryAttack(0), "Final attack failed");
    expect(g.enemies[0].hp == 0 && g.phase == Phase::Won, "Final enemy should immediately trigger win");
    expect(!g.occupied({1, 5}), "Dead unit occupancy");
    expect(!g.tryMove({2, 6}) && !g.tryAttack(0) && !g.endTurn(), "Terminal actions must be rejected");
    g.stepEnemy();
    expect(g.phase == Phase::Won && g.player.hp == 12, "Terminal phase mutated");
}
void phaseGuardsAndSkip() {
    Game g;
    expect(!g.tryMove({2, 6}) && !g.tryAttack(0) && !g.endTurn(), "Actions from title");
    g.start();
    expect(g.endTurn(), "Skipping player actions should work");
    expect(!g.tryMove({2, 6}) && !g.tryAttack(0) && !g.endTurn(), "Player input during enemy turn");
    resolveEnemies(g);
    expect(g.phase == Phase::PlayerTurn && g.round == 2 && !g.moved && !g.attacked, "Next-turn reset");
}
void adjacentEnemyAndDeath() {
    auto g = emptyArena();
    g.player.hp = 1;
    g.enemies[0] = {{1, 5}, 6};
    g.enemies[1] = {{7, 7}, 6};
    g.endTurn();
    g.stepEnemy();
    expect(g.player.hp == 0 && g.phase == Phase::Lost, "Lethal enemy attack must stop immediately");
    expect(g.enemies[0].position == Position{1, 5}, "Adjacent enemy must not move");
    const auto second = g.enemies[1].position;
    g.stepEnemy();
    expect(g.enemies[1].position == second && g.round == 1, "Enemy acted after player death");
}
void enemyStepAndTieBreak() {
    auto g = emptyArena();
    g.player.position = {3, 3};
    g.enemies[0] = {{3, 0}, 6};
    g.endTurn();
    g.stepEnemy();
    expect(g.enemies[0].position == Position{3, 2}, "Enemy should move two and attack");
    expect(g.player.hp == 10, "Move-then-attack damage");
    auto tie = emptyArena();
    tie.player.position = {3, 3};
    tie.enemies[0] = {{1, 1}, 6};
    tie.endTurn();
    auto copy = tie;
    tie.stepEnemy(); copy.stepEnemy();
    expect(tie.enemies[0].position == Position{3, 1}, "Equal-distance target should prefer upper neighbor");
    expect(tie.enemies[0].position == copy.enemies[0].position && tie.log == copy.log, "AI is nondeterministic");
}
void blockedEnemyAndDeadSkip() {
    auto g = emptyArena();
    g.enemies[0] = {{6, 1}, 6};
    for (Position p : {Position{6, 0}, Position{7, 1}, Position{6, 2}, Position{5, 1}})
        g.walls[p.y * 8 + p.x] = true;
    g.endTurn(); g.stepEnemy();
    expect(g.enemies[0].position == Position{6, 1} && g.phase == Phase::PlayerTurn, "Blocked AI must wait and yield");
    g = emptyArena();
    g.enemies[1] = {{1, 5}, 6};
    g.moved = true; g.attacked = true;
    g.endTurn();
    expect(g.activeEnemy() == 1, "Dead first enemy must be skipped");
    g.stepEnemy();
    expect(g.player.hp == 10 && g.canMove() && !g.attacked, "Living second enemy should act, then reset player");
}
void wholeMatches() {
    Game win;
    win.start();
    expect(win.tryMove({4, 6}), "Opening move");
    for (int turn = 0; turn < 12 && win.phase == Phase::PlayerTurn; ++turn) {
        int target = -1;
        for (int i = 0; i < 2; ++i)
            if (win.canAttack(i) && (target == -1 || win.enemies[i].hp < win.enemies[target].hp)) target = i;
        if (target != -1) win.tryAttack(target);
        if (win.phase == Phase::Won) break;
        win.endTurn();
        while (win.phase == Phase::EnemyTurn) { win.stepEnemy(); expectNoOverlap(win); }
    }
    expect(win.phase == Phase::Won && win.player.hp > 0, "Original map needs a playable winning strategy");
    Game lose;
    lose.start();
    for (int turn = 0; turn < 30 && lose.phase == Phase::PlayerTurn; ++turn) {
        lose.endTurn();
        while (lose.phase == Phase::EnemyTurn) { lose.stepEnemy(); expectNoOverlap(lose); }
    }
    expect(lose.phase == Phase::Lost && lose.player.hp == 0, "Idle strategy should reach defeat");
    expect(lose.log.size() <= 5, "Action journal exceeded five entries");
}
void restartAndTitle() {
    Game g;
    for (int i = 0; i < 10; ++i) {
        g.phase = i % 2 ? Phase::Won : Phase::Lost;
        g.player.hp = 0; g.player.position = {7, 7};
        g.walls.fill(true); g.enemies[0].hp = 0;
        g.round = 99; g.enemyIndex = 2; g.moved = true; g.attacked = true; g.guarding = true;
        g.log.assign(5, "stale");
        g.start();
        expect(g.validInitialLayout() && g.phase == Phase::PlayerTurn, "Restart layout/phase");
        expect(g.player.hp == 12 && g.enemies[0].hp == 6 && g.enemies[1].hp == 6, "Restart HP");
        expect(g.round == 1 && g.enemyIndex == 0 && !g.moved && !g.attacked && !g.guarding && g.log.size() == 1, "Stale state after restart");
    }
    g.toTitle();
    expect(g.phase == Phase::Title && g.log.empty(), "Return to title");
    g.start();
    expect(g.player.position == Position{1, 6}, "Start must create a new match");
}

void distinctEnemyRoles() {
    auto goblin = emptyArena();
    goblin.player.position = {3, 3};
    goblin.enemies[0] = {{1, 3}, 6};
    goblin.endTurn(); goblin.stepEnemy();
    expect(goblin.enemies[0].position == Position{2, 3} && goblin.player.hp == 10,
           "Goblin should take the shortest approach and attack");

    auto flank = emptyArena();
    flank.player.position = {3, 3};
    flank.enemies[0] = {{3, 2}, 6};
    flank.enemies[1] = {{1, 3}, 6};
    flank.endTurn();
    flank.stepEnemy();
    const auto before = flank;
    flank.stepEnemy();
    expect(flank.enemies[1].position == Position{2, 4}, "Bandit should approach the opposite side instead of rushing left");
    expect(flank.player.hp == 10, "A distant bandit must not attack diagonally");
    expectNoOverlap(flank);
    auto repeat = before;
    repeat.stepEnemy();
    expect(repeat.enemies[1].position == flank.enemies[1].position && repeat.log == flank.log, "Flank planning must be deterministic");
    flank.endTurn(); resolveEnemies(flank);
    expect(flank.enemies[0].position == Position{3, 2} && flank.enemies[1].position == Position{3, 4},
           "Enemies should occupy opposite attack sides when reachable");
    expect(flank.player.hp == 6, "Each adjacent enemy attacks once");
}

void banditFallbacks() {
    auto adjacent = emptyArena();
    adjacent.player.position = {3, 3};
    adjacent.enemies[0] = {{3, 2}, 6};
    adjacent.enemies[1] = {{2, 3}, 6};
    adjacent.endTurn(); resolveEnemies(adjacent);
    expect(adjacent.enemies[1].position == Position{2, 3} && adjacent.player.hp == 8, "Adjacent bandit must attack without repositioning");

    auto solo = emptyArena();
    solo.player.position = {3, 3};
    solo.enemies[1] = {{1, 3}, 6};
    solo.endTurn(); solo.stepEnemy();
    expect(solo.enemies[1].position == Position{2, 3} && solo.player.hp == 10, "Solo bandit should use shortest approach");

    auto bounded = emptyArena();
    bounded.player.position = {3, 3};
    bounded.enemies[0] = {{3, 2}, 6};
    bounded.enemies[1] = {{1, 3}, 6};
    bounded.walls[4 * 8 + 2] = true;
    expect(bounded.path({1, 3}, {3, 4}).size() == 6, "Fixture needs a five-step opposite-side route");
    bounded.endTurn(); resolveEnemies(bounded);
    expect(bounded.enemies[1].position == Position{2, 3}, "Bandit should reject a detour longer than shortest plus two");
    expect(bounded.player.hp == 8, "Fallback bandit should still attack when adjacent");

    auto blocked = emptyArena();
    blocked.player.position = {3, 3};
    blocked.enemies[0] = {{3, 2}, 6};
    blocked.enemies[1] = {{1, 3}, 6};
    blocked.walls[4 * 8 + 3] = true;
    blocked.endTurn(); resolveEnemies(blocked);
    expect(blocked.enemies[1].position == Position{2, 3}, "Blocked opposite side must not stall a reachable bandit");
    expectNoOverlap(blocked);

    auto trapped = emptyArena();
    trapped.player.position = {3, 3};
    trapped.enemies[0] = {{3, 2}, 6};
    trapped.enemies[1] = {{6, 6}, 6};
    for (Position p : {Position{6, 5}, Position{7, 6}, Position{6, 7}, Position{5, 6}}) trapped.walls[p.y * 8 + p.x] = true;
    trapped.endTurn(); resolveEnemies(trapped);
    expect(trapped.enemies[1].position == Position{6, 6} && trapped.phase == Phase::PlayerTurn, "Trapped bandit must wait and finish turn");

    auto corner = emptyArena();
    corner.player.position = {0, 0};
    corner.enemies[0] = {{1, 0}, 6};
    corner.enemies[1] = {{0, 3}, 6};
    corner.endTurn(); resolveEnemies(corner);
    expect(corner.enemies[1].position == Position{0, 1} && corner.player.hp == 8, "Corner goals must stay in bounds and use the available attack side");
    expectNoOverlap(corner);
}

void guardConsumesActions() {
    auto game = emptyArena();
    game.enemies[0] = {{1, 5}, 6};
    expect(game.canGuard() && game.tryGuard(), "Guard should be available on a fresh turn");
    expect(game.guarding && game.moved && game.attacked && game.player.hp == 12, "Guard must consume actions without healing");
    expect(!game.canMove() && !game.canAttack(0) && !game.canGuard(), "Guard must close movement and attack");
    expect(!game.tryMove({2, 6}) && !game.tryAttack(0) && !game.tryGuard(), "Cannot act twice after guarding");
    expect(game.enemies[0].hp == 6 && game.player.hp == 12, "Rejected guarded actions mutated HP");

    game.start();
    expect(game.tryMove({2, 6}) && game.tryGuard(), "Move then guard should be allowed");
    expect(game.endTurn() && game.guarding && game.phase == Phase::EnemyTurn, "Ending the turn must retain protection");
}

void guardDamageAndExpiry() {
    auto game = emptyArena();
    game.enemies[0] = {{1, 5}, 6};
    game.enemies[1] = {{2, 6}, 6};
    expect(game.tryGuard() && game.endTurn(), "Guard setup");
    game.stepEnemy();
    expect(game.player.hp == 11 && game.guarding && game.phase == Phase::EnemyTurn, "First hit should deal one and retain protection");
    game.stepEnemy();
    expect(game.player.hp == 10 && !game.guarding && game.phase == Phase::PlayerTurn, "Both enemy hits should be reduced, then protection expires");
    expect(game.canMove() && game.canGuard() && !game.attacked, "Next turn restores actions");
    game.endTurn(); resolveEnemies(game);
    expect(game.player.hp == 6 && !game.guarding, "Expired guard must not reduce next-turn damage");
}

void invalidGuardPhases() {
    Game game;
    expect(!game.canGuard() && !game.tryGuard() && !game.guarding, "Cannot guard on title");
    game.start(); game.endTurn();
    expect(!game.tryGuard() && !game.guarding, "Cannot guard during enemy actions");
    for (const auto phase : {Phase::Won, Phase::Lost}) {
        game.phase = phase;
        expect(!game.tryGuard() && !game.guarding, "Cannot guard after match end");
    }
    game = emptyArena();
    game.enemies[0] = {{1, 5}, 6};
    expect(game.tryAttack(0) && !game.tryGuard() && !game.guarding, "Cannot guard after spending the attack");
}

void guardWithoutDamage() {
    auto game = emptyArena();
    game.enemies[0] = {{6, 1}, 6};
    for (Position p : {Position{6, 0}, Position{7, 1}, Position{6, 2}, Position{5, 1}}) game.walls[p.y * 8 + p.x] = true;
    game.tryGuard(); game.endTurn(); resolveEnemies(game);
    expect(game.player.hp == 12 && !game.guarding && game.canGuard(), "Unused protection still expires at the next player turn");
    game.tryGuard(); game.toTitle();
    expect(game.phase == Phase::Title && !game.guarding, "Returning to title clears guard");
}

void lethalGuardDamage() {
    auto game = emptyArena();
    game.player.hp = 1;
    game.enemies[0] = {{1, 5}, 6};
    game.enemies[1] = {{7, 7}, 6};
    game.tryGuard(); game.endTurn(); game.stepEnemy();
    expect(game.phase == Phase::Lost && game.player.hp == 0 && !game.guarding, "Guard reduces damage but must not grant invulnerability");
    game.stepEnemy();
    expect(game.enemies[1].position == Position{7, 7}, "Lethal guarded damage must stop later enemies");
    game.start();
    expect(game.player.hp == 12 && !game.guarding && game.canGuard(), "Restart after guarded defeat clears protection");
}

void expectPreviewMatches(Game game) {
    const auto intents = game.enemyIntents();
    game.endTurn();
    for (const auto& intent : intents) {
        expect(game.activeEnemy() == intent.enemy, "Preview changed enemy action order");
        const auto before = game;
        game.stepEnemy();
        expect(!intent.route.empty() && intent.route.front() == before.enemies[intent.enemy].position &&
               intent.route.back() == game.enemies[intent.enemy].position && intent.route.size() <= 3,
               "Preview destination differs from actual enemy turn");
        expect(before.player.hp - game.player.hp == intent.damage, "Preview HP loss differs from actual enemy turn");
        for (std::size_t i = 1; i < intent.route.size(); ++i)
            expect(distance(intent.route[i - 1], intent.route[i]) == 1 && !before.wallAt(intent.route[i]) &&
                   !before.occupied(intent.route[i]), "Preview route crosses an obstacle or occupied tile");
    }
    expect(game.phase != Phase::EnemyTurn, "Preview omitted an enemy that would act");
}

void previewIsReadOnly() {
    auto game = emptyArena();
    game.player.position = {3, 3};
    game.enemies[0] = {{3, 2}, 6};
    game.enemies[1] = {{1, 3}, 6};
    const auto before = game;
    const auto intents = game.enemyIntents();
    expect(intents.size() == 2 && intents[0].enemy == 0 && intents[1].enemy == 1, "Preview must retain spawn order");
    expect(intents[0].route.size() == 1 && intents[0].damage == 2, "Adjacent goblin preview");
    expect(intents[1].route.back() == Position{2, 4} && intents[1].damage == 0, "Flanking preview must use actual bandit planner");
    expect(game.phase == before.phase && game.round == before.round && game.enemyIndex == before.enemyIndex &&
           game.moved == before.moved && game.attacked == before.attacked && game.guarding == before.guarding &&
           game.walls == before.walls && game.log == before.log && game.message == before.message,
           "Preview mutated turn state, map, actions or journal");
    expect(game.player.position == before.player.position && game.player.hp == before.player.hp &&
           game.enemies[0].position == before.enemies[0].position && game.enemies[0].hp == before.enemies[0].hp &&
           game.enemies[1].position == before.enemies[1].position && game.enemies[1].hp == before.enemies[1].hp,
           "Preview mutated living units");
    const auto again = game.enemyIntents();
    expect(again[0].route == intents[0].route && again[1].route == intents[1].route &&
           again[0].damage == intents[0].damage && again[1].damage == intents[1].damage, "Preview must be deterministic");
    expectPreviewMatches(game);
}

void previewUpdatesAfterActions() {
    Game game;
    game.start();
    const auto initial = game.enemyIntents();
    expectPreviewMatches(game);
    expect(game.tryMove({4, 6}), "Preview movement fixture");
    const auto moved = game.enemyIntents();
    expect(moved.size() == 2 && moved[1].route != initial[1].route, "Preview did not update after player movement");
    expectPreviewMatches(game);
    game = emptyArena();
    game.enemies[0] = {{1, 5}, 3};
    game.enemies[1] = {{4, 6}, 6};
    expect(game.enemyIntents().size() == 2 && game.tryAttack(0), "Preview kill fixture");
    const auto afterKill = game.enemyIntents();
    expect(afterKill.size() == 1 && afterKill[0].enemy == 1 && afterKill[0].damage == 2,
           "Dead enemy still appears in preview or solo bandit uses stale ally");
    expectPreviewMatches(game);
}

void previewGuardAndLethal() {
    auto game = emptyArena();
    game.enemies[0] = {{1, 5}, 6};
    game.enemies[1] = {{2, 6}, 6};
    auto intents = game.enemyIntents();
    expect(intents.size() == 2 && intents[0].damage == 2 && intents[1].damage == 2, "Unguarded forecast damage");
    expect(game.tryGuard(), "Guard forecast fixture");
    intents = game.enemyIntents();
    expect(intents.size() == 2 && intents[0].damage == 1 && intents[1].damage == 1, "Guard did not update forecast");
    expectPreviewMatches(game);
    game.player.hp = 1;
    intents = game.enemyIntents();
    expect(intents.size() == 1 && intents[0].enemy == 0 && intents[0].damage == 1,
           "Lethal preview must clamp HP loss and cancel later enemy");
    expectPreviewMatches(game);
}

void previewPhasesAndBlocked() {
    Game game;
    for (const auto phase : {Phase::Title, Phase::EnemyTurn, Phase::Won, Phase::Lost}) {
        game.phase = phase;
        expect(game.enemyIntents().empty() && game.phase == phase, "Preview must only exist on player turn");
    }
    game = emptyArena();
    game.player.position = {0, 0};
    game.enemies[1] = {{0, 3}, 6};
    expectPreviewMatches(game);
    game.enemies[1].position = {6, 6};
    for (Position p : {Position{6, 5}, Position{7, 6}, Position{6, 7}, Position{5, 6}}) game.walls[p.y * 8 + p.x] = true;
    const auto intents = game.enemyIntents();
    expect(intents.size() == 1 && intents[0].enemy == 1 && intents[0].route == std::vector<Position>{{6, 6}} &&
           intents[0].damage == 0, "Blocked enemy should preview waiting on its current tile");
    expectPreviewMatches(game);
    game = emptyArena();
    game.player.position = {3, 3};
    game.enemies[0] = {{1, 3}, 6};
    game.walls[3 * 8 + 2] = true;
    const auto detour = game.enemyIntents();
    expect(detour.size() == 1 && detour[0].route == std::vector<Position>{{1, 3}, {1, 2}, {2, 2}},
           "Preview must show the orthogonal route around a wall, not a direct diagonal");
    expectPreviewMatches(game);
}
} // namespace

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests{
        {"initial layout", initialLayout}, {"shortest path and unreachable cells", shortestPath},
        {"movement guards and range", movementGuards}, {"occupied and dead units", occupiedAndDead},
        {"invalid attacks", invalidAttacks}, {"combat and action order", combatAndActionOrder},
        {"winning stops actions", winningStopsActions}, {"phase guards and skip", phaseGuardsAndSkip},
        {"adjacent enemy and lethal damage", adjacentEnemyAndDeath},
        {"enemy movement and deterministic tie break", enemyStepAndTieBreak},
        {"blocked AI and dead-unit skipping", blockedEnemyAndDeadSkip},
        {"complete winning and losing matches", wholeMatches}, {"ten restarts and title", restartAndTitle},
        {"distinct goblin and bandit behavior", distinctEnemyRoles}, {"bandit fallback and detour budget", banditFallbacks},
        {"guard consumes actions", guardConsumesActions}, {"guard damage and expiry", guardDamageAndExpiry},
        {"invalid guard phases", invalidGuardPhases}, {"guard without incoming damage", guardWithoutDamage},
        {"guarded lethal damage", lethalGuardDamage},
        {"read-only deterministic enemy preview", previewIsReadOnly},
        {"preview updates after movement and kills", previewUpdatesAfterActions},
        {"guard and lethal enemy preview", previewGuardAndLethal},
        {"preview phases blocked and corner", previewPhasesAndBlocked}};
    int failures = 0;
    for (const auto& test : tests) {
        try { test.second(); std::cout << "PASS " << test.first << '\n'; }
        catch (const std::exception& e) { ++failures; std::cerr << "FAIL " << test.first << ": " << e.what() << '\n'; }
    }
    std::cout << tests.size() - failures << '/' << tests.size() << " scenarios; " << checks << " checks\n";
    return failures ? 1 : 0;
}
