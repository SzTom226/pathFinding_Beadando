#include <CL/cl.h>
#include <iostream>

int main() {
    cl_uint platformCount = 0;
    clGetPlatformIDs(1, nullptr, &platformCount);

    cl_platform_id platform;
    clGetPlatformIDs(1, &platform, nullptr);

    cl_uint deviceCount = 0;
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 1, nullptr, &deviceCount);

    cl_device_id device;
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 1, &device, nullptr);

    char name[128];
    clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(name), name, nullptr);

    std::cout << "Device: " << name << std::endl;

    cl_uint computeUnits;
    clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS,
        sizeof(computeUnits), &computeUnits, nullptr);

    std::cout << "Compute Units: " << computeUnits << std::endl;

    return 0;
}