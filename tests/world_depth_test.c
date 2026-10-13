#include "view/game_render.c"
#include "sim/game_state.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    Vitals obstacle = { .max_life = 10, .life = 0 };
    assert(game_state_obstacle_depleted(&obstacle));
    obstacle.life = 1;
    assert(!game_state_obstacle_depleted(&obstacle));
    obstacle.life = 0;
    obstacle.max_life = 0;
    assert(!game_state_obstacle_depleted(&obstacle));

    assert(0 > compare_depth(10.0f, 26, 11.0f, 1));
    assert(0 < compare_depth(11.0f, 1, 10.0f, 26));
    assert(0 > compare_depth(10.0f, 1, 10.0f, 2));
    assert(0 == compare_depth(10.0f, 1, 10.0f, 1));

    /* Close depths keep a transitive order, independent of IDs. */
    EntitySortEntry entries[] = {
        { .depth_y = 10.0015f, .sort_id = 1 },
        { .depth_y = 10.0008f, .sort_id = 2 },
        { .depth_y = 10.0f, .sort_id = 3 },
    };
    const int permutations[][3] = {
        {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0},
    };
    for (int i = 0; 6 > i; i++) {
        EntitySortEntry sorted[3];
        for (int j = 0; 3 > j; j++) sorted[j] = entries[permutations[i][j]];
        qsort(sorted, 3, sizeof sorted[0], compare_entities_by_depth);
        assert(3 == sorted[0].sort_id);
        assert(2 == sorted[1].sort_id);
        assert(1 == sorted[2].sort_id);
    }

    /* Equal depths keep the same order across snapshot permutations. */
    entries[0].depth_y = entries[1].depth_y = entries[2].depth_y = 10.0f;
    qsort(entries, 3, sizeof entries[0], compare_entities_by_depth);
    assert(1 == entries[0].sort_id);
    assert(2 == entries[1].sort_id);
    assert(3 == entries[2].sort_id);

    puts("world_depth_test OK");
    return 0;
}
