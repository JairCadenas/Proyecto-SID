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
//Librerias Nativas de red
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;
using json = nlohmann::json;
map<string, int> conteoLocalSecuencial(const string& texto) {
    map<string, int> mapaLocal;
    string textoLimpio=texto;
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
int main(int argc,char*argv[]){
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "[Coordinador] Error critico al inicializar Winsock (Infraestructura de red).\n";
        return 1;
    }
    string filename = "texto.txt";
    if (argc>1) filename = argv[1];
    if (!filesystem::exists(filename)) {
        cerr << "Archivo " << filename << " no encontrado.\n";
        WSACleanup();
        return 1;
    }
    ifstream file(filename);
    if(!file.is_open()){
        cerr<<"No se pudo abrir el archivo: "<<filename<<"\n";
        WSACleanup();
        return 1;
    }
    stringstream buffer;
    buffer << file.rdbuf();
    string textoCompleto = buffer.str();
    vector<WorkerConfig> WORKERS_LIST={ //Configuración de los Equipos
        {"worker_1", "127.0.0.1", 5001},     // Tu misma PC
        {"worker_2", "127.0.0.1", 5002},  // Otra PC o VM
        {"worker_3", "127.0.0.1", 5003},   // Otra PC o VM
        {"worker_4", "127.0.0.1", 5004},
        {"worker_5", "127.0.0.1", 5005}
    };
    //Carga el archivo  en la memoria del cordinador para medirlo
    long totalBytes = textoCompleto.size();
    int numWorkers = WORKERS_LIST.size();
    long bytesPerWorkes = totalBytes / numWorkers;
    cout<<"Coordinador iniciado"<<std::endl;
    cout<<"Archivo: " << filename << "("<<totalBytes<<" bytes)\n";
    cout<<"Divide el texto en "<<numWorkers<<" fragmentos\n";
    Emsamblador emsamblador(WORKERS_LIST, 300000,3);
    vector<future<DispatchResult>>futures;
    auto startDistribuido = chrono::steady_clock::now();
    //Reparte Rangos de bytes de forma paralela
    for (int i=0;i<numWorkers; ++i){
        long start = i *bytesPerWorkes;
        //El ultimo worker se lleva el residuo exacto hasta el final del documento
        long end = (i == numWorkers -1) ? totalBytes : (i+1)*bytesPerWorkes;
        //Mandamos el Rango que le toca procesar con JSON
        json tarea;
        tarea["start"]=start;
        tarea["end"]=end;
        int workerIndex = i;
        //lanza un hilo en paralelo para que el Emsamblador hable por red con el worker
        futures.push_back(async(launch::async, [&emsamblador, tarea, workerIndex]() {
            return emsamblador.dispatch(tarea.dump(), workerIndex);
        }));
    }
    map<string,int>mapaGlobal;
    long palabrasTotalesDistribuidas = 0;
    vector<string>reporteWorkers; //ALmacena de fora temporl el status de los hilos
    cout<<"Enviando fragmentos a los workers...\n";
    for(size_t i=0; i<futures.size();++i){
        // El coordinador espera de forma ordenada a que cada hilo responda
        DispatchResult res = futures[i].get();
        // Si el JSON viene vacío, significa que el worker falló o su Circuit Breaker se activó
        if (!res.success || res.rawBody.empty() || res.rawBody== "{}") {
            reporteWorkers.push_back(" -> El [worker_" + to_string(i + 1) + "] no pudo completar su tarea o se desconecto.");
            continue;
        }
        // Almacenamos el log limpio del Worker para el resumen finalizado del fondo
        palabrasTotalesDistribuidas += res.totalWords;
        reporteWorkers.push_back(" -> El [worker_" + to_string(i + 1) + "] respondio exitosamente.");
        // nlohmann/json convierte el texto JSON directamente a un mapa std::map de C++
        auto mapaWorker = json::parse(res.rawBody).get<map<string, int>>(); 
        // Mezclar y sumar las frecuencias del Worker en el Mapa Global
        for (auto const& [palabra, conteo] : mapaWorker) {
            mapaGlobal[palabra] += conteo;
        }
    }
    auto endDistribuido = chrono::steady_clock::now();
    double tiempoDistribuidoMs = chrono::duration<double, milli>(endDistribuido - startDistribuido).count();
    cout << "PROCESO DISTRIBUIDO TERMINADO\n";
    char opcion;
    cout << "¿Desea iniciar la auditoria local secuencial para verificacion de los mapas? (y/n): ";
    cin >> opcion;
    bool ejecutoLocal = false;
    double tiempoLocalMs = 0.0;
    long palabrasTotalesLocal = 0;
    map<string, int> mapaAuditoriaLocal;

    if (opcion == 'y' || opcion == 'Y') {
        cout << "\nIniciando segundo conteo local secuencial para verificacion...\n";
        ejecutoLocal = true;
        auto startLocal = chrono::steady_clock::now();
        
        mapaAuditoriaLocal = conteoLocalSecuencial(textoCompleto);
        
        auto endLocal = chrono::steady_clock::now();
        tiempoLocalMs = chrono::duration<double, milli>(endLocal - startLocal).count();
        // Calcular la cantidad de palabras brutas encontradas en la auditoría local
        for (auto const& [palabra, conteo] : mapaAuditoriaLocal) {
            palabrasTotalesLocal += conteo;
        }
        cout << "Proceso local de verificacion completado en hilos internos.\n\n";
    } else {
        cout << "\n[Aviso] Se omitio el analisis secuencial local por seleccion del usuario.\n\n";
    }

    // ==========================================================================================
    // VACIADO DE DICCIONARIO (A la mitad para que suba de largo en la consola)
    // ==========================================================================================
    cout << "Frecuencia total\n";
    // Este bucle recorrerá el mapa completo de principio a fin mostrando TODO el diccionario
    for (auto const& [palabra, conteo] : mapaGlobal) {
        cout << "Palabra: [" << palabra << "] aparece " << conteo << " veces.\n";
    }
    cout << "=======================================================\n\n";

    // ==========================================================================================
    // CONSOLIDADO METRICO FINAL (Lo último que se quedará estático en el fondo de la pantalla)
    // ==========================================================================================
    cout << "         RESUMEN DE PROCESAMIENTO Y AUDITORIA          \n";
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

        // Verificación exacta de que los mapas son idénticos (Auditoría lógica)
        bool todoOk = (mapaGlobal == mapaAuditoriaLocal);
        cout << ">>> VERIFICACION DE LOGICA: ";
        if (todoOk) {
            cout << "¡CORRECTO! Los resultados coinciden exactamente.\n";
        } else {
            cout << "¡ALERTA! Los mapas no coinciden debido al desfase de fronteras.\n";
        }

        // Comparativa directa de velocidad exigida por el profesor
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
