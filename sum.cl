__kernel void sum(__global const int *input, __global int *output, const int inputSize, __local int* reductionSums) {
	const int globalID = get_global_id(0);
	const int localID = get_local_id(0);
	const int localSize = get_local_size(0);
	const int workgroupID = globalID / localSize;

	reductionSums[localID] = input[globalID];
	
	for(int offset = localSize / 2; offset > 0; offset /= 2) {
		barrier(CLK_LOCAL_MEM_FENCE);	// wait for all other work-items to finish previous iteration.
		if(localID < offset) {
			reductionSums[localID] += reductionSums[localID + offset];
		}
	}
	
	if(localID == 0) {	// the root of the reduction subtree
		output[workgroupID] = reductionSums[0];
	}
}

__kernel void inefficient(__global const int *input, __global int *output, const int inputSize, __local int* reductionSums) {
	const int globalID = get_global_id(0);
	const int localID = get_local_id(0);
	const int localSize = get_local_size(0);
	const int workgroupID = globalID / localSize;

	reductionSums[localID] = input[globalID];
	
	barrier(CLK_LOCAL_MEM_FENCE); // wait for the rest of the work items to copy the input value to their local memory.
	if(localID == 0) {
		int sum = 0;
		for(int i = 0; i < localSize; i++) {
			if((i+localID) < localSize) {
				sum += reductionSums[i+localID];
			}
		}
		output[workgroupID] = sum;
	}
}
