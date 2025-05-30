import heapq


# Heuristic function (Manhattan distance)
def heuristic(a, b, rows, cols):
    """
    Calculates the shortest distance between two points in a wrapped grid.
    This is a Manhattan distance that considers wrapping around the grid.
    """
    dr = abs(a[0] - b[0])
    dc = abs(a[1] - b[1])

    # Consider wrapping around for rows
    dr_wrapped = min(dr, rows - dr)
    # Consider wrapping around for columns
    dc_wrapped = min(dc, cols - dc)

    return dr_wrapped + dc_wrapped


def astar(grid, start, end):
    # grid: 2D list, 0 for walkable, 1 for obstacle
    # start: (row, col) - wrapped grid coordinates
    # end: (row, col) - wrapped grid coordinates

    rows = len(grid)
    cols = len(grid[0])

    # open_set: Priority queue (f_score, (absolute_row, absolute_col))
    # We need to track absolute coordinates to correctly reconstruct the path across wraps
    open_set = [(0, (start[0], start[1]))]  # Store (f_score, (abs_row, abs_col))

    # g_score: cost from start to current node, mapped by (absolute_row, absolute_col)
    g_score = {(start[0], start[1]): 0}

    # f_score: estimated total cost from start to end through current node, mapped by (absolute_row, absolute_col)
    f_score = {
        (start[0], start[1]): heuristic(start, end, rows, cols)
    }  # Use wrapped heuristic

    # came_from: reconstruct path. Stores (absolute_row, absolute_col) -> (absolute_prev_row, absolute_prev_col)
    came_from = {}

    while open_set:
        current_f, current_abs_node = heapq.heappop(open_set)
        current_wrapped_node = (current_abs_node[0] % rows, current_abs_node[1] % cols)

        # Check if the wrapped version of the current_abs_node is the end node
        if current_wrapped_node == end:
            path = []
            node_to_reconstruct = current_abs_node
            while node_to_reconstruct in came_from:
                path.append(node_to_reconstruct)
                node_to_reconstruct = came_from[node_to_reconstruct]
            path.append(start)  # Add the true start node (which is also absolute)
            path.reverse()
            return path

        # Define neighbors (8 directions)
        for dr, dc in [
            (-1, 0),
            (1, 0),
            (0, -1),
            (0, 1),
            (-1, -1),
            (-1, 1),
            (1, -1),
            (1, 1),
        ]:
            # Calculate the absolute neighbor coordinates
            abs_neighbor_row, abs_neighbor_col = (
                current_abs_node[0] + dr,
                current_abs_node[1] + dc,
            )

            # Calculate the wrapped neighbor coordinates for grid lookup
            wrapped_neighbor_row = abs_neighbor_row % rows
            wrapped_neighbor_col = abs_neighbor_col % cols

            # Check if the wrapped neighbor is within bounds and not an obstacle
            if (
                0 <= wrapped_neighbor_row < rows
                and 0 <= wrapped_neighbor_col < cols
                and grid[wrapped_neighbor_row][wrapped_neighbor_col] == 0
            ):  # 0 for walkable

                # Calculate tentative g_score for the neighbor
                # Diagonal moves have a cost of sqrt(2), cardinal moves have a cost of 1
                tentative_g_score = g_score[current_abs_node] + (
                    1.414 if dr != 0 and dc != 0 else 1
                )

                # If this path to neighbor is better than any previous one
                if tentative_g_score < g_score.get(
                    (abs_neighbor_row, abs_neighbor_col), float("inf")
                ):
                    came_from[(abs_neighbor_row, abs_neighbor_col)] = current_abs_node
                    g_score[(abs_neighbor_row, abs_neighbor_col)] = tentative_g_score
                    f_score[(abs_neighbor_row, abs_neighbor_col)] = (
                        tentative_g_score
                        + heuristic(
                            (abs_neighbor_row, abs_neighbor_col), end, rows, cols
                        )
                    )  # Use wrapped heuristic
                    heapq.heappush(
                        open_set,
                        (
                            f_score[(abs_neighbor_row, abs_neighbor_col)],
                            (abs_neighbor_row, abs_neighbor_col),
                        ),
                    )

    return None  # No path found
