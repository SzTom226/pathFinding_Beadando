#pragma once
#include <CL/cl.h>

class OpenCLManager {
public:
	bool initialize();

	cl_context getContext() const { return context; }
	cl_command_queue getQueue() const { return queue; }
	cl_device_id getDevice() const { return device; }

	~OpenCLManager();
private:
	cl_platform_id platform = nullptr;
	cl_device_id device = nullptr;
	cl_context context = nullptr;
	cl_command_queue queue = nullptr;
};