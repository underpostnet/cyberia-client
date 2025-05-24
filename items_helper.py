# Helper function to add padding to a matrix
def add_padding(matrix, padding):
    if not matrix:
        return []

    rows = len(matrix)
    cols = len(matrix[0])

    new_rows = rows + 2 * padding
    new_cols = cols + 2 * padding

    new_matrix = [[0 for _ in range(new_cols)] for _ in range(new_rows)]

    for r in range(rows):
        for c in range(cols):
            new_matrix[r + padding][c + padding] = matrix[r][c]
    return new_matrix
