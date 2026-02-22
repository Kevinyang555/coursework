# CS 225: Data Structures

**Fall 2024** | **University of Illinois Urbana-Champaign**

## Course Information

**Credits:** 4 credit hours
**Language:** C++ (C++17)

### Official Description

Data abstractions: stacks, queues, linked structures, binary trees, hash tables, graphs. Algorithm analysis: asymptotic notation, recursion, sorting. Object-oriented programming with inheritance and polymorphism.

## Assignments

### Labs
| Lab | Topic |
|-----|-------|
| lab_debug | Debugging exercise (find/fix C++ bugs in image processing) |
| lab_memory | Rule of Three (copy constructor, assignment operator, destructor) |
| lab_quacks | Recursion with stacks and queues |
| lab_bst | Binary search tree (insert, find, remove) |
| lab_avl | AVL tree with rotations and balancing |
| lab_hash | Hash tables with double hashing and linear probing |
| lab_heaps | Priority queues and heaps |
| lab_huffman | Huffman encoding trees |
| lab_btree | B-trees |
| lab_bloom | Bloom filters |
| lab_dict | Dictionary structures |
| lab_flow | Network flow algorithms |
| lab_ml | Graph-based machine learning |

### Machine Projects
| MP | Topic |
|----|-------|
| mp_stickers | Image manipulation and sticker compositing |
| mp_lists | Doubly-linked list with merge sort |
| mp_traversals | BFS/DFS image traversals |
| mp_mosaics | KD-tree nearest neighbor for photomosaics |
| mp_mazes | Disjoint sets for maze generation and BFS solving |
| mp_puzzle | State space search for puzzle solving |

## Building & Testing

All assignments use CMake with Catch2 tests. Compiler: `clang++` (C++17).

```bash
cd <assignment>
mkdir build && cd build
cmake ..
make
./test          # run Catch2 tests
./<entrypoint>  # run demo binary
```

Entrypoint names are defined in each `assignment.cmake` file.

---

CS 225 @ UIUC | Fall 2024
