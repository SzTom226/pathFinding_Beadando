#pragma once
#include <CL/cl.h>

// Kezeli az OpenCL platform, eszköz, context és parancssort (queue) létrehozását,
// valamint az OpenGL–OpenCL interophoz szükséges megosztott context beállítását.
class OpenCLManager {
public:
    // Inicializálja az OpenCL-t: megkeresi az első platformot és GPU eszközt,
    // ellenőrzi a cl_khr_gl_sharing támogatást, majd létrehozza a GL-sharing
    // contextet és a parancssort. Visszatér false-szal, ha bármelyik lépés sikertelen.
    // FONTOS: hívása előtt az OpenGL contextnek már léteznie kell (glfwMakeContextCurrent).
    bool initialize();

    // Az OpenCL context handle – kernel és buffer létrehozáshoz szükséges.
    cl_context       getContext() const { return context; }

    // A parancsor handle – kernel enqueue és buffer olvasás/írás műveletekhez.
    cl_command_queue getQueue()   const { return queue; }

    // Az eszköz handle – program build és eszközinfo lekérdezéshez szükséges.
    cl_device_id     getDevice()  const { return device; }

    // Felszabadítja a parancssort és a contextet.
    ~OpenCLManager();

private:
    cl_platform_id   platform = nullptr;  // az első elérhető OpenCL platform
    cl_device_id     device = nullptr;  // az első GPU eszköz a platformon
    cl_context       context = nullptr;  // GL-sharing OpenCL context
    cl_command_queue queue = nullptr;  // in-order parancsor a kernelekhez
};