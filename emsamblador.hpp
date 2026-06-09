#pragma once
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <chrono>
#include <stdexcept>
#include <thread>
#include "nlohmann/json.hpp"
// Librerías nativas de red para Windows (Winsock)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

using json = nlohmann::json;

struct WorkerConfig {
    std::string id;
    std::string host;
    int         port;
};

struct DispatchResult {
    bool         success = false;  // Indica si la operación por red fue exitosa
    std::string workerId;         // Qué máquina respondió
    long         totalWords = 0;   // El entero directo con la suma de palabras del fragmento
    double       elapsedMs = 0.0;  // Cuánto tardó la llamada de red en milisegundos
    int          attempts  = 0;    // Cuántas veces se intentó reconectar antes de tirar la toalla
    std::string rawBody;          // Almacenamiento crudo de la respuesta JSON por seguridad
};

class Emsamblador {
public:
    // Constructor con soporte para Timeouts extendidos de forma nativa
    Emsamblador(const std::vector<WorkerConfig>& workers, int timeoutMs = 300000, int maxRetries = 3)
        : workers_(workers), timeoutMs_(timeoutMs), maxRetries_(maxRetries) {}

    DispatchResult dispatch(const std::string& requestBody, int workerIndex) {
        auto start = std::chrono::steady_clock::now();
        DispatchResult res;
        res.workerId = "worker_desconocido";
        
        if (workerIndex < 0 || workerIndex >= (int)workers_.size()) {
            res.success = false;
            return res;
        }

        WorkerConfig targetWorker = workers_[workerIndex];
        res.workerId = targetWorker.id;

        std::string lastError = "Ninguno";
        
        for (int attempt = 1; attempt <= maxRetries_; ++attempt) {
            res.attempts = attempt;
            SOCKET socket_fd = INVALID_SOCKET;

            try {
                // 1. Crear el socket del cliente
                socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (socket_fd == INVALID_SOCKET) {
                    throw std::runtime_error("No se pudo crear el socket TCP");
                }

                // Asignar timeouts nativos estables en Windows
                int timeoutConfig = timeoutMs_; 
                setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutConfig, sizeof(timeoutConfig));
                setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeoutConfig, sizeof(timeoutConfig));

                // 2. Configurar la dirección del Worker destino
                sockaddr_in serverAddr;
                serverAddr.sin_family = AF_INET;
                serverAddr.sin_port = htons(targetWorker.port);
                inet_pton(AF_INET, targetWorker.host.c_str(), &serverAddr.sin_addr);

                // 3. Conectarse al Worker
                if (connect(socket_fd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                    throw std::runtime_error("Conexion rechazada por el servidor destino");
                }

                // 4. Enviar la tarea JSON al Worker
                int bytesEnviados = send(socket_fd, requestBody.c_str(), (int)requestBody.size(), 0);
                if (bytesEnviados == SOCKET_ERROR) {
                    throw std::runtime_error("Error enviando datos al socket de destino");
                }

                // 5. Bucle de lectura por ráfagas protegido
                std::string respuestaCompleta = "";
                char buffer[65536]; // Buffer de lectura de 64 KB
                
                while (true) {
                    // Limpieza preventiva de seguridad del buffer receptor
                    std::fill(std::begin(buffer), std::end(buffer), 0);
                    
                    int bytesLeidos = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
                    
                    if (bytesLeidos > 0) {
                        respuestaCompleta.append(buffer, bytesLeidos);
                        
                        // CORRECCIÓN INTERMEDIA: Si el buffer termina en '}', verificamos si el JSON ya cerró completamente.
                        // Esto evita quedarnos colgados en la red esperando un fin de flujo de un socket que ya se va a cerrar.
                        if (!respuestaCompleta.empty() && respuestaCompleta.back() == '}') {
                            if (json::accept(respuestaCompleta)) {
                                break; 
                            }
                        }
                    } else if (bytesLeidos == 0) {
                        // El flujo finalizó de forma correcta por el cierre estándar del socket
                        break; 
                    } else {
                        int errorNativo = WSAGetLastError();
                        
                        if (errorNativo == WSAEWOULDBLOCK || errorNativo == WSAEINPROGRESS) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(20));
                            continue;
                        }
                        // Si da error de reset pero el JSON ya está completo en el string, lo aceptamos como válido
                        if (errorNativo == WSAECONNRESET && !respuestaCompleta.empty()) {
                            if (json::accept(respuestaCompleta)) {
                                break;
                            }
                        }
                        throw std::runtime_error("Desconexion abrupta del socket durante la transferencia masiva (Error: " + std::to_string(errorNativo) + ")");
                    }
                }

                if (respuestaCompleta.empty()) {
                    throw std::runtime_error("El Worker respondio un flujo de datos vacio");
                }

                res.rawBody = respuestaCompleta;
                
                // Parsear el diccionario recibido para acumular el volumen total de palabras distribuidas
                json parsed = json::parse(respuestaCompleta);
                res.totalWords = 0;
                for (auto it = parsed.begin(); it != parsed.end(); ++it) {
                    res.totalWords += it.value().get<long>();
                }

                res.success = true;
                auto end = std::chrono::steady_clock::now();
                res.elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
                
                closesocket(socket_fd);
                return res; // Éxito total, salimos del bucle de intentos

            } catch (const std::exception& e) {
                lastError = e.what();
                if (socket_fd != INVALID_SOCKET) {
                    closesocket(socket_fd);
                }
            }

            if (attempt < maxRetries_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
            }
        }

        auto end = std::chrono::steady_clock::now();
        res.elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
        res.success = false;
        std::cout << "  [Emsamblador] Error critico enviando tarea al [" << targetWorker.id 
                  << "] tras " << res.attempts << " intentos. Razon: " << lastError << "\n";      
        return res;
    }

private:
    std::vector<WorkerConfig> workers_;
    int                       timeoutMs_;
    int                       maxRetries_;
};