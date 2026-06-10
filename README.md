# Contador Distribuido de Frecuencia de Palabras — C++
### Patrones Ambassador y Circuit Breaker | Windows / MinGW

---

## 📋 Tabla de contenidos

- [Descripción del proyecto](#descripción-del-proyecto)
- [Requisitos](#requisitos)
- [Instalación](#instalación)
- [Compilación](#compilación)
- [Uso](#uso)
- [Arquitectura](#arquitectura)
- [Solución de problemas](#solución-de-problemas)

---

## 📖 Descripción del proyecto

**Contador Distribuido de Frecuencia de Palabras** es una aplicación cliente-servidor en C++ que procesa textos de gran tamaño de manera distribuida.

**Características principales:**
- 🔗 **Ambassador Pattern**: Coordina comunicaciones entre coordinador y múltiples workers
- 🛡️ **Circuit Breaker Pattern**: Detecta y mitiga fallos en la comunicación de red
- ⚡ **Paralelismo**: Procesa fragmentos en múltiples threads concurrentes
- 🔐 **Thread-safe**: Sincronización con `std::mutex` en componentes críticos

**Especificaciones técnicas:**
| Campo | Valor |
|-------|-------|
| Lenguaje | C++17 |
| Plataforma | Windows 7/10/11 |
| Compilador | g++ (MinGW-w64) |
| Red | TCP con Winsock2 (nativa) |

---

## 📁 Estructura del proyecto

```
word_counter_cpp/
├── circuit_breaker.hpp     // Patrón Circuit Breaker (CLOSED/OPEN/HALF_OPEN)
├── text_extractor.hpp      // Extractor de archivos .txt
├── emsamblador.hpp         // Patrón Ambassador (envío de tareas por red)
├── worker.cpp              // Nodo trabajador (servidor TCP)
├── coordinador.cpp         // Coordinador principal
├── build.bat               // Script de compilación MinGW
└── README.md
```

> **Todos los headers son header-only** (`.hpp`). Solo `worker.cpp` y `coordinador.cpp` se compilan.

---

## 1️⃣ Requisitos

| Herramienta | Versión mínima | Propósito |
|---|---|---|
| Windows | 7 / 10 / 11 | Sistema operativo |
| g++ / MinGW-w64 | 10+ (C++17) | Compilador |
| nlohmann/json | 3.x | Serialización JSON (header-only) |

---

## 2️⃣ Instalación

### Opción A — MSYS2 (recomendado)

1. Descarga desde **https://www.msys2.org**
2. Ejecuta el instalador (ruta por defecto: `C:\msys64`)
3. Abre **MSYS2 UCRT64** y ejecuta:
   ```bash
   pacman -S mingw-w64-ucrt-x86_64-gcc
   ```
4. Cierra la terminal. `g++` estará en `C:\msys64\ucrt64\bin`

### Opción B — WinLibs (sin MSYS2)

1. Descarga desde **https://winlibs.com** (GCC 13 + MinGW-w64, `.zip`)
2. Extrae en ruta sin espacios: `C:\mingw64`
3. Ejecutable: `C:\mingw64\bin\g++.exe`

### Agregar MinGW al PATH de Windows

1. Presiona `Win + S` → Busca "Variables de entorno"
2. Haz clic en "Editar las variables de entorno del sistema"
3. Selecciona "Variables de entorno..."
4. En "Variables del sistema", busca `Path` → Editar
5. Haz clic en "Nuevo" y agrega tu ruta:

| Instalación | Ruta |
|---|---|
| MSYS2 | `C:\msys64\ucrt64\bin` |
| WinLibs | `C:\mingw64\bin` |

6. Haz clic en "Aceptar" en todas las ventanas
7. **Abre un CMD nuevo** para verificar los cambios

### Verificar la instalación

```cmd
g++ --version
```

Deberías ver:
```
g++ (Rev1, Built by MSYS2 project) 13.2.0
```

### Descargar nlohmann/json

1. Ve a **https://github.com/nlohmann/json/releases**
2. Descarga **`json.hpp`** (Single Header)
3. Crea la estructura en tu carpeta del proyecto:
   ```
   word_counter_cpp/
   └── nlohmann/
       └── json.hpp
   ```

**Alternativa con PowerShell** (desde la carpeta del proyecto):
```powershell
mkdir nlohmann
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp" -OutFile "nlohmann\json.hpp"
```

---

## 3️⃣ Compilación

Con MinGW en el PATH y `nlohmann/json.hpp` en su lugar, ejecuta desde CMD:

```cmd
build.bat
```

Genera: **`worker.exe`** y **`coordinator.exe`**

> Si ves `fatal error: nlohmann/json.hpp: No such file or directory`, verifica que creaste `nlohmann\` correctamente.

---

## 4️⃣ Uso

### Paso 1 — Iniciar los Workers (5 terminales separadas)

Cada Worker necesita: ID único, puerto TCP y archivo de texto

```cmd
REM Terminal 1
worker.exe --id worker_1 --port 5001 --file libro.txt

REM Terminal 2
worker.exe --id worker_2 --port 5002 --file libro.txt

REM Terminal 3
worker.exe --id worker_3 --port 5003 --file libro.txt

REM Terminal 4
worker.exe --id worker_4 --port 5004 --file libro.txt

REM Terminal 5
worker.exe --id worker_5 --port 5005 --file libro.txt
```

> El archivo debe estar en la misma carpeta o usa la ruta completa: `C:\textos\libro.txt`

### Paso 2 — Ejecutar el Coordinador (6ª terminal)

```cmd
coordinator.exe libro.txt
```

El Coordinador:
- Divide el archivo en 5 fragmentos
- Envía cada uno a un Worker diferente
- Agrega los resultados en un conteo global
- Muestra las palabras más frecuentes

---

## 5️⃣ Arquitectura

```
┌──────────────────┐     ┌──────────────────┐     ┌────────────────────┐
│ coordinator.exe  │────▶│ emsamblador.hpp  │────▶│circuit_breaker.hpp │
│(coordinador.cpp) │     │  (Ambassador)    │     │  (por worker)      │
└──────────────────┘     └──────────────────┘     └────────────────────┘
       │                                                       ▼
       │                                             ┌─────────────────┐
       │                                             │  worker.exe (x5)│
       │                                             └─────────────────┘
       │                                                       ▲
       └──────────── JSON sobre TCP (Winsock2) ────────────┘
```

### Flujo de datos

```
coordinator.exe
  │
  ├─ Lee archivo .txt en memoria
  ├─ Divide en N fragmentos por bytes
  ├─ Lanza N std::async → Emsamblador::dispatch()
  │
  Emsamblador::dispatch()
    ├─ CircuitBreaker::allowRequest() → ¿abierto?
    ├─ TCP socket → JSON { "start": X, "end": Y } → worker.exe
    ├─ Reintentos si falla (hasta 2 intentos)
    └─ CircuitBreaker::recordSuccess() / recordFailure()
  │
  └─ Combina mapas de frecuencia → muestra resultados
```

### Estados del Circuit Breaker

```
  CLOSED ──(3 fallos)──▶ OPEN ──(10 seg)──▶ HALF_OPEN
    ▲                                            │
    └──────────────── éxito ──────────────────┘
                              fallo → OPEN
```

**Significado de estados:**
- **CLOSED**: Sistema normal, procesa todas las solicitudes
- **OPEN**: Se detectaron 3 fallos, rechaza solicitudes inmediatamente
- **HALF_OPEN**: Después de 10 segundos, permite 1 solicitud de prueba

---

## 🔧 Características técnicas

- ✅ **Sin dependencias externas de red** — Solo Winsock2 (nativa de Windows)
- ✅ **Comunicación TCP cruda** — JSON puro sobre socket (sin HTTP)
- ✅ **Paralelismo con `std::async`** — Cada fragmento en su propio hilo
- ✅ **Circuit Breaker thread-safe** — Sincronizado internamente
- ✅ **Filtrado de palabras** — Ignora tokens de 3 caracteres o menos
- ✅ **Reintentos automáticos** — Hasta 2 intentos por conexión fallida

---

## 🐛 Solución de problemas

| Error | Causa | Solución |
|---|---|---|
| `'g++' no se reconoce` | MinGW no en PATH | Repite config. del PATH y abre CMD nuevo |
| `nlohmann/json.hpp: No such file` | Falta librería JSON | Verifica que creaste `nlohmann\json.hpp` |
| `Error: --file requerido` | Worker sin parámetro `--file` | Agrega `--file archivo.txt` al comando |
| `bind() failed (puerto ocupado)` | Worker ya corriendo en ese puerto | Cierra con `taskkill /f /im worker.exe` |
| `connect() failed` | Worker no iniciado | Inicia todos los workers antes del coordinador |
| `WSAStartup failed` | Falta flag Winsock2 | Verifica que `build.bat` incluye `-lws2_32` |
| `Circuit Breaker activo` | 3+ fallos detectados | Espera 10 segundos y reintenta |
| `Lectura de archivo fallida` | Ruta inválida | Verifica que el archivo existe |

---

## 📝 Ejemplo completo

Supongamos `libro.txt` en `C:\proyectos\word_counter_cpp\`:

**Terminales 1-5** (Workers):
```cmd
cd C:\proyectos\word_counter_cpp
worker.exe --id worker_1 --port 5001 --file libro.txt
worker.exe --id worker_2 --port 5002 --file libro.txt
worker.exe --id worker_3 --port 5003 --file libro.txt
worker.exe --id worker_4 --port 5004 --file libro.txt
worker.exe --id worker_5 --port 5005 --file libro.txt
```

**Terminal 6** (Coordinador):
```cmd
cd C:\proyectos\word_counter_cpp
coordinator.exe libro.txt
```

**Salida esperada:**
```
Procesando: libro.txt
Enviando tarea a worker_1 (localhost:5001)...
Enviando tarea a worker_2 (localhost:5002)...
...
Top 10 palabras más frecuentes:
1. the - 1542
2. and - 1203
3. to - 987
...
```

---

## 📄 Licencia

Proyecto para propósitos educativos.

---

## 👤 Autor

**Jair Cadenas**, **Tania Chávez**, **Mochi-Pichy** y **arieleru**

Diseño e implementación de patrones Ambassador y Circuit Breaker para procesamiento distribuido de texto.