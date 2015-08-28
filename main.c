#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

#define MEM_SIZE 128
#define MAX_SOURCE_SIZE 0x100000
#define DEVICE_GROUP_SIZE 64

void error_handler(char err[], int code) {
	if(code != CL_SUCCESS) {
		printf("%s, Error code:%d\n", err, code);
		exit(EXIT_FAILURE);
	}
}

// method is broken! realloc when implemented here messes the values up!
void resize_array(int* orig, int newSize) {
	printf("Resizeing to: %lu\n", sizeof(*orig) * newSize);
	int *temp = realloc(orig, sizeof(*orig) * newSize);

	if(temp == NULL) {
		puts("resize failed");
		exit(EXIT_FAILURE);
	}
	else{
		orig = temp;
	}
	
}

int main(int argc, char * argv[]) {
	int ret;

	int inputSize = atoi(argv[1]);
	printf("Specified input array will have %d items\n", inputSize);
	printf("The program will sum the integers in the range [0, %d)\n", inputSize);
	printf("==============\n");
	/* create input data */
	int* input = malloc(sizeof(*input) * inputSize);
	for(int i = 0; i < inputSize; i++) {
		input[i] = i;
	}

	int sizeDiff = inputSize - DEVICE_GROUP_SIZE;
	int multiplier = 1;
	if(sizeDiff < 0) { // inputSize is smaller than DEVICE_GROUP_SIZE
		int *temp = realloc(input, multiplier * DEVICE_GROUP_SIZE * sizeof(*input));
		input = temp;
//		resize_array(input, DEVICE_GROUP_SIZE);
	}
	else if(sizeDiff > 0) {
		multiplier = ((inputSize % DEVICE_GROUP_SIZE) == 0) ? inputSize / DEVICE_GROUP_SIZE : (inputSize / DEVICE_GROUP_SIZE) +1;
		int *temp = realloc(input, multiplier * DEVICE_GROUP_SIZE * sizeof(*input));
		input = temp;
//		resize_array(input, multiplier * DEVICE_GROUP_SIZE);
	}

	for(int i = 0; i < abs(sizeDiff); i++) {
		input[inputSize + i] = 0;
	}

	inputSize = multiplier * DEVICE_GROUP_SIZE;

	/* define platform */
	cl_platform_id platformID;
	ret = clGetPlatformIDs(1, &platformID, NULL);
	error_handler("Get platform error", ret);	

	/* define device */
	cl_device_id deviceID;
	ret = clGetDeviceIDs(platformID, CL_DEVICE_TYPE_GPU, 1, &deviceID, NULL);
	error_handler("Get deviceID error", ret);

	cl_char vendorName[1024] = {0};
	cl_char deviceName[1024] = {0};
	ret = clGetDeviceInfo(deviceID, CL_DEVICE_VENDOR, sizeof(vendorName), vendorName, NULL);
	ret = clGetDeviceInfo(deviceID, CL_DEVICE_NAME, sizeof(deviceName), deviceName, NULL);
	printf("Connecting to %s %s...\n", vendorName, deviceName);

	/* define context */
	cl_context context;
	context = clCreateContext(NULL, 1, &deviceID, NULL, NULL, &ret);
	error_handler("define context error", ret);

	/* define command queue */
	cl_command_queue commandQueue;
	commandQueue = clCreateCommandQueue(context, deviceID, 0, &ret);
	error_handler("Create command queue error", ret);

	/* create memory objects */
	cl_mem inputMemObj;
	inputMemObj = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int)*inputSize, NULL, &ret);
	error_handler("Create input buffer failed", ret);
	ret = clEnqueueWriteBuffer(commandQueue, inputMemObj, CL_TRUE, 0, sizeof(int)*inputSize, (const void*)input, 0, NULL, NULL);
	error_handler("Write to input buffer failed", ret);

	// output need only be an array of inputSize / DEVICE_GROUP_SIZE long
	cl_mem outputMemObj;
	outputMemObj = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int)*multiplier, NULL, &ret);
	error_handler("Create output buffer failed", ret);

	/* read kernel file from source */
	char fileName[] = "./sum.cl";
	char *sourceStr;	
	size_t sourceSize;

	FILE *fp = fopen(fileName, "r");
	if(!fp) {
		puts("Failed to load kernel file");
		exit(EXIT_FAILURE);
	}
	sourceStr = (char*)malloc(MAX_SOURCE_SIZE);
	sourceSize = fread(sourceStr, 1, MAX_SOURCE_SIZE, fp);
	fclose(fp);

	
	/* create program object */
	cl_program program = clCreateProgramWithSource(context, 1, (const char**)&sourceStr, (const size_t*)&sourceSize, &ret);	
	error_handler("create program failure", ret);
	ret = clBuildProgram(program, 1, &deviceID, NULL, NULL, NULL);
	if(ret != CL_SUCCESS) {
		puts("Build program error");
		size_t len;
		char buildLog[2048];
		clGetProgramBuildInfo(program, deviceID, CL_PROGRAM_BUILD_LOG, sizeof(buildLog), buildLog, &len);
		printf("%s\n", buildLog);
	}

	/* create kernel */
	cl_kernel kernel = clCreateKernel(program, "sum", &ret);
	error_handler("Create kernel failure", ret);

	/* set kernel arguments */
	ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&inputMemObj);
	error_handler("Set arg 1 failure", ret);
	ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&outputMemObj);
	error_handler("Set arg 2 failure", ret);
	ret = clSetKernelArg(kernel, 2, sizeof(int), (void *)&inputSize);
	error_handler("Set arg 3 failure", ret);
	ret = clSetKernelArg(kernel, 3, sizeof(int) * DEVICE_GROUP_SIZE, NULL);
	error_handler("Set arg 4 failure", ret);
	/* enqueue and execute */
	const size_t globalWorkSize = inputSize;
	const size_t localWorkSize = DEVICE_GROUP_SIZE; //inputSize / DEVICE_GROUP_SIZE; // in addition, we need work group size to be at least 2... figure this one out later
	ret = clEnqueueNDRangeKernel(commandQueue, kernel, 1, NULL, &globalWorkSize, &localWorkSize, 0, NULL, NULL);
	error_handler("Enqueue/execute failure", ret);
	
	/* read results from output buffer */
	int * results = (int*) malloc(sizeof(int)*multiplier);
	ret = clEnqueueReadBuffer(commandQueue, outputMemObj, CL_TRUE, 0, sizeof(int)*multiplier, results, 0, NULL, NULL);
	error_handler("Read output buffer fail", ret);

	int sum = 0;
	for(int i = 0; i < multiplier; i++) {
		sum += results[i];
	}
	printf("==============\n");
	printf("The sum of the array is: %d\n", sum);
		
	/* release */
	ret = clReleaseKernel(kernel);
	ret = clReleaseProgram(program);
	ret = clReleaseMemObject(inputMemObj);
	ret = clReleaseMemObject(outputMemObj);
	ret = clReleaseCommandQueue(commandQueue);
	ret = clReleaseContext(context);	
	exit(EXIT_SUCCESS);
}
