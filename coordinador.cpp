#include <iostream>
#include <vector>
#include <map>
#include <future>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include "nlohmann/json.hpp"
#include "emsamblador.hpp"
// Librerías Nativas de red
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;
using json = nlohmann::json;

// Estructura auxiliar para guardar los rangos de los workers que fallaron
struct RangoHuerfano {
    unsigned long long start;
    unsigned long long end;
};

// CORRECCIÓN SINTÁCTICA: Mismo tokenizador exacto que usan los Workers
map<string, int> conteoLocalSecuencial(const string& texto) {
    map<string, int> mapaLocal;
    string textoLimpio = texto;
    
    // Cambiar no-alfanuméricos por espacios, igual que en el Worker
    for (char& c : textoLimpio) {
        if (!isalnum(static_cast<unsigned char>(c))) {
            c = ' ';
        } else {
            c = tolower(static_cast<unsigned char>(c)); 
        }
    }
    
    stringstream ss(textoLimpio);
    string palabra;
    while (ss >> palabra) {
        if (palabra.size() > 3) {
            mapaLocal[palabra]++;
        }
    }
    return mapaLocal;
}

int main(int argc, char* argv[]) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "[Coordinador] Error critico al inicializar Winsock.\n";
        return 1;
    }
    
    string filename = "texto.txt";
    if (argc > 1) filename = argv[1];
    if (!filesystem::exists(filename)) {
        cerr << "Archivo " << filename << " no encontrado.\n";
        WSACleanup();
        return 1;
    }

    // 1. MEDIR EL TAMAÑO FÍSICO REAL EN DISCO
    unsigned long long totalBytes = filesystem::file_size(filename); 

    vector<WorkerConfig> WORKERS_LIST = {
        {"worker_1", "127.0.0.1", 5001},
        {"worker_2", "127.0.0.1", 5002},
        {"worker_3", "127.0.0.1", 5003},
        {"worker_4", "127.0.0.1", 5004},
        {"worker_5", "127.0.0.1", 5005}
    };
    
    int numWorkers = WORKERS_LIST.size();
    unsigned long long bytesPerWorkers = totalBytes / numWorkers;
    
    cout << "Archivo: " << filename << " (" << totalBytes << " bytes reales en disco)\n";
    cout << "Estrategia: Modo Streaming bajo demanda (Lazy Loading)\n";
    cout << "Dividiendo el archivo en " << numWorkers << " fragmentos...\n\n";
    
    Emsamblador emsamblador(WORKERS_LIST, 300000, 3);
    vector<future<DispatchResult>> futures;
    
    auto startDistribuido = chrono::steady_clock::now();
    
    // Repartir rangos de bytes de forma paralela a los workers (Ronda 1)
    for (int i = 0; i < numWorkers; ++i) {
        unsigned long long start = i * bytesPerWorkers;
        unsigned long long end = (i == numWorkers - 1) ? totalBytes : (i + 1) * bytesPerWorkers;
        
        json tarea;
        tarea["start"] = start;
        tarea["end"] = end;
        int workerIndex = i;
        
        futures.push_back(async(launch::async, [&emsamblador, tarea, workerIndex]() {
            return emsamblador.dispatch(tarea.dump(), workerIndex);
        }));
    }
    
    map<string, int> mapaGlobal;
    long palabrasTotalesDistribuidas = 0;
    vector<string> reporteWorkers;
    
    // Colecciones para rastrear el estado de la red y planear el Failover
    vector<RangoHuerfano> listaHuerfanos;
    vector<int> indexesWorkersSanos;

    cout << "Enviando fragmentos a los workers...\n";
    
    // Recolección y evaluación de la Ronda 1
    for (size_t i = 0; i < futures.size(); ++i) {
        DispatchResult res = futures[i].get();
        
        if (!res.success || res.rawBody.empty() || res.rawBody == "{}") {
            cout << " [¡ALERTA!] El [worker_" << (i + 1) << "] fallo. Guardando rango para redistribucion.\n";
            reporteWorkers.push_back(" -> El [worker_" + to_string(i + 1) + "] FALLO (Su carga fue redistribuida).");
            
            // Calculamos el rango exacto que este nodo no pudo procesar
            unsigned long long startHuerfano = i * bytesPerWorkers;
            unsigned long long endHuerfano = (i == numWorkers - 1) ? totalBytes : (i + 1) * bytesPerWorkers;
            listaHuerfanos.push_back({startHuerfano, endHuerfano});
            continue;
        }
        
        // Si el worker respondió bien, guardamos su índice como nodo de confianza
        indexesWorkersSanos.push_back(i);
        palabrasTotalesDistribuidas += res.totalWords;
        reporteWorkers.push_back(" -> El [worker_" + to_string(i + 1) + "] respondio exitosamente.");
        
        auto mapaWorker = json::parse(res.rawBody).get<map<string, int>>(); 
        for (auto const& [palabra, conteo] : mapaWorker) {
            mapaGlobal[palabra] += conteo;
        }
    }
    
    // --- RONDA 2 DE EMERGENCIA: REDISTRIBUCIÓN EN CALIENTE ---
    if (!listaHuerfanos.empty() && !indexesWorkersSanos.empty()) {
        cout << "\n[Failover] Iniciando redistribucion de " << listaHuerfanos.size() << " fragmento(s) huerfano(s)...\n";
        vector<future<DispatchResult>> futuresFailover;

        // Procesamos cada bloque de datos que quedó flotando
        for (const auto& huerfano : listaHuerfanos) {
            unsigned long long chunkHuerfanoTotal = huerfano.end - huerfano.start;
            // Dividimos el fragmento huérfano de manera equitativa entre la cantidad de nodos sanos
            unsigned long long subFragmento = chunkHuerfanoTotal / indexesWorkersSanos.size();

            for (size_t j = 0; j < indexesWorkersSanos.size(); ++j) {
                int targetWorkerIndex = indexesWorkersSanos[j];
                
                unsigned long long startSub = huerfano.start + (j * subFragmento);
                unsigned long long endSub = (j == indexesWorkersSanos.size() - 1) ? huerfano.end : huerfano.start + ((j + 1) * subFragmento);

                json tareaSub;
                tareaSub["start"] = startSub;
                tareaSub["end"] = endSub;

                cout << "  -> Reasignando sub-rango [" << startSub << " - " << endSub << "] al [worker_" << (targetWorkerIndex + 1) << "]\n";

                futuresFailover.push_back(async(launch::async, [&emsamblador, tareaSub, targetWorkerIndex]() {
                    return emsamblador.dispatch(tareaSub.dump(), targetWorkerIndex);
                }));
            }
        }

        // Recolectar las respuestas de la ronda de redistribución de emergencia
        for (size_t i = 0; i < futuresFailover.size(); ++i) {
            DispatchResult res = futuresFailover[i].get();
            if (res.success && !res.rawBody.empty() && res.rawBody != "{}") {
                palabrasTotalesDistribuidas += res.totalWords;
                auto mapaWorker = json::parse(res.rawBody).get<map<string, int>>(); 
                for (auto const& [palabra, conteo] : mapaWorker) {
                    mapaGlobal[palabra] += conteo;
                }
            }
        }
        cout << "[Failover] Redistribucion completada exitosamente.\n";
    } else if (indexesWorkersSanos.empty()) {
        cerr << "\n[ERROR CRÍTICO] Todos los workers de la red fallaron. No hay infraestructura viva.\n";
    }
    
    auto endDistribuido = chrono::steady_clock::now();
    double tiempoDistribuidoMs = chrono::duration<double, milli>(endDistribuido - startDistribuido).count();
    cout << "PROCESO DISTRIBUIDO TERMINADO\n\n";
    
    char opcion;
    cout << "¿Desea iniciar la auditoria local secuencial para verificacion de los mapas? (y/n): ";
    cin >> opcion;
    
    bool ejecutoLocal = false;
    double tiempoLocalMs = 0.0;
    long palabrasTotalesLocal = 0;
    map<string, int> mapaAuditoriaLocal;

    if (opcion == 'y' || opcion == 'Y') {
        cout << "\nIniciando auditoria local secuencial bajo demanda...\n";
        ejecutoLocal = true;
        auto startLocal = chrono::steady_clock::now();
        
        ifstream fileLocal(filename, ios::binary);
        if (fileLocal.is_open()) {
            const size_t CHUNK_AUDITORIA = 10 * 1024 * 1024; 
            vector<char> buffer(CHUNK_AUDITORIA);
            string residuo = "";

            while (fileLocal.good()) {
                fileLocal.read(buffer.data(), CHUNK_AUDITORIA);
                size_t leidos = fileLocal.gcount();
                if (leidos == 0) break;

                string bloque = residuo + string(buffer.data(), leidos);
                
                for (char& c : bloque) {
                    if (!isalnum(static_cast<unsigned char>(c))) c = ' ';
                    else c = tolower(static_cast<unsigned char>(c));
                }

                stringstream ss(bloque);
                string palabra;
                string ultimaPalabra = "";

                while (ss >> palabra) {
                    ultimaPalabra = palabra;
                    if (palabra.size() > 3) {
                        mapaAuditoriaLocal[palabra]++;
                    }
                }

                if (!bloque.empty() && bloque.back() != ' ') {
                    if (!ultimaPalabra.empty() && mapaAuditoriaLocal[ultimaPalabra] > 0) {
                        mapaAuditoriaLocal[ultimaPalabra]--;
                        if (mapaAuditoriaLocal[ultimaPalabra] == 0) {
                            mapaAuditoriaLocal.erase(ultimaPalabra);
                        }
                    }
                    residuo = ultimaPalabra;
                } else {
                    residuo = "";
                }
            }
            if (!residuo.empty() && residuo.size() > 3) {
                mapaAuditoriaLocal[residuo]++;
            }
            fileLocal.close();
        } else {
            cerr << "Error: No se pudo abrir el archivo para la auditoria local.\n";
        }
        
        auto endLocal = chrono::steady_clock::now();
        tiempoLocalMs = chrono::duration<double, milli>(endLocal - startLocal).count();
        
        for (auto const& [palabra, conteo] : mapaAuditoriaLocal) {
            palabrasTotalesLocal += conteo;
        }
        cout << "Proceso local de verificacion completado.\n\n";
    } else {
        cout << "\n[Aviso] Se omitio el analisis secuencial local por seleccion del usuario.\n\n";
    }

    // ==========================================================================================
    // CONSOLIDADO MÉTRICO FINAL
    // ==========================================================================================
    cout << "=======================================================\n";
    cout << "         RESUMEN DE PROCESAMIENTO Y AUDITORIA          \n";
    cout << "=======================================================\n";
    cout << "[LOG INDIVIDUAL DE WORKERS]:\n";
    for (const string& log : reporteWorkers) {
        cout << log << "\n";
    }
    cout << "-------------------------------------------------------\n";
    cout << "Total de palabras unicas (Diccionario global): " << mapaGlobal.size() << "\n";
    cout << "-------------------------------------------------------\n";
    cout << "VOLUMEN DE PALABRAS CONTADAS (MAYORES A 3 CARACTERES):\n";
    cout << " -> Conteo Total en Red (Distribuido): " << palabrasTotalesDistribuidas << " palabras.\n";
    if (ejecutoLocal) {
        cout << " -> Conteo Total en un Hilo (Local):   " << palabrasTotalesLocal << " palabras.\n";
        long diferenciaPalabras = abs(palabrasTotalesLocal - palabrasTotalesDistribuidas);
        cout << " -> Discrepancia por cortes de frontera: " << diferenciaPalabras << " palabras.\n";
    }
    cout << "-------------------------------------------------------\n";
    cout << "TIEMPOS DE EJECUCION:\n";
    cout << " -> Tiempo del proceso distribuido: " << tiempoDistribuidoMs << " ms.\n";

    if (ejecutoLocal) {
        cout << " -> Tiempo del proceso local:       " << tiempoLocalMs << " ms.\n";
        cout << "-------------------------------------------------------\n";

        // Verificación de los mapas
        bool todoOk = (mapaGlobal == mapaAuditoriaLocal);
        cout << ">>> VERIFICACION DE LOGICA: ";
        if (todoOk) {
            cout << "¡CORRECTO! Los resultados coinciden exactamente.\n";
        } else {
            cout << "¡ALERTA! Los mapas difieren en caracteres internos (Revisar tokenizacion).\n";
        }

        // Comparativa de velocidad
        cout << ">>> COMPARATIVA DE VELOCIDAD: ";
        if (tiempoDistribuidoMs < tiempoLocalMs) {
            cout << "El proceso DISTRIBUIDO fue mas rapido por " << (tiempoLocalMs - tiempoDistribuidoMs) << " ms.\n";
        } else {
            cout << "El proceso LOCAL SECUENCIAL fue mas rapido por " << (tiempoDistribuidoMs - tiempoLocalMs) << " ms.\n";
        }
    } else {
        cout << "-------------------------------------------------------\n";
        cout << ">>> VERIFICACION DE LOGICA: No ejecutada.\n";
    }
    cout << "=======================================================\n";
    WSACleanup();
    return 0;
}