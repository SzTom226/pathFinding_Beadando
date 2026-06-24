#include "OpenCLManager.h"
#include <iostream>

bool OpenCLManager::initialize() {
	cl_int err;
	cl_uint platformCount = 0;

	err = clGetPlatformIDs(0, nullptr, &platformCount);

	if (err != CL_SUCCESS || platformCount == 0)
	{
		std::cout << "No OpenCL platform found\n";
		return false;
	}

	std::cout << "Platforms found: " << platformCount << "\n";

	cl_platform_id* platforms = new cl_platform_id[platformCount];
	clGetPlatformIDs(platformCount, platforms, nullptr);
	platform = platforms[0];
	delete[] platforms;

	err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);

	if (err != CL_SUCCESS)
	{
		std::cout << "No GPU device found\n";
		return false;
	}

	char deviceName[256];
	clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(deviceName), deviceName, nullptr);

	std::cout << "Device: " << deviceName << "\n";

	context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
	if (err != CL_SUCCESS)
	{
		std::cout << "Failed creating context\n";
	}
#if CL_TARGET_OPENCL_VERSION >= 200
	queue = clCreateCommandQueueWithProperties(context, device, nullptr, &err);
#else
	queue = clCreateCommandQueue(context, device, 0, &err);
#endif

	if (err != CL_SUCCESS)
	{
		std::cout << "Failed creating queue\n";
		return false;
	}
	std::cout << "OpenCL initialized successfully\n";
	return true;
}

OpenCLManager::~OpenCLManager() {
	if (queue)
	{
		clReleaseCommandQueue(queue);
	}
	if (context)
	{
		clReleaseContext(context);
	}
}