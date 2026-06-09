#include <iostream>     // Para que la computadora pueda hablar en la pantalla.
#include <vector>       // Una lista súper mágica para guardar muchas cosas en orden.
#include <map>          // Un diccionario secreto: buscas un juguete y te dice cuántos tienes.
#include <future>       // Clones del futuro. Mandas a hacer tareas y te avisan cuando terminen.
#include <filesystem>   // El detective del disco duro. Busca archivos y mide su tamaño.
#include <fstream>      // El camión que abre los archivos pesados como si fueran libros gigantes.
#include <sstream>      // Una fábrica de palabras muy ordenada.
#include <chrono>       // El súper cronómetro para ver quién es el más rápido del oeste.
#include "nlohmann/json.hpp" // Un traductor universal para hablar con robots usando JSON (mensajes en cajas).
#include "emsamblador.hpp"   // El mensajero especial que viaja por los cables de internet.

// Bloques nativos para que Windows entienda qué es el internet y los enchufes de red (Sockets).
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib") // ¡Oye Windows, activa los cables de red ahora mismo!

using namespace std;    // Para no tener que escribir "std::" a cada rato (que da mucha flojera).
using json = nlohmann::json; // Un apodo corto para nuestro traductor de cajas JSON.

// --- ¡LA FÁBRICA LOCAL DE PALABRAS! ---
// Esta caja procesa todo el texto junto en un solo hilo de la computadora.
map<string, int> conteoLocalSecuencial(const string& texto) {
    map<string, int> mapaLocal; // Nuestro almacén vacío de palabras.
    string textoLimpio = texto; // Hacemos una copia para no ensuciar el texto original.
    
    // Pasamos un borrador por cada letra del cuento.
    for (char& c : textoLimpio) {
        // ¿Eres una letra o un número de verdad?
        if (!isalnum(static_cast<unsigned char>(c))) {
            c = ' '; // Si eres un punto, una coma o un bicho raro... ¡pum! Te convierto en espacio en blanco.
        } else {
            c = tolower(static_cast<unsigned char>(c)); // Si eres mayúscula, te encojo a minúscula. ¡Todos iguales!
        }
    }
    
    stringstream ss(textoLimpio); // Metemos el texto limpio en la máquina separadora.
    string palabra; // Aquí meteremos cada palabra que vaya saliendo de la máquina.
    
    // Sacamos palabras una por una de la fábrica.
    while (ss >> palabra) {
        // ¿Eres una palabra grande y fuerte de más de 3 letras? (Omitimos mini-palabras como "el", "un", "la").
        if (palabra.size() > 3) {
            mapaLocal[palabra]++; // ¡Te encontramos! Te sumamos 1 punto en el almacén.
        }
    }
    return mapaLocal; // Devolvemos el diccionario lleno al jefe.
}

// --- ¡EL CENTRO DE CONTROL PRINCIPAL (EL JEFE JUGUETÓN)! ---
int main(int argc, char* argv[]) {
    WSADATA wsaData; // La maleta donde Windows guarda la configuración del internet.
    
    // Encendemos la antena de red de Windows. Si falla, nos ponemos tristes y cerramos el programa.
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "[Coordinador] ¡Oh no! El internet de Windows está roto.\n";
        return 1;
    }
    
    string filename = "texto.txt"; // El cuento por defecto que vamos a leer se llama "texto.txt".
    
    // Si el usuario nos grita otro nombre al iniciar el programa, lo cambiamos.
    if (argc > 1) filename = argv[1];
    
    // Le preguntamos al detective del disco duro: "¿Existe ese cuento?"
    if (!filesystem::exists(filename)) {
        cerr << "¡Alerta! El libro llamado " << filename << " se perdió y no está en la biblioteca.\n";
        WSACleanup(); // Apagamos la antena de red antes de irnos a dormir.
        return 1;
    }

    // 1. EL DETECTIVE MIDE EL LIBRO (¡Gasta 0 MB de memoria RAM porque solo mira la etiqueta!)
    long totalBytes = filesystem::file_size(filename); 

    // Aquí anotamos las direcciones de nuestros 5 robots ayudantes (Workers) en la red.
    vector<WorkerConfig> WORKERS_LIST = {
        {"worker_1", "127.0.0.1", 5001}, // Robot 1 vive en el puerto 5001
        {"worker_2", "127.0.0.1", 5002}, // Robot 2 vive en el puerto 5002
        {"worker_3", "127.0.0.1", 5003}, // Robot 3 vive en el puerto 5003
        {"worker_4", "127.0.0.1", 5004}, // Robot 4 vive en el puerto 5004
        {"worker_5", "127.0.0.1", 5005}  // Robot 5 vive en el puerto 5005
    };
    
    int numWorkers = WORKERS_LIST.size(); // Contamos cuántos robots ayudantes tenemos (son 5).
    long bytesPerWorkes = totalBytes / numWorkers; // Dividimos las páginas del libro equitativamente entre los 5.
    
    // Mensajes divertidos para que el usuario vea qué está haciendo el programa.
    cout << "=======================================================\n";
    cout << "            COORDINADOR DISTRIBUIDO INICIADO           \n";
    cout << "=======================================================\n";
    cout << "Archivo: " << filename << " (" << totalBytes << " bytes reales en disco)\n";
    cout << "Estrategia: Modo Streaming bajo demanda (Lazy Loading)\n";
    cout << "Dividiendo el archivo en " << numWorkers << " fragmentos...\n\n";
    
    Emsamblador emsamblador(WORKERS_LIST, 300000, 3); // Creamos al mensajero. Esperará hasta 5 minutos y reintentará 3 veces si un robot se cae.
    vector<future<DispatchResult>> futures; // Una lista de hilos del futuro para vigilar a los robots que trabajan en paralelo.
    
    auto startDistribuido = chrono::steady_clock::now(); // ¡CLIC! Iniciamos el cronómetro para el trabajo en equipo por red.
    
    // --- ¡A REPARTIR LA TAREA A LOS ROBOTS! ---
    for (int i = 0; i < numWorkers; ++i) {
        long start = i * bytesPerWorkes; // ¿Desde qué página empieza a leer este robot?
        // Si es el último robot, se lleva todo lo que quede del libro. Si no, su pedazo exacto.
        long end = (i == numWorkers - 1) ? totalBytes : (i + 1) * bytesPerWorkes;
        
        json tarea; // Creamos una cajita de regalo JSON.
        tarea["start"] = start; // Guardamos la página de inicio dentro de la cajita.
        tarea["end"] = end;     // Guardamos la página de fin dentro de la cajita.
        int workerIndex = i;    // El número de la lista del robot actual.
        
        // Lanzamos un clon en paralelo (hilo de fondo). Corre a hablar por red sin detener al Jefe.
        futures.push_back(async(launch::async, [&emsamblador, tarea, workerIndex]() {
            return emsamblador.dispatch(tarea.dump(), workerIndex); // El mensajero corre, le da la tarea al robot y espera su respuesta.
        }));
    }
    
    map<string, int> mapaGlobal; // El Gran Baúl donde mezclaremos todas las respuestas de los robots.
    long palabrasTotalesDistribuidas = 0; // El contador general de palabras del equipo de robots.
    vector<string> reporteWorkers; // Un cuaderno de notas para ver qué robot se portó bien y cuál mal.
    cout << "Enviando fragmentos a los workers...\n";
    
    // --- ¡ESPERANDO LAS RESPUESTAS DE LA RED! ---
    for (size_t i = 0; i < futures.size(); ++i) {
        DispatchResult res = futures[i].get(); // Nos quedamos congelados vigilando el clon del futuro hasta que vuelva con la respuesta del robot.
        
        // ¿El robot explotó, mandó una caja vacía o se desconectó de la pared?
        if (!res.success || res.rawBody.empty() || res.rawBody == "{}") {
            reporteWorkers.push_back(" -> El [worker_" + to_string(i + 1) + "] no pudo completar su tarea o se desconecto.");
            continue; // Pasamos al siguiente robot, no podemos procesar basura.
        }
        
        palabrasTotalesDistribuidas += res.totalWords; // Sumamos las palabras que contó este robot al marcador global.
        reporteWorkers.push_back(" -> El [worker_" + to_string(i + 1) + "] respondio exitosamente."); // ¡Carita feliz para este robot!
        
        auto mapaWorker = json::parse(res.rawBody).get<map<string, int>>(); // Abrimos la caja JSON que nos mandó el robot y sacamos su diccionario.
        
        // Vaciamos el diccionario del robot dentro de nuestro Gran Baúl Global.
        for (auto const& [palabra, conteo] : mapaWorker) {
            mapaGlobal[palabra] += conteo; // Si la palabra ya existía, sumamos sus puntos. Si no, la anotamos por primera vez.
        }
    }
    
    auto endDistribuido = chrono::steady_clock::now(); // ¡CLIC! Detenemos el cronómetro del trabajo distribuido en equipo.
    double tiempoDistribuidoMs = chrono::duration<double, milli>(endDistribuido - startDistribuido).count(); // Calculamos cuántos milisegundos pasaron.
    cout << "PROCESO DISTRIBUIDO TERMINADO\n\n";
    
    // --- ¡LA AUDITORÍA BAJO DEMANDA! ---
    char opcion;
    cout << "¿Desea iniciar la auditoria local secuencial para verificacion de los mapas? (y/n): ";
    cin >> opcion; // Le preguntamos al usuario si quiere activar el modo lento de una sola CPU para comparar.
    
    bool ejecutoLocal = false; // Bandera para recordar si hicimos la prueba lenta o no.
    double tiempoLocalMs = 0.0; // Cronómetro para la prueba lenta.
    long palabrasTotalesLocal = 0; // Contador de palabras para la prueba lenta.
    map<string, int> mapaAuditoriaLocal; // Diccionario exclusivo para la prueba lenta.

    // Si el usuario presionó la 'y' de "Yes" (Sí)... ¡Comienza la carrera!
    if (opcion == 'y' || opcion == 'Y') {
        cout << "\nIniciando auditoria local secuencial bajo demanda...\n";
        ejecutoLocal = true;
        auto startLocal = chrono::steady_clock::now(); // ¡CLIC! Iniciamos cronómetro para la CPU solitaria.
        
        ifstream fileLocal(filename, ios::binary); // Abrimos el archivo gigante en modo binario (lectura exacta de bytes).
        if (fileLocal.is_open()) {
            string textoCompletoBinario(totalBytes, '\0'); // Creamos un mega contenedor de texto del tamaño exacto del libro.
            fileLocal.read(&textoCompletoBinario[0], totalBytes); // El camión succiona todo el libro completo de un solo golpe hacia la RAM.
            fileLocal.close(); // Cerramos el libro, ya lo guardamos en la RAM.
            
            mapaAuditoriaLocal = conteoLocalSecuencial(textoCompletoBinario); // Ponemos a trabajar a la fábrica local de palabras.
        } else {
            cerr << "Error: No se pudo abrir el archivo para la auditoria local.\n";
        }
        
        auto endLocal = chrono::steady_clock::now(); // ¡CLIC! Paramos el cronómetro de la CPU solitaria.
        tiempoLocalMs = chrono::duration<double, milli>(endLocal - startLocal).count(); // Calculamos su tiempo en milisegundos.
        
        // Contamos cuántas palabras encontró en total la CPU solitaria.
        for (auto const& [palabra, conteo] : mapaAuditoriaLocal) {
            palabrasTotalesLocal += conteo;
        }
        cout << "Proceso local de verificacion completado.\n\n";
    } else {
        cout << "\n[Aviso] Se omitio el analisis secuencial local por seleccion del usuario.\n\n";
    }

    // ==========================================================================================
    //           --- ¡EL GRAN MARCADOR FINAL DE LA GRAN CARRERA! ---
    // ==========================================================================================
    cout << "=======================================================\n";
    cout << "         RESUMEN DE PROCESAMIENTO Y AUDITORIA          \n";
    cout << "=======================================================\n";
    cout << "[LOG INDIVIDUAL DE WORKERS]:\n";
    for (const string& log : reporteWorkers) {
        cout << log << "\n"; // Imprimimos las notas de cómo se portó cada robot.
    }
    cout << "-------------------------------------------------------\n";
    cout << "Total de palabras unicas (Diccionario global): " << mapaGlobal.size() << "\n";
    cout << "-------------------------------------------------------\n";
    cout << "VOLUMEN DE PALABRAS CONTADAS (MAYORES A 3 CARACTERES):\n";
    cout << " -> Conteo Total en Red (Distribuido): " << palabrasTotalesDistribuidas << " palabras.\n";
    
    // Si se ejecutó la auditoría local, mostramos los dos puntajes para ver si coinciden.
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

        // Comparamos si los diccionarios son exactamente idénticos (Palabra por palabra y punto por punto).
        bool todoOk = (mapaGlobal == mapaAuditoriaLocal);
        cout << ">>> VERIFICACION DE LOGICA: ";
        if (todoOk) {
            cout << "¡CORRECTO! Los resultados coinciden exactamente.\n"; // ¡Felicidades! Los robots y el local hicieron magia perfecta.
        } else {
            cout << "¡ALERTA! Los mapas difieren en caracteres internos (Revisar tokenizacion).\n"; // Algo salió mal en las letras.
        }

        // Le presumimos al profesor quién ganó la carrera de velocidad.
        cout << ">>> COMPARATIVA DE VELOCIDAD: ";
        if (tiempoDistribuidoMs < tiempoLocalMs) {
            cout << "El proceso DISTRIBUIDO fue mas rapido por " << (tiempoLocalMs - tiempoDistribuidoMs) << " ms.\n"; // ¡Ganó el trabajo en equipo!
        } else {
            cout << "El proceso LOCAL SECUENCIAL fue mas rapido por " << (tiempoDistribuidoMs - tiempoLocalMs) << " ms.\n"; // Ganó la CPU solitaria.
        }
    } else {
        cout << "-------------------------------------------------------\n";
        cout << ">>> VERIFICACION DE LOGICA: No ejecutada.\n";
    }
    cout << "=======================================================\n";
    
    WSACleanup(); // Apagamos por completo y de forma limpia el sistema de internet de Windows.
    return 0; // Terminamos con éxito total. ¡Yuju!
}