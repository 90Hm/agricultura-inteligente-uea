#pragma once

// Guardamos las credenciales reales solo en credenciales.h.
// Si aun no existe, usamos la plantilla para poder verificar el sketch.

#if __has_include("credenciales.h")
  #include "credenciales.h"
#else
  #include "credenciales_ejemplo.h"
#endif
