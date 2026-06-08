#include <iostream>
#include <string>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>
#include <csignal>
#include <atomic>
#include <thread>
#include <map>
#include <chrono>
#include "nlohmann/json.hpp"
#include "circuit_breaker.hpp"
#include "text_extractor.hpp"

// Librerías nativas de red para Windows (Winsock)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;
using json = nlohmann::json;

// Configuración del Worker
static string WORKER_ID = "worker_1";
static int PORT = 5001;
static string LOCAL_FILE_PATH = ""; 
static atomic<bool> g_running{true};

// Circuit Breaker
static CircuitBreaker g_cb("WorkerInternalCB", 3, 10.0);

// ==========================================================================================
// CAMBIO 1: SE ELIMINÓ LA VARIABLE GLOBAL 'LOCAL_EXTRACTED_TEXT'
// Ya no guardamos gigabytes de texto en memoria RAM para evitar congelamientos en pruebas masivas.
// ==========================================================================================

// Cierre controlado del servidor al presionar Ctrl+C
void signalHandler(int signum) {
    cout << "\n[" << WORKER_ID << "] Apagando servidor de red de forma segura...\n";
    g_running = false;
}

// ==========================================================================================
// CAMBIO 2: FUNCIÓN DE CONTEO OPTIMIZADA CON STREAMING (APERTURA DINÁMICA POR CHUNKS)
// En vez de usar .substr() sobre la RAM, lee directamente del disco duro por bloques de 1 MB.
// ==========================================================================================
map<string, int> processTextRange(long start, long end) {
    map<string, int> mapaFrecuencias;
    
    // Control de seguridad: rangos inválidos o ruta vacía
    if (LOCAL_FILE_PATH.empty() || start >= end) {
        return mapaFrecuencias;
    }

    // Abrimos el archivo local directamente en modo binario para posicionamiento exacto
    ifstream file(LOCAL_FILE_PATH, ios::binary);
    if (!file.is_open()) {
        cerr << " [" << WORKER_ID << "] Error: No se pudo abrir el archivo en la ruta: " << LOCAL_FILE_PATH << "\n";
        return mapaFrecuencias;
    }

    // Mover el cabezal del disco exactamente al byte de inicio que ordenó el Coordinador
    file.seekg(start);
    
    long bytesPorProcesar = end - start;
    const size_t TAMANO_CHUNK = 1024 * 1024; // Bloques de 1 MB en RAM a la vez
    vector<char> bufferChunk(TAMANO_CHUNK);
    string fragmentoAcumulado = "";

    // Leemos el segmento asignado por pedazos
    while (bytesPorProcesar > 0 && file) {
        size_t bytesALeer = min((long)TAMANO_CHUNK, bytesPorProcesar);
        file.read(bufferChunk.data(), bytesALeer);
        size_t bytesLeidos = file.gcount();
        if (bytesLeidos == 0) break;

        // Añadir el bloque leído al string de procesamiento temporal
        fragmentoAcumulado.append(bufferChunk.data(), bytesLeidos);
        bytesPorProcesar -= bytesLeidos;

        // Limpieza de caracteres extraños y conversión a minúsculas
        for (char& c : fragmentoAcumulado) {
            if (!isalnum(static_cast<unsigned char>(c))) {
                c = ' ';
            } else {
                c = tolower(static_cast<unsigned char>(c)); 
            }
        }

        // Procesamiento del bloque de texto mediante stringstream
        stringstream ss(fragmentoAcumulado);
        string palabra;
        string ultimaPalabraIncompleta = "";

        while (ss >> palabra) {
            ultimaPalabraIncompleta = palabra;
            // Filtro básico de longitud de palabra
            if (palabra.size() > 3) {
                mapaFrecuencias[palabra]++; 
            }
        }

        // Manejo de fronteras: si el bloque de 1 MB corta una palabra a la mitad,
        // revertimos su conteo y la dejamos acumulada para que se complete con el siguiente bloque.
        if (!fragmentoAcumulado.empty() && fragmentoAcumulado.back() != ' ') {
            if (!ultimaPalabraIncompleta.empty() && mapaFrecuencias[ultimaPalabraIncompleta] > 0) {
                mapaFrecuencias[ultimaPalabraIncompleta]--; 
                if (mapaFrecuencias[ultimaPalabraIncompleta] == 0) {
                    mapaFrecuencias.erase(ultimaPalabraIncompleta);
                }
            }
            fragmentoAcumulado = ultimaPalabraIncompleta;
        } else {
            fragmentoAcumulado = "";
        }
    }

    file.close();
    return mapaFrecuencias;
}

// Procesa la petición cruda del socket, aplica el Circuit Breaker y devuelve un JSON string
string handleNetworkRequest(const string& rawBody, int& statusCode) {
    // El Circuit Breaker decide si el Worker está sano o colapsado
    if (!g_cb.allowRequest()) {
        statusCode = 503; 
        return "{\"error\":\"Circuit Breaker ABIERTO. Nodo temporalmente fuera de servicio\"}";
    }
    try {
        // LEER LA TAREA EN JSON desde el socket
        json tareaJson = json::parse(rawBody);
        long start = tareaJson["start"].get<long>();
        long end = tareaJson["end"].get<long>();
        cout << "[" << WORKER_ID << "] Procesando rango de bytes: [" << start << " - " << end << "]\n";
        
        // EJECUTAR EL TRABAJO REAL Y MEDIR TIEMPO
        auto startClock = chrono::steady_clock::now();
        map<string, int> resultadoMapa = processTextRange(start, end);
        auto endClock = chrono::steady_clock::now();
        double workerElapsedMs = chrono::duration<double, milli>(endClock - startClock).count();
        
        // Imprimir en la consola del Worker cuánto tardó él individualmente
        cout << " [" << WORKER_ID << "] Procesamiento local completado en: " << workerElapsedMs << " ms.\n";
        
        // NOTIFICAR ÉXITO AL CIRCUIT BREAKER
        g_cb.recordSuccess();
        statusCode = 200;
        
        // CONVERTIR EL MAPA A JSON
        json respuestaJson = resultadoMapa;
        return respuestaJson.dump();
    } catch (const exception& e) {
        g_cb.recordFailure();   
        statusCode = 500;
        cerr << " [" << WORKER_ID << "] Error interno procesando tarea: " << e.what() << "\n";
        return "{\"error\":\"Fallo interno en el Worker\"}";
    }
}

int main(int argc, char* argv[]) {
    // Leer los argumentos pasados por consola
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--id" && i + 1 < argc) WORKER_ID = argv[++i];
        if (arg == "--port" && i + 1 < argc) PORT = std::stoi(argv[++i]);
        if (arg == "--file" && i + 1 < argc) LOCAL_FILE_PATH = argv[++i];
    }
    
    if (LOCAL_FILE_PATH.empty()) {
        cerr << "Error critico: Debe indicar el archivo local usando --file\n";
        return 1;
    }

    // ==========================================================================================
    // CAMBIO 3: ELIMINACIÓN DE CARGA TOTAL EN MEMORIA RAM
    // Ya no llamamos a extractText(). Solo verificamos la existencia física del archivo.
    // ==========================================================================================
    ifstream testFile(LOCAL_FILE_PATH);
    if (!testFile.good()) {
        cerr << "Error critico: El archivo asignado no existe o no se puede leer: " << LOCAL_FILE_PATH << "\n";
        return 1;
    }
    testFile.close();
    cout << "[" << WORKER_ID << "] Modo Streaming listo. Archivo vinculado de forma segura.\n";

    // 1. INICIALIZAR WINSOCK
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Error al inicializar Winsock.\n";
        return 1;
    }
    
    signal(SIGINT, signalHandler);
    
    // 2. CREAR EL SOCKET DEL SERVIDOR
    SOCKET servidor_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (servidor_fd == INVALID_SOCKET) {
        cerr << "Error al crear el socket del servidor.\n";
        WSACleanup();
        return 1;
    }
    
    // Hacer que el socket NO SEA BLOQUEANTE para que respete a g_running
    u_long modo = 1;
    ioctlsocket(servidor_fd, FIONBIO, &modo);
    
    // 3. CONFIGURAR DIRECCIÓN Y PUERTO
    sockaddr_in direccion;
    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = INADDR_ANY; 
    direccion.sin_port = htons(PORT);
    
    if (bind(servidor_fd, (struct sockaddr*)&direccion, sizeof(direccion)) == SOCKET_ERROR) {
        cerr << "Error en el bind (¿puerto " << PORT << " ocupado?).\n";
        closesocket(servidor_fd);
        WSACleanup();
        return 1;
    }
    
    // 4. ESCUCHAR CONEXIONES
    if (listen(servidor_fd, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "Error en el listen.\n";
        closesocket(servidor_fd);
        WSACleanup();
        return 1;
    }
    
    cout << "[" << WORKER_ID << "] Servidor Worker inicializado con exito (Sockets Nativos).\n";
    cout << "[" << WORKER_ID << "] Escuchando peticiones crudas en el puerto " << PORT << "...\n";
    
    // 5. BUCLE ACTIVO BASADO EN TU ATOMIC (g_running)
    while (g_running.load()) {
        sockaddr_in dir_cliente;
        int tamano_dir = sizeof(dir_cliente);        
        
        SOCKET nuevo_socket = accept(servidor_fd, (struct sockaddr*)&dir_cliente, &tamano_dir);
        if (nuevo_socket != INVALID_SOCKET) {
            // Volver el socket del cliente bloqueante temporalmente para leer/escribir seguro
            u_long modo_cliente = 0;
            ioctlsocket(nuevo_socket, FIONBIO, &modo_cliente);
            
            // ==================================================================================
            // CAMBIO 4: BUFFER DE RECEPCIÓN EXPANDIDO (64 KB)
            // Evita cortes de flujo cuando el Coordinador envía parámetros en redes congestionadas.
            // ==================================================================================
            char buffer[65536] = {0};
            int bytesRecibidos = recv(nuevo_socket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytesRecibidos > 0) {
                string peticionStr(buffer);
                int statusCode = 200;
                
                string jsonRespuesta = handleNetworkRequest(peticionStr, statusCode);
                
                // Enviar la respuesta directa al Coordinador a través del socket
                send(nuevo_socket, jsonRespuesta.c_str(), jsonRespuesta.size(), 0);
            }
            closesocket(nuevo_socket);
        }
        this_thread::sleep_for(chrono::milliseconds(100)); //
    }
    
    // 6. LIMPIEZA NATIVA
    closesocket(servidor_fd);
    WSACleanup();
    cout << "[" << WORKER_ID << "] Nodo desconectado limpiamente de la red.\n";
    return 0;
}