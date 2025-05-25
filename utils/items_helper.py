# Helper function to add padding to a matrix
def add_padding(matrix: list[list[int]], padding: int) -> list[list[int]]:
    """
    Adds a border of zero-padding around a given 2D matrix.

    Args:
        matrix (list[list[int]]): The input 2D matrix (list of lists of integers).
        padding (int): The number of zero-padded rows/columns to add on each side.

    Returns:
        list[list[int]]: A new matrix with the specified padding applied.
                         Returns an empty list if the input matrix is empty.
    """
    if not matrix:
        return []

    rows = len(matrix)
    cols = len(matrix[0])

    new_rows = rows + 2 * padding
    new_cols = cols + 2 * padding

    # Create a new matrix filled with zeros
    new_matrix = [[0 for _ in range(new_cols)] for _ in range(new_rows)]

    # Copy the original matrix into the center of the new, padded matrix
    for r in range(rows):
        for c in range(cols):
            new_matrix[r + padding][c + padding] = matrix[r][c]
    return new_matrix
