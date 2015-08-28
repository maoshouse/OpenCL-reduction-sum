# OpenCL-reduction-sum
## What?
This program demonstrates the use of OpenCL to sum an array of integers.
## How?
The implementation feeds the compute device with the array of integers. The kernel then iteratively sums pairs and reduces the input.
## Usage
I compiled using clang with framework flag set to opencl. "clang -framework opencl main.c" will do the trick. The program takes one argument, n,
an integer specifying how many elements the input array will contain. This input array is simply the sequence of numbers in the range [0, n).
