# Contador Distribuido de Frecuencia de Palabras — C++
### Patrones Ambassador y Circuit Breaker | Windows / MinGW

---

## Archivos del proyecto

```
word_counter_cpp/
├── circuit_breaker.hpp   // Patrón Circuit Breaker (CLOSED/OPEN/HALF_OPEN)
├── text_extractor.hpp    // Extractor .txt
├── emsamblador.hpp       // Patrón Ambassador (envío de tareas por red)
├── worker.cpp            // Nodo trabajador (servidor TCP)
├── coordinador.cpp       // Coordinador principal
├── build.bat             // Script de compilación MinGW
└── README.md
```

> **Todos los headers son header-only** (`.hpp`). Solo hay dos `.cpp` que se compilan.

---

## 1. Requisitos

| Herramienta          | Versión mínima | Para qué se usa                          |
|----------------------|----------------|------------------------------------------|
| Windows              | 7 / 10 / 11    | Sistema operativo objetivo               |
| g++ / MinGW-w64      | 10+ (C++17)    | Compilar los `.cpp`                      |
| nlohmann/json        | 3.x            | Serialización JSON (header-only)         |

---

## 2. Instalar MinGW-w64 (compilador g++)

MinGW-w64 es el compilador de C++ para Windows. Sigue estos pasos:

### Opción A — Instalador MSYS2 (recomendado)

1. Ve a **https://www.msys2.org** y descarga el instalador (`msys2-x86_64-*.exe`).
2. Ejecuta el instalador. Deja la ruta por defecto (`C:\msys64`).
3. Al terminar, abre la terminal **MSYS2 UCRT64** y ejecuta:

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc
```

4. Cierra esa terminal. Ya tienes `g++` instalado en `C:\msys64\ucrt64\bin`.

### Opción B — WinLibs (instalación sin MSYS2)

1. Ve a **https://winlibs.com** y descarga el paquete **GCC 13 + MinGW-w64** para Windows 64-bit (`.zip`).
2. Extrae el ZIP en una ruta sin espacios, por ejemplo `C:\mingw64`.
3. El ejecutable de g++ queda en `C:\mingw64\bin\g++.exe`.

---

## 3. Agregar MinGW al PATH de Windows

Para que el comando `g++` funcione desde cualquier CMD o PowerShell, debes agregar la carpeta `bin` de MinGW a la variable de entorno `PATH`.

### Pasos (Windows 10 / 11)

1. Presiona `Win + S` y busca **"Variables de entorno"**.
2. Haz clic en **"Editar las variables de entorno del sistema"**.
3. En la ventana que se abre, haz clic en el botón **"Variables de entorno..."** (abajo a la derecha).
4. En la sección **"Variables del sistema"**, busca la variable **`Path`** y selecciónala.
5. Haz clic en **"Editar..."**.
6. Haz clic en **"Nuevo"** y escribe la ruta a la carpeta `bin` de tu instalación:

| Instalación          | Ruta a agregar               |
|----------------------|------------------------------|
| MSYS2 (opción A)     | `C:\msys64\ucrt64\bin`       |
| WinLibs (opción B)   | `C:\mingw64\bin`             |

7. Haz clic en **Aceptar** en todas las ventanas abiertas.
8. **Cierra y vuelve a abrir CMD** para que los cambios tomen efecto.

### Verificar la instalación

Abre una nueva ventana de CMD y ejecuta:

```bat
g++ --version
```

Deberías ver algo como:

```
g++ (Rev1, Built by MSYS2 project) 13.2.0
```

Si ves un error de `'g++' no se reconoce como comando interno`, revisa que la ruta del paso 6 es correcta y que abriste un CMD nuevo.

---

## 4. Descargar nlohmann/json (dependencia de cabecera)

El proyecto usa `nlohmann/json.hpp` para serializar y deserializar JSON. Es una librería de un solo archivo.

### Pasos

1. Ve a **https://github.com/nlohmann/json/releases** y descarga la versión más reciente.
2. En los *Assets* de la release busca el archivo **`json.hpp`** (bajo *Single Header*) o descarga el ZIP del código fuente.
3. Dentro de la carpeta del proyecto, crea la siguiente estructura de carpetas:

```
word_counter_cpp/
└── nlohmann/
    └── json.hpp       ← pega el archivo aquí
```

4. Así el `#include "nlohmann/json.hpp"` de los `.cpp` lo encontrará automáticamente.

> **Alternativa rápida con PowerShell** (desde la carpeta del proyecto):
> ```powershell
> mkdir nlohmann
> Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp" -OutFile "nlohmann\json.hpp"
> ```

---

## 5. Compilar

Con MinGW en el PATH y `nlohmann/json.hpp` en su lugar, ejecuta desde CMD dentro de la carpeta del proyecto:

```bat
build.bat
```

Esto genera `worker.exe` y `coordinator.exe` en la misma carpeta.

> Si ves el error `fatal error: nlohmann/json.hpp: No such file or directory`, verifica que creaste la subcarpeta `nlohmann\` con el archivo dentro.

---

## 6. Archivos de texto soportados

| Formato | Soporte      | Notas                                              |
|---------|--------------|----------------------------------------------------|
| `.txt`  | Completo     | Lectura directa, sin dependencias adicionales      |
| `.pdf`  | No soportado | `text_extractor.hpp` solo procesa `.txt` actualmente |

---

## 7. Ejecutar

### Paso 1 — Iniciar los Workers (5 terminales separadas)

Cada Worker debe conocer su ID, su puerto y el archivo de texto que procesará.

```bat
REM Terminal 1
worker.exe --id worker_1 --port 5001 --file Alice_in_Wonderland.txt

REM Terminal 2
worker.exe --id worker_2 --port 5002 --file Alice_in_Wonderland.txt

REM Terminal 3
worker.exe --id worker_3 --port 5003 --file Alice_in_Wonderland.txt

REM Terminal 4
worker.exe --id worker_4 --port 5004 --file Alice_in_Wonderland.txt

REM Terminal 5
worker.exe --id worker_5 --port 5005 --file Alice_in_Wonderland.txt
```

> El archivo indicado con `--file` debe estar en la misma carpeta donde ejecutas el comando, o puedes escribir la ruta completa (ej. `C:\textos\Alice_in_Wonderland.txt`).

### Paso 2 — Ejecutar el Coordinador (6ta terminal)

```bat
coordinator.exe Alice_in_Wonderland.txt
```

El Coordinador dividirá el archivo en 5 fragmentos, los enviará a cada Worker y mostrará el conteo global de palabras al final.

---

## 8. Arquitectura

```
┌──────────────────┐     ┌──────────────────┐     ┌────────────────────┐     ┌──────────┐
│  coordinator.exe │────▶│ emsamblador.hpp  │────▶│ circuit_breaker.hpp│────▶│worker.exe│
│  (coordinador.cpp│     │ (Ambassador)     │     │ (por worker)       │     │ (x5)     │
└──────────────────┘     └──────────────────┘     └────────────────────┘     └──────────┘
         │                                                                          │
         │◀──────────────────── JSON sobre TCP crudo (Winsock2) ───────────────────┘
```

### Flujo de datos

```
coordinator.exe
  │
  ├─ Lee el archivo .txt completo a memoria
  ├─ Divide en N fragmentos por bytes
  ├─ Lanza N std::async → Emsamblador::dispatch()
  │
  Emsamblador::dispatch()
    ├─ CircuitBreaker::allowRequest() → ¿abierto?
    ├─ TCP socket → JSON { "start": X, "end": Y } → worker.exe
    ├─ Timeout / reintento si falla (hasta 2 intentos)
    └─ CircuitBreaker::recordSuccess() / recordFailure()
  │
  └─ Combina mapas de frecuencia → muestra resultados
```

### Estados del Circuit Breaker

```
    CLOSED ──(3 fallos)──▶ OPEN ──(10 seg)──▶ HALF_OPEN
      ▲                                              │
      └──────────────── éxito ──────────────────────┘
                              fallo → OPEN
```

---

## 9. Notas técnicas

- **Sin dependencias externas de red.** Solo Winsock2 (incluida en Windows, activa con `-lws2_32`).
- **Comunicación TCP cruda.** No es HTTP; el Coordinador envía el JSON directamente por el socket y el Worker responde con JSON.
- **Paralelismo con `std::async`.** Cada fragmento se despacha en su propio hilo del SO.
- **Circuit Breaker thread-safe.** Protegido internamente con `std::mutex`.
- **Palabras filtradas.** El Worker ignora tokens de 3 caracteres o menos (artículos, preposiciones, etc.).

---

## 10. Solución de problemas

| Error | Causa probable | Solución |
|-------|----------------|----------|
| `'g++' no se reconoce como comando` | MinGW no está en el PATH | Repite el paso 3 y abre un CMD nuevo |
| `fatal error: nlohmann/json.hpp` | Falta la librería JSON | Sigue el paso 4 |
| `Error critico: Debe indicar el archivo local usando --file` | Worker iniciado sin `--file` | Agrega `--file NombreArchivo.txt` al comando del Worker |
| `bind() failed (puerto ocupado)` | El Worker ya corre en ese puerto | Cierra el proceso anterior con `taskkill /f /im worker.exe` o usa otro puerto |
| `connect() failed` | El Worker no está corriendo | Inicia primero todos los `worker.exe`, luego el coordinador |
| `WSAStartup failed` | Falta el flag de enlace | Asegúrate de compilar con `-lws2_32` (ya incluido en `build.bat`) |
| `El [worker_N] no pudo completar su tarea` | Circuit Breaker activo o Worker caído | Espera 10 segundos (cooldown) y vuelve a lanzar el coordinador |
