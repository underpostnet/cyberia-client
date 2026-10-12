#include "game_render.c"
#include "game_state.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    WorldObject obstacle = { .max_life = 10.0f, .life = 0.0f };
    assert(game_state_obstacle_depleted(&obstacle));
    obstacle.life = 1.0f;
    assert(!game_state_obstacle_depleted(&obstacle));
    obstacle.life = 0.0f;
    obstacle.max_life = 0.0f;
    assert(!game_state_obstacle_depleted(&obstacle));

    assert(0 > compare_depth(10.0f, "z", 11.0f, "a"));
    assert(0 < compare_depth(11.0f, "a", 10.0f, "z"));
    assert(0 > compare_depth(10.0f, "a", 10.0f, "b"));
    assert(0 == compare_depth(10.0f, "a", 10.0f, "a"));

    /* Close depths keep a transitive order, independent of IDs. */
    EntitySortEntry entries[] = {
        { .depth_y = 10.0015f, .sort_id = "a" },
        { .depth_y = 10.0008f, .sort_id = "b" },
        { .depth_y = 10.0f, .sort_id = "c" },
    };
    const int permutations[][3] = {
        {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0},
    };
    for (int i = 0; 6 > i; i++) {
        EntitySortEntry sorted[3];
        for (int j = 0; 3 > j; j++) sorted[j] = entries[permutations[i][j]];
        qsort(sorted, 3, sizeof sorted[0], compare_entities_by_depth);
        assert(0 == strcmp("c", sorted[0].sort_id));
        assert(0 == strcmp("b", sorted[1].sort_id));
        assert(0 == strcmp("a", sorted[2].sort_id));
    }

    /* Equal depths keep the same order across snapshot permutations. */
    entries[0].depth_y = entries[1].depth_y = entries[2].depth_y = 10.0f;
    qsort(entries, 3, sizeof entries[0], compare_entities_by_depth);
    assert(0 == strcmp("a", entries[0].sort_id));
    assert(0 == strcmp("b", entries[1].sort_id));
    assert(0 == strcmp("c", entries[2].sort_id));

    puts("world_depth_test OK");
    return 0;
}
