#include "OpenCLManager.h"
#include <iostream>
#include <string>

// Platform-specifikus GL interop headerek:
// A megosztott context létrehozásához az aktuális GL context handle-jét kell
// átadni, amelynek elérése platformonként különböző API-n keresztül történik.
#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#elif defined(__APPLE__)
#include <OpenGL/OpenGL.h>
#else
#include <GL/glx.h>
#endif

#include <CL/cl_gl.h>

// Ellenőrzi, hogy az adott OpenCL eszköz támogatja-e a cl_khr_gl_sharing
// kiterjesztést, amely az OpenGL–OpenCL megosztott memóriához szükséges.
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

    // Az elérhető platformok számának lekérdezése
    cl_uint platformCount = 0;
    clGetPlatformIDs(0, nullptr, &platformCount);

    if (platformCount == 0) {
        std::cout << "No OpenCL platform found\n";
        return false;
    }

    // Az első platform kiválasztása (általában az elsődleges GPU gyártójáé)
    cl_platform_id* platforms = new cl_platform_id[platformCount];
    clGetPlatformIDs(platformCount, platforms, nullptr);
    platform = platforms[0];
    delete[] platforms;

    // Az első elérhető GPU eszköz lekérdezése a platformról
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);

    // Az eszköz nevének kiírása diagnosztikához
    char name[256];
    clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(name), name, nullptr);
    std::cout << "OpenCL device: " << name << "\n";

    // GL sharing nélkül nem lehet a textúrát megosztani OpenCL és OpenGL között
    if (!supportsGLSharing(device)) {
        std::cout << "HIBA: cl_khr_gl_sharing nem támogatott ezen az eszközön!\n";
        return false;
    }

    // GL-sharing context properties:
    // Az aktuális OpenGL context handle-jét adjuk át, hogy az OpenCL ugyanazon
    // a GPU memórián dolgozzon, mint az OpenGL – így nincs szükség CPU roundtripre.
    // A properties tömb platformonként eltérő struktúrájú.
#ifdef _WIN32
    cl_context_properties props[] = {
        CL_GL_CONTEXT_KHR,   (cl_context_properties)wglGetCurrentContext(),
        CL_WGL_HDC_KHR,      (cl_context_properties)wglGetCurrentDC(),
        CL_CONTEXT_PLATFORM,  (cl_context_properties)platform,
        0
    };
#elif defined(__APPLE__)
    // macOS-en a CGL sharegroup-on keresztül történik az összekötés
    CGLContextObj glCtx = CGLGetCurrentContext();
    cl_context_properties props[] = {
        CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
        (cl_context_properties)CGLGetShareGroup(glCtx),
        0
    };
#else
    // Linux/X11: GLX context és display szükséges
    cl_context_properties props[] = {
        CL_GL_CONTEXT_KHR,   (cl_context_properties)glXGetCurrentContext(),
        CL_GLX_DISPLAY_KHR,  (cl_context_properties)glXGetCurrentDisplay(),
        CL_CONTEXT_PLATFORM,  (cl_context_properties)platform,
        0
    };
#endif

    // GL-sharing context létrehozása – ez teszi lehetővé a clCreateFromGLTexture hívást
    context = clCreateContext(props, 1, &device, nullptr, nullptr, &err);
    if (err != CL_SUCCESS) {
        std::cout << "clCreateContext (GL sharing) failed: " << err << "\n";
        return false;
    }

    // Parancsor létrehozása – OpenCL 2.0+ és régebbi API is támogatott
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

// A parancssort előbb kell felszabadítani, mint a contextet,
// mert a queue a contexthez tartozó erőforrás.
OpenCLManager::~OpenCLManager()
{
    if (queue)   clReleaseCommandQueue(queue);
    if (context) clReleaseContext(context);
}