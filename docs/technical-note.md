# BFS and turn management

Turnforge has a fixed 8x8 board with four-direction movement and uniform step costs. Breadth-first search gives a shortest valid route without the additional machinery of A*. At most 64 cells are visited per search.

## One source of occupancy

`Game::occupied` derives tile occupancy directly from the positions of living units. There is no separate grid of unit IDs to update after movement or death. The BFS marks the origin as visited before exploring: a unit may start on its own occupied tile, but every later cell must be free and within the map.

The search records distance and parent for each cell. Distances power movement highlights; the parent chain reconstructs a route. The current tile has distance zero and does not consume a move when clicked. Invalid moves do not mutate positions or action allowances.

## Finding an enemy destination

The player tile remains occupied, so enemies do not search for a route onto it. Instead, BFS computes routes to the empty orthogonal neighbors of the player. Candidate goals and BFS neighbors use the fixed order up, right, down, left. An enemy already next to the player attacks without moving.

The goblin selects the shortest route, keeping the first candidate on an equal distance. In v0.3, the bandit uses those same four candidate routes to look for a flank while the goblin is alive:

1. Find the shortest candidate route length.
2. Exclude candidates more than two steps longer than that route.
3. Prefer the goal with the greatest squared distance from the goblin; this distinguishes an opposite-side tile from a perpendicular one without floating-point math.
4. Break equal spacing by shorter path, then the existing candidate order.

The bandit still advances only two tiles and may attack after moving. It does not teleport to the chosen goal or reserve it across turns. With a dead goblin it falls back to the shortest route; with no route it waits. Squared spacing only scores cached candidate paths, so the flank preference does not add another set of BFS searches.

![Distinct enemy tactics](images/06-ai-musuh.png)

*Figure 6. Tested example: direct goblin approach versus a bandit approaching the opposite side.*

Enemies act in stable spawn order. Each action performs a fresh search using current positions, preventing the second enemy from walking onto the first. If all routes are blocked, the enemy waits and the turn continues. A two-step move may be followed by one attack if the enemy is now adjacent.

## Separating rules from presentation

The core exposes `tryMove`, `tryAttack`, `tryGuard`, `endTurn`, and `stepEnemy`. The game window maps input to these actions, renders the resulting state, and waits approximately 300 ms between enemy actions with a non-blocking timer. Frame rate never changes damage, movement range, or action count.

A player attack consumes both attack and remaining movement. Terminal transitions happen immediately: killing the final enemy enters `Won`; a lethal enemy attack enters `Lost` and stops remaining enemies. `start()` replaces the complete session with its initial state, including map, HP, action flags, enemy index, round, and journal. The presentation timer is reset by the controller when starting, restarting, returning to title, or ending a player turn.

In v0.2, `Characters` observes model changes and animates a three-character 2D atlas. Movement follows a reconstructed orthogonal route, not a straight line through walls. Actions pause until movement, attack, or death presentation finishes, while Escape can still return to title. The terminal model state remains immediate; the result overlay waits for the finishing animation. Enemy presentation waits approximately 300 ms before the next action after the preceding animation completes. Damage, range, pathfinding, and turn order remain in the original core.

## Guard lifecycle — v0.4

One `guarding` flag belongs to the gameplay model. `tryGuard()` requires the player's phase and an unused attack, then consumes the attack and remaining movement. Movement before Guard is legal. Ending the turn retains protection; every enemy hit uses damage 1 instead of 2. `finishEnemyTurn()` clears it before restoring the player's actions, including a phase without incoming damage. Lethal damage, a new match, and returning to title clear it too.

The renderer stores the observed HP delta for each hit. This keeps the floating damage correct when the final enemy's hit also clears Guard before the renderer observes the model. The brief shield hit effect uses that recorded damage; it does not extend the model's protection.

## Evidence

The v0.4 suite totals 20 scenarios / 268 checks. Guard cases cover action consumption, move-then-guard, two reduced hits, expiry with and without hits, subsequent full damage, invalid phases, attack conflicts, title/restart cleanup, and lethal damage stopping later enemies. The renderer self-check verifies reduced damage after protection has expired.

The automated core suite covers shortest-route detours, unreachable cells, invalid coordinates and targets, occupancy after death, action ordering, deterministic tie breaks, blocked AI, death mid-turn, a complete winning strategy, an idle losing match, and ten successive resets. v0.3 adds direct-versus-flanking behavior, opposite attack sides, deterministic flank decisions, a bounded detour, and dead-ally/adjacent/blocked/corner cases. These checks run without raylib and stay enabled in optimized builds.

On the original map, a verified v0.3 winning opening is moving from `(1, 6)` to `(4, 6)`, then attacking the lowest-HP adjacent living enemy once each turn and ending the turn. The automated demo wins in round 6 with 2 HP remaining. Splitting damage using the old fixed enemy-index priority can now lose, so target selection matters. This demonstrates that the initial map remains winnable; it is not a broad balance study or evidence of a particular playtime.
