# Contador Distribuido de Frecuencia de Palabras — C++
### Patrones Ambassador y Circuit Breaker | Windows / MinGW

---

## 📋 Tabla de contenidos

- [Descripción del proyecto](#descripción-del-proyecto)
- [Archivos del proyecto](#archivos-del-proyecto)
- [Requisitos](#requisitos)
- [Instalación](#instalación)
- [Compilación](#compilación)
- [Uso](#uso)
- [Arquitectura](#arquitectura)
- [Solución de problemas](#solución-de-problemas)

---

## 📖 Descripción del proyecto

**Contador Distribuido de Frecuencia de Palabras** es una aplicación cliente-servidor en C++ que procesa textos de gran tamaño de manera distribuida. Utiliza dos patrones de diseño clave:

- **Ambassador Pattern**: Coordina las comunicaciones entre el coordinador central y múltiples workers
- **Circuit Breaker Pattern**: Detecta y mitiga fallos en la comunicación de red

La aplicación divide un archivo de texto en fragmentos, los envía a workers independientes (nodos de procesamiento) a través de TCP, y agrega los resultados finales en un conteo global de palabras.

**Lenguaje**: C++17  
**Plataforma**: Windows 7/10/11  
**Compilador**: g++ (MinGW-w64)

---

## 📁 Archivos del proyecto

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

## 1️⃣ Requisitos

| Herramienta          | Versión mínima | Propósito                            |
|----------------------|----------------|--------------------------------------|
| Windows              | 7 / 10 / 11    | Sistema operativo objetivo           |
| g++ / MinGW-w64      | 10+ (C++17)    | Compilador de C++                    |
| nlohmann/json        | 3.x            | Serialización JSON (header-only)     |

---

## 2️⃣ Instalación

### Instalar MinGW-w64 (compilador g++)

MinGW-w64 es el compilador de C++ para Windows. Elige una de las siguientes opciones:

#### Opción A — MSYS2 (recomendado)

1. Descarga el instalador desde **https://www.msys2.org** (`msys2-x86_64-*.exe`)
2. Ejecuta el instalador y deja la ruta por defecto (`C:\msys64`)
3. Al terminar, abre la terminal **MSYS2 UCRT64** y ejecuta:

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc
```

4. Cierra la terminal. Ya tienes `g++` instalado en `C:\msys64\ucrt64\bin`

#### Opción B — WinLibs (instalación directa sin MSYS2)

1. Descarga el paquete desde **https://winlibs.com** (GCC 13 + MinGW-w64 para Windows 64-bit, `.zip`)
2. Extrae el ZIP en una ruta sin espacios, por ejemplo `C:\mingw64`
3. El ejecutable está en `C:\mingw64\bin\g++.exe`

### Agregar MinGW al PATH de Windows

Para que el comando `g++` funcione desde cualquier terminal:

1. Presiona `Win + S` y busca **"Variables de entorno"**
2. Haz clic en **"Editar las variables de entorno del sistema"**
3. Haz clic en **"Variables de entorno..."** (abajo a la derecha)
4. En la sección **"Variables del sistema"**, busca **`Path`** y selecciónala
5. Haz clic en **"Editar..."** y luego en **"Nuevo"**
6. Agrega la ruta correspondiente a tu instalación:

| Instalación          | Ruta a agregar               |
|----------------------|------------------------------|
| MSYS2 (opción A)     | `C:\msys64\ucrt64\bin`       |
| WinLibs (opción B)   | `C:\mingw64\bin`             |

7. Haz clic en **"Aceptar"** en todas las ventanas
8. **Cierra y vuelve a abrir CMD** para que los cambios tomen efecto

#### Verificar la instalación

Abre una nueva ventana de CMD y ejecuta:

```cmd
g++ --version
```

Deberías ver algo como:

```
g++ (Rev1, Built by MSYS2 project) 13.2.0
```

Si ves `'g++' no se reconoce`, verifica que la ruta es correcta y que abriste un CMD nuevo.

### Descargar nlohmann/json

El proyecto usa `nlohmann/json.hpp` para serializar JSON. Es una librería de un solo archivo:

1. Ve a **https://github.com/nlohmann/json/releases** y descarga la versión más reciente
2. En los *Assets* busca **`json.hpp`** bajo *Single Header* (o descarga el ZIP)
3. Dentro de la carpeta del proyecto, crea esta estructura:

```
word_counter_cpp/
└── nlohmann/
    └── json.hpp       ← pega el archivo aquí
```

4. Ahora `#include "nlohmann/json.hpp"` lo encontrará automáticamente

**Alternativa rápida con PowerShell** (desde la carpeta del proyecto):

```powershell
mkdir nlohmann
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp" -OutFile "nlohmann\json.hpp"
```

---

## 3️⃣ Compilación

Con MinGW en el PATH y `nlohmann/json.hpp` en su lugar, ejecuta desde CMD en la carpeta del proyecto:

```cmd
build.bat
```

Esto genera **`worker.exe`** y **`coordinator.exe`** en la misma carpeta.

> Si ves `fatal error: nlohmann/json.hpp: No such file or directory`, verifica que creaste `nlohmann\` con el archivo dentro.

---

## 4️⃣ Uso

### Archivos soportados

| Formato | Soporte      | Notas                                              |
|---------|--------------|----------------------------------------------------|
| `.txt`  | ✅ Completo  | Lectura directa, sin dependencias adicionales      |
| `.pdf`  | ❌ No        | `text_extractor.hpp` solo procesa `.txt` actualmente |

### Ejecución

#### Paso 1 — Iniciar los Workers (5 terminales separadas)

Cada Worker necesita su ID único, un puerto TCP y el archivo de texto a procesar:

```cmd
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

> El archivo indicado con `--file` debe estar en la misma carpeta o escribe la ruta completa (ej. `C:\textos\Alice_in_Wonderland.txt`)

#### Paso 2 — Ejecutar el Coordinador (6ª terminal)

```cmd
coordinator.exe Alice_in_Wonderland.txt
```

El Coordinador:
- Divide el archivo en 5 fragmentos
- Envía cada fragmento a un Worker diferente
- Agrega los resultados en un conteo global
- Muestra el top de palabras más frecuentes

---

## 5️⃣ Arquitectura

```
┌──────────────────┐     ┌──────────────────┐     ┌────────────────────┐
│ coordinator.exe  │────▶│ emsamblador.hpp  │────▶│circuit_breaker.hpp │
│(coordinador.cpp) │     │  (Ambassador)    │     │  (por worker)      │
└──────────────────┘     └──────────────────┘     └────────────────────┘
       │                                                      ▼
       │                                            ┌─────────────────┐
       │                                            │  worker.exe (x5)│
       │                                            └─────────────────┘
       │                                                      ▲
       └──────────── JSON sobre TCP (Winsock2) ────────────┘
```

### Flujo de datos

```
coordinator.exe
  │
  ├─ Lee el archivo .txt completo en memoria
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

**Estados**:
- **CLOSED**: Sistema funcionando normalmente, todas las solicitudes se procesan
- **OPEN**: Se han detectado 3 fallos consecutivos, todas las solicitudes se rechazan inmediatamente
- **HALF_OPEN**: Después de 10 segundos en estado OPEN, se permite 1 solicitud de prueba

---

## 6️⃣ Características técnicas

- ✅ **Sin dependencias externas de red.** Solo Winsock2 (nativa de Windows)
- ✅ **Comunicación TCP cruda.** No es HTTP; envío directo de JSON por socket
- ✅ **Paralelismo con `std::async`.** Cada fragmento se despacha en su propio hilo del SO
- ✅ **Circuit Breaker thread-safe.** Protegido internamente con `std::mutex`
- ✅ **Filtrado de palabras.** Se ignoran tokens de 3 caracteres o menos (artículos, preposiciones)
- ✅ **Reintentos automáticos.** Hasta 2 intentos si falla la conexión a un Worker

---

## 7️⃣ Solución de problemas

| Error | Causa probable | Solución |
|-------|----------------|----------|
| `'g++' no se reconoce como comando` | MinGW no está en el PATH | Repite el paso de configuración del PATH y abre un CMD nuevo |
| `fatal error: nlohmann/json.hpp: No such file or directory` | Falta la librería JSON | Verifica que creaste `nlohmann\` con `json.hpp` dentro |
| `Error critico: Debe indicar el archivo local usando --file` | Worker iniciado sin `--file` | Agrega `--file NombreArchivo.txt` al comando del Worker |
| `bind() failed (puerto ocupado)` | El Worker ya corre en ese puerto | Cierra con `taskkill /f /im worker.exe` o usa otro puerto |
| `connect() failed` | El Worker no está corriendo | Inicia primero todos los `worker.exe`, luego el coordinador |
| `WSAStartup failed` | Falta el flag de enlace Winsock2 | Verifica que `build.bat` incluye `-lws2_32` |
| `El [worker_N] no pudo completar su tarea` | Circuit Breaker activo o Worker caído | Espera 10 segundos (cooldown) y vuelve a ejecutar el coordinador |
| `Lectura de archivo fallida` | Ruta incorrecta o archivo no existe | Verifica que el archivo existe en la ruta indicada |

---

## 📝 Ejemplo completo

Supongamos que tienes `book.txt` en `C:\proyectos\word_counter_cpp\`:

**Terminal 1-5** (Workers):
```cmd
cd C:\proyectos\word_counter_cpp
worker.exe --id worker_1 --port 5001 --file book.txt
worker.exe --id worker_2 --port 5002 --file book.txt
... (y así sucesivamente)
```

**Terminal 6** (Coordinador):
```cmd
cd C:\proyectos\word_counter_cpp
coordinator.exe book.txt
```

**Salida esperada**:
```
Procesando: book.txt
Enviando tarea a worker_1 (localhost:5001)...
Enviando tarea a worker_2 (localhost:5002)...
...
Top 10 palabras más frecuentes:
1. the - 1542
2. and - 1203
...
```

---

## 📄 Licencia

Este proyecto es para propósitos educativos.

---

## 👤 Autor

**Jair Cadenas**, **Tania Chávez**, **Mochi-Pichy** y **arieleru**

Diseño e implementación del patrón Ambassador y Circuit Breaker para procesamiento distribuido de texto.
