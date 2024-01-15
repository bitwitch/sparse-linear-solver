import os
import sys

from scipy.sparse import diags
from scipy.sparse.linalg import cg
import numpy as np

def write_test_file(path, A, b, sol):
    with open(path, "w") as f:
        f.write("format: float\n")
        f.write("solver: conjugate_gradients\n")
        f.write(f"matrix: {len(A.data)}\n")
        for i, val in enumerate(A.data):
            f.write(f"{A.row[i]} {A.col[i]} {val}\n")
        f.write(f"vector: {len(b)}\n")
        for val in b:
            f.write(f"{val} ")
        f.write("\n")
        f.write(f"solution: {len(sol)}\n")
        for val in sol:
            f.write(f"{val} ")
        f.write("\n")

def generate_test(path, diag_values, vec_size, max_iterations) -> bool:
    assert len(diag_values) % 2 == 1, "the length of diag_values must be odd"

    half_len = len(diag_values) // 2
    offsets = list(range(-half_len, half_len+1))

    data = []
    for i, val in enumerate(diag_values):
        offset_mag = abs(offsets[i])
        data.append(val * np.ones(vec_size - offset_mag))

    A = diags(data, offsets=offsets, shape=(vec_size, vec_size), format='coo')

    # ensure matrix is positive definite by performing cholesky decomposition
    # try:
        # np.linalg.cholesky(A.todense())
    # except np.linalg.LinAlgError as e:
        # return False

    b = np.random.randn(vec_size)
    sol, _ = cg(A, b, tol=1E-6, maxiter=max_iterations)

    write_test_file(path, A, b, sol)

    return True


if __name__ == "__main__":
    rng = np.random.default_rng()

    if len(sys.argv) < 4:
        print(f"Usage: python {sys.argv[0]} [output_directory] [num_tests] [vec_size]")
        sys.exit(1)

    base_dir = sys.argv[1]
    num_tests = int(sys.argv[2])
    vec_size = int(sys.argv[3])

    os.makedirs(base_dir, exist_ok=True)

    cur_test = 0
    print(f"Generating {num_tests} tests...")
    while cur_test < num_tests:
        max_iterations = 1000
        path = f"{base_dir}/test_{cur_test}.txt"

        # pick an odd number of diagonals
        odd_min = 1
        odd_max = 13
        diag_count = 0
        while (diag_count % 2 != 1):
            diag_count = rng.integers(low=odd_min, high=odd_max, endpoint=True)

        # create that many diagonals, using random floats (ensure symmetric matrix) 
        # NOTE(shaw): the main diagonal entry is scaled up while the other
        # offset diagonals are scaled down to increase probability that the
        # matrix is positive-definite
        cur_diag_count_success = False
        while (cur_diag_count_success != True):
            max_scale = 13
            diag_values = np.empty(diag_count)
            half_count = diag_count // 2
            for i in range(half_count):
                val = (rng.random() - 0.5) * 0.5 * max_scale
                diag_values[i] = val
                diag_values[diag_count - 1 - i] = val
            diag_values[half_count] = (rng.random()+5) * max_scale

            cur_diag_count_success = generate_test(path, diag_values, vec_size, max_iterations)

        print(f"\tgenerated test {cur_test}")
        cur_test += 1

    print("Complete.")
