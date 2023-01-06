# MultithreadedHashing
I used multithreading to find unique and deterministic (i.e. replicable) hash values for specified input files (placed in the same directory as the project). 

## Custom number of threads and other input
The program take the file name and the number of threads as input. Run using MTHashing [filename] [numberOfThreads].   

## Use of a Binary Tree
I used a binary hash tree to find hash values for each block and then combine and rehash these recursively until a single hash value is created for the final file. 
