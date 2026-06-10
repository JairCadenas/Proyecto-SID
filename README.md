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
- [Redirección de rango cuando worker falla](#redirección-de-rango-cuando-worker-falla)
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
┌──────────────────┐     ┌──────────────────┐     ┌────────────────────┐[...]
│ coordinator.exe  │────▶│ emsamblador.hpp  │────▶│circuit_breaker.hpp │
│(coordinador.cpp) │     │  (Ambassador)    │     │  (por worker)      │
└──────────────────┘     └──────────────────┘     └────────────────────┘[...]
       │                                                       ▼
       │                                             ┌─────────────────┐
       │                                             │  worker.exe (x5)│
       │                                             ��─────────────────┘
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

## 6️⃣ Redirección de rango cuando worker falla

### ¿Qué sucede cuando un worker se detiene?

Cuando un **worker falla** o se desconecta, el **Circuit Breaker** lo detecta y el **Coordinador** debe reasignar el rango de datos que ese worker estaba procesando a otros workers disponibles.

### Procedimiento para provocar fallo y reasignar rango

#### **Paso 1: Sistema normal en ejecución**

Todas las terminales están activas:
- **Terminales 1-5**: 5 workers corriendo normalmente
- **Terminal 6**: Coordinador procesando

#### **Paso 2: Simular fallo controlado del worker**

Para que un worker **termine pero permita reintentos** (primera pausa):

```cmd
Presiona: Ctrl + C (UNA VEZ)
```

**¿Qué sucede?**
- El worker recibe la señal SIGINT
- Finaliza su proceso actual de manera ordenada
- **Sigue procesando hasta terminar el fragmento actual**
- El socket se mantiene abierto momentáneamente

**Estado del sistema:**
- ✅ El worker intenta terminar correctamente
- ✅ El Coordinador aún puede intentar reconectar
- ⚠️ El Circuit Breaker pasa a estado **OPEN** tras 3 fallos

#### **Paso 3: Forzar desconexión y reasignación (segunda pausa)**

Para que el worker se **desconecte completamente** y se dispare la reasignación:

```cmd
Presiona: Ctrl + C (DOS VECES - rápidamente)
```

**¿Qué sucede?**
- Primera `Ctrl + C`: Iniciada la terminación ordenada
- Segunda `Ctrl + C`: **Termina el worker inmediatamente sin esperar**
- El socket TCP se cierra abruptamente
- Se detiene todo procesamiento en ese worker

**Estado del sistema:**
- 🔴 El worker está **COMPLETAMENTE OFFLINE**
- ⚠️ Circuit Breaker en estado **OPEN**
- 🔄 **El Coordinador detecta la caída**
- ✅ **Reasigna automáticamente el rango de datos** a los workers restantes

### Flujo de reasignación de rango

```
┌─────────────────────────────────────────────────────────────┐
│  Coordinador envía fragmento a worker_3                     │
│  (bytes 0-20000)                                            │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
                   ┌──────────────────┐
                   │  worker_3.exe    │
                   │  Procesando... ◯ │
                   └──────────────────┘
                           │
                 ┌─────────┴──────────┐
                 │ Usuario presiona  │
                 │ Ctrl+C DOS VECES  │
                 └─────────┬──────────┘
                           ▼
            ┌──────────────────────────────────┐
            │ worker_3 TERMINA ABRUPTAMENTE    │
            │ Socket se cierra                 │
            └──────────────────────────────────┘
                           │
            ┌──────────────▼──────────────────┐
            │ Coordinador detecta:            │
            │ ✘ connect() failed              │
            │ ✘ Circuit Breaker → OPEN       │
            └──────────────┬──────────────────┘
                           │
            ┌──────────────▼──────────────────┐
            │ REASIGNACIÓN AUTOMÁTICA:        │
            │ • Rango 0-20000 → worker_1     │
            │ • Rango 20001-40000 → worker_2 │
            │ • Rango 40001-60000 → worker_4 │
            │ • Rango 60001-80000 → worker_5 │
            └──────────────────────────────────┘
                           │
            ┌──────────────▼──────────────────┐
            │ Procesamiento continúa con 4   │
            │ workers disponibles             │
            └──────────────────────────────────┘
```

### Ejemplo práctico: Paso a paso

**Escenario: Ejecutar el sistema completo**

```cmd
REM Terminal 1: worker_1
C:\proyecto> worker.exe --id worker_1 --port 5001 --file libro.txt
✓ Esperando conexiones en localhost:5001

REM Terminal 2: worker_2
C:\proyecto> worker.exe --id worker_2 --port 5002 --file libro.txt
✓ Esperando conexiones en localhost:5002

REM Terminal 3: worker_3
C:\proyecto> worker.exe --id worker_3 --port 5003 --file libro.txt
✓ Esperando conexiones en localhost:5003
[... continúa normalmente ...]
```

```cmd
REM Terminal 6: Coordinador
C:\proyecto> coordinator.exe libro.txt
Procesando: libro.txt
Enviando tarea a worker_1 (localhost:5001)...
Enviando tarea a worker_2 (localhost:5002)...
Enviando tarea a worker_3 (localhost:5003)...
[... procesando fragmentos ...]
```

**Ahora en la Terminal 3 (worker_3), simular fallo:**

```cmd
REM Terminal 3 - Primera acción: Ctrl + C una vez
Procesando bytes 40001-60000...

Procesando bytes 40001-60000...
^C
⏸ Recibida señal de terminación (SIGINT)
⏸ Finalizando tareas actuales...

REM Espera unos segundos...

REM Ahora presionar Ctrl + C DOS VECES rápidamente
^C^C
🔴 TERMINACIÓN FORZADA - worker_3 APAGADO
[proceso terminado]
```

**Lo que pasa en el Coordinador (Terminal 6):**

```cmd
[...procesando...]
⚠ worker_3: connection timeout (intento 1/2)
⚠ worker_3: connection timeout (intento 2/2)
🔴 FALLO CRÍTICO: worker_3 desconectado
🔄 REASIGNACIÓN: Redistribuyendo bytes 40001-60000...
   → Asignando a worker_1: +15000 bytes
   → Asignando a worker_2: +15000 bytes
✅ Rango reasignado correctamente
Continuando con 4 workers...
[procesamiento sigue con los 4 workers restantes]
```

### Resumen de controles

| Acción | Resultado | Cuándo usar |
|--------|-----------|-----------|
| **1x Ctrl + C** | Finalización ordenada, permite reintentos | Para pausar temporalmente |
| **2x Ctrl + C** | Terminación forzada inmediata | Para simular caída / forzar reasignación |
| **Esperar 10 seg** | Circuit Breaker vuelve a HALF_OPEN | Para permitir reintentos tras OPEN |

---

## 🔧 Características técnicas

- ✅ **Sin dependencias externas de red** — Solo Winsock2 (nativa de Windows)
- ✅ **Comunicación TCP cruda** — JSON puro sobre socket (sin HTTP)
- ✅ **Paralelismo con `std::async`** — Cada fragmento en su propio hilo
- ✅ **Circuit Breaker thread-safe** — Sincronizado internamente
- ✅ **Filtrado de palabras** — Ignora tokens de 3 caracteres o menos
- ✅ **Reintentos automáticos** — Hasta 2 intentos por conexión fallida
- ✅ **Reasignación dinámica de rango** — Detecta fallos y redistribuye automáticamente

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
| `Reasignación no ocurre` | Presionaste Ctrl+C solo una vez | Presiona **DOS VECES rápido** para forzar cierre |

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
