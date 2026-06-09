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
    long totalBytes = filesystem::file_size(filename); 

    vector<WorkerConfig> WORKERS_LIST = {
        {"worker_1", "127.0.0.1", 5001},
        {"worker_2", "127.0.0.1", 5002},
        {"worker_3", "127.0.0.1", 5003},
        {"worker_4", "127.0.0.1", 5004},
        {"worker_5", "127.0.0.1", 5005}
    };
    
    int numWorkers = WORKERS_LIST.size();
    long bytesPerWorkes = totalBytes / numWorkers;
    
    cout << "=======================================================\n";
    cout << "            COORDINADOR DISTRIBUIDO INICIADO           \n";
    cout << "=======================================================\n";
    cout << "Archivo: " << filename << " (" << totalBytes << " bytes reales en disco)\n";
    cout << "Estrategia: Modo Streaming bajo demanda (Lazy Loading)\n";
    cout << "Dividiendo el archivo en " << numWorkers << " fragmentos...\n\n";
    
    Emsamblador emsamblador(WORKERS_LIST, 300000, 3);
    vector<future<DispatchResult>> futures;
    
    auto startDistribuido = chrono::steady_clock::now();
    
    // Repartir rangos de bytes de forma paralela a los workers
    for (int i = 0; i < numWorkers; ++i) {
        long start = i * bytesPerWorkes;
        long end = (i == numWorkers - 1) ? totalBytes : (i + 1) * bytesPerWorkes;
        
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
    cout << "Enviando fragmentos a los workers...\n";
    
    for (size_t i = 0; i < futures.size(); ++i) {
        DispatchResult res = futures[i].get();
        if (!res.success || res.rawBody.empty() || res.rawBody == "{}") {
            reporteWorkers.push_back(" -> El [worker_" + to_string(i + 1) + "] no pudo completar su tarea o se desconecto.");
            continue;
        }
        
        palabrasTotalesDistribuidas += res.totalWords;
        reporteWorkers.push_back(" -> El [worker_" + to_string(i + 1) + "] respondio exitosamente.");
        auto mapaWorker = json::parse(res.rawBody).get<map<string, int>>(); 
        for (auto const& [palabra, conteo] : mapaWorker) {
            mapaGlobal[palabra] += conteo;
        }
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
            string textoCompletoBinario(totalBytes, '\0');
            fileLocal.read(&textoCompletoBinario[0], totalBytes);
            fileLocal.close();
            
            mapaAuditoriaLocal = conteoLocalSecuencial(textoCompletoBinario);
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