## Sparse Linear Solver
Usage: `linear\_solver.exe FILENAME`

### Input File Format
format: [float, double]  
solver: [conjugate\_gradients, conjugate\_directions, steepes\_descent]  
matrix: [number of nonzero entries]  
[row] [col] [val]  
[row] [col] [val]  
[row] [col] [val]  
[row] [col] [val]  
...  
vector: [number of entries]  
[index] [val]  
[index] [val]  
...  

__Example__:  
```
format: float
solver: conjugate_gradients
matrix: 4
0 0 3
0 1 2
1 0 2
1 1 6
vector: 2
2
-8
```
