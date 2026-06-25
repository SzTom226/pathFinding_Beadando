#include "OpenCLManager.h"
#include <iostream>
#include <string>

// Platform-specifikus GL interop headerek
#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#elif defined(__APPLE__)
#include <OpenGL/OpenGL.h>
#else
#include <GL/glx.h>
#endif

#include <CL/cl_gl.h>

static bool supportsGLSharing(cl_device_id dev)
{
    size_t n = 0;
    clGetDeviceInfo(dev, CL_DEVICE_EXTENSIONS, 0, nullptr, &n);
    std::string ext(n, '\0');
    clGetDeviceInfo(dev, CL_DEVICE_EXTENSIONS, n, ext.data(), nullptr);
    return ext.find("cl_khr_gl_sharing") != std::string::npos;
}

bool OpenCLManager::initialize()
{
    cl_int err;
    cl_uint platformCount = 0;
    clGetPlatformIDs(0, nullptr, &platformCount);

    if (platformCount == 0) {
        std::cout << "No OpenCL platform found\n";
        return false;
    }

    cl_platform_id* platforms = new cl_platform_id[platformCount];
    clGetPlatformIDs(platformCount, platforms, nullptr);
    platform = platforms[0];
    delete[] platforms;

    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);

    char name[256];
    clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(name), name, nullptr);
    std::cout << "OpenCL device: " << name << "\n";

    if (!supportsGLSharing(device)) {
        std::cout << "HIBA: cl_khr_gl_sharing nem támogatott ezen az eszközön!\n";
        return false;
    }

    // GL-sharing context properties –
    // Az aktuális OpenGL context-et kötjük be, hogy ugyanaz a GPU memória legyen.
#ifdef _WIN32
    cl_context_properties props[] = {
        CL_GL_CONTEXT_KHR,  (cl_context_properties)wglGetCurrentContext(),
        CL_WGL_HDC_KHR,     (cl_context_properties)wglGetCurrentDC(),
        CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
        0
    };
#elif defined(__APPLE__)
    CGLContextObj glCtx = CGLGetCurrentContext();
    cl_context_properties props[] = {
        CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
        (cl_context_properties)CGLGetShareGroup(glCtx),
        0
    };
#else
    cl_context_properties props[] = {
        CL_GL_CONTEXT_KHR,  (cl_context_properties)glXGetCurrentContext(),
        CL_GLX_DISPLAY_KHR, (cl_context_properties)glXGetCurrentDisplay(),
        CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
        0
    };
#endif

    context = clCreateContext(props, 1, &device, nullptr, nullptr, &err);
    if (err != CL_SUCCESS) {
        std::cout << "clCreateContext (GL sharing) failed: " << err << "\n";
        return false;
    }

#if CL_TARGET_OPENCL_VERSION >= 200
    queue = clCreateCommandQueueWithProperties(context, device, nullptr, &err);
#else
    queue = clCreateCommandQueue(context, device, 0, &err);
#endif

    if (err != CL_SUCCESS) {
        std::cout << "clCreateCommandQueue failed: " << err << "\n";
        return false;
    }

    std::cout << "OpenCL initialized with GL sharing support\n";
    return true;
}

OpenCLManager::~OpenCLManager()
{
    if (queue)   clReleaseCommandQueue(queue);
    if (context) clReleaseContext(context);
}