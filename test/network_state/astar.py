# Credit for this: Nicholas Swift
# as found at https://medium.com/@nicholas.w.swift/easy-a-star-pathfinding-7e6689c7f7b2
from warnings import warn
import heapq


class Node:
    """
    A node class for A* Pathfinding algorithm.
    """

    def __init__(self, parent=None, position=None):
        self.parent = parent
        self.position = position  # (row, col)

        self.g = 0  # Cost from start to current node
        self.h = 0  # Heuristic cost from current node to end node
        self.f = 0  # Total cost (g + h)

    def __eq__(self, other):
        return self.position == other.position

    def __repr__(self):
        return f"{self.position} - g: {self.g} h: {self.h} f: {self.f}"

    # Define less than and greater than for heap queue operations
    def __lt__(self, other):
        return self.f < other.f

    def __gt__(self, other):
        return self.f > other.f


def return_path(current_node):
    """Reconstructs the path from the end node back to the start node."""
    path = []
    current = current_node
    while current is not None:
        path.append(current.position)
        current = current.parent
    return path[::-1]  # Return reversed path to get it from start to end


def astar(maze, start, end, allow_diagonal_movement=False):
    """
    Returns a list of tuples (row, col) as a path from the given start to the given end in the given maze.
    :param maze: A 2D list (grid) where 0 is walkable and 1 is an obstacle.
    :param start: A tuple (row, col) representing the start position.
    :param end: A tuple (row, col) representing the end position.
    :param allow_diagonal_movement: Boolean to allow diagonal movement.
    :return: A list of tuples (row, col) representing the path, or None if no path is found.
    """

    start_node = Node(None, start)
    start_node.g = start_node.h = start_node.f = 0
    end_node = Node(None, end)
    end_node.g = end_node.h = end_node.f = 0

    open_list = []  # Priority queue for nodes to be evaluated
    closed_list = []  # List of nodes already evaluated

    heapq.heapify(open_list)
    heapq.heappush(open_list, start_node)

    max_iterations = len(maze[0]) * len(maze) * 2  # Safety break for infinite loops
    outer_iterations = 0

    # Define possible movements (up, down, left, right)
    adjacent_squares = (
        (0, -1),  # Up
        (0, 1),  # Down
        (-1, 0),  # Left
        (1, 0),  # Right
    )
    if allow_diagonal_movement:
        # Add diagonal movements
        adjacent_squares += (
            (-1, -1),  # Up-Left
            (-1, 1),  # Down-Left
            (1, -1),  # Up-Right
            (1, 1),  # Down-Right
        )

    while len(open_list) > 0:
        outer_iterations += 1
        if outer_iterations > max_iterations:
            warn("A* pathfinding exceeded max iterations. Returning partial path.")
            return return_path(current_node)

        current_node = heapq.heappop(open_list)
        closed_list.append(current_node)

        if current_node == end_node:
            return return_path(current_node)

        children = []
        for new_position in adjacent_squares:
            node_position = (
                current_node.position[0] + new_position[0],
                current_node.position[1] + new_position[1],
            )

            # Check if within maze boundaries
            if not (
                0 <= node_position[0] < len(maze)
                and 0 <= node_position[1] < len(maze[0])
            ):
                continue

            # Check if walkable terrain (0 means walkable)
            if maze[node_position[0]][node_position[1]] != 0:
                continue

            new_node = Node(current_node, node_position)
            children.append(new_node)

        for child in children:
            # Child is already in the closed list
            if child in closed_list:
                continue

            # Calculate f, g, and h values
            child.g = current_node.g + 1
            # Heuristic: Euclidean distance squared
            child.h = ((child.position[0] - end_node.position[0]) ** 2) + (
                (child.position[1] - end_node.position[1]) ** 2
            )
            child.f = child.g + child.h

            # Child is already in the open list and has a worse G score
            if any(
                child.position == open_node.position and child.g >= open_node.g
                for open_node in open_list
            ):
                continue

            heapq.heappush(open_list, child)

    warn("Couldn't get a path to destination.")
    return None
