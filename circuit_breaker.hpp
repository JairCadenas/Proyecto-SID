#pragma once
#include <string>
#include <mutex>
#include <chrono>
#include <iostream>
enum class CBState { CLOSED, OPEN, HALF_OPEN };

inline std::string stateStr(CBState s) {
    switch (s) {
        case CBState::CLOSED:    return "CLOSED";
        case CBState::OPEN:      return "OPEN";
        case CBState::HALF_OPEN: return "HALF_OPEN";
    }
    return "UNKNOWN";
}
class CircuitBreaker {
public:
    // Inicializa las políticas de tolerancia a fallos
    CircuitBreaker(
        std::string  name,               
        int          failureThreshold = 3,// Cuántos fallos seguidos abren el circuito
        double       recoverySeconds  = 10.0, // Tiempo que se queda bloqueado en OPEN antes de pasar a HALF_OPEN
        int          successThreshold = 1 // Cuántos éxitos en HALF_OPEN se necesitan para volver a CLOSED
    )
        : name_(std::move(name))
        , failureThreshold_(failureThreshold)
        , recoverySeconds_(recoverySeconds)
        , successThreshold_(successThreshold)
        , state_(CBState::CLOSED)
        , failureCount_(0)
        , successCount_(0)
        , lastFailureTime_{}
    {}

    /* allowRequest()
     * Es llamada por el Worker antes de ejecutar la lógica de conteo.
     * Determina si la petición HTTP entrante debe procesarse o rechazarse de inmediato.
     */
    bool allowRequest() {
        // Exclusión mutua (Thread-safety): Evita condiciones de carrera si entran múltiples peticiones Http concurrentes
        std::lock_guard<std::mutex> lk(mtx_);
        // Verifica si el tiempo de castigo en estado OPEN ya expiró para pasar a modo de prueba (HALF_OPEN)
        updateState();
        if (state_ == CBState::CLOSED)    return true;  // Pasa con normalidad
        if (state_ == CBState::OPEN)      return false; // El fusible saltó; bloquea la petición
        return true; // Si está en HALF_OPEN, deja pasar una petición piloto para probar salud
    }
    /* recordSuccess()
     * Se invoca cuando el Worker termina de contar las palabras con éxito sin arrojar excepciones.
     */
    void recordSuccess() {
        std::lock_guard<std::mutex> lk(mtx_);
        if (state_ == CBState::HALF_OPEN) {
            successCount_++; // Contabiliza el éxito piloto
            // Si alcanzamos los éxitos requeridos en la prueba, el circuito sana y se cierra de nuevo
            if (successCount_ >= successThreshold_) {
                transition(CBState::CLOSED);
            }
        } else if (state_ == CBState::CLOSED) {
            failureCount_ = 0; // Limpia fallos acumulados esporádicos del pasado
        }
    }
    /* recordFailure()
     * Se invoca dentro del bloque catch(...) del Worker si la lectura del archivo o procesamiento falló.
     */
    void recordFailure() {
        std::lock_guard<std::mutex> lk(mtx_);
        // Marca la estampa de tiempo exacta del último error para calcular el tiempo de gracia posterior
        lastFailureTime_ = std::chrono::steady_clock::now();
        successCount_ = 0; // Si estábamos en prueba en HALF_OPEN y falló, reinicia el progreso de cura
        updateState();
        if (state_ == CBState::CLOSED || state_ == CBState::HALF_OPEN) {
            failureCount_++; // Incrementa el contador de fallos consecutivos
            // Si se supera el límite de tolerancia tolerado, el fusible se dispara (OPEN)
            if (failureCount_ >= failureThreshold_) {
                transition(CBState::OPEN);
            }
        }
    }
    // Getters simples para bitácora
    const std::string& getName() const { return name_; }
    std::string getStateStr() { 
        std::lock_guard<std::mutex> lk(mtx_);
        updateState();
        return stateStr(state_); 
    }
private:
    std::string  name_;
    int          failureThreshold_;
    double       recoverySeconds_;
    int          successThreshold_;
    CBState      state_;
    int          failureCount_;
    int          successCount_;
    std::chrono::steady_clock::time_point lastFailureTime_; // Almacena cuándo ocurrió el último desastre
    std::mutex   mtx_; // Mutex interno para blindar el objeto contra accesos multi-hilo
    /* updateState()
     * Evalúa dinámicamente el paso del tiempo real. Si el fusible saltó (OPEN) pero ya pasaron los
     * segundos de cooldown configurados, se transiciona automáticamente a HALF_OPEN.
     */
    void updateState() {
        if (state_ == CBState::OPEN) {
            auto now     = std::chrono::steady_clock::now();
            // Calcula la diferencia en segundos flotantes entre "ahora" y el "momento de la falla"
            double secs  = std::chrono::duration<double>(now - lastFailureTime_).count();
            if (secs >= recoverySeconds_) {
                transition(CBState::HALF_OPEN); // El tiempo de penalización expiró, entra en fase de testeo
            }
        }
    }
    /* transition()
     * Helper interno para cambiar el estado de la máquina de estados y escribirlo de forma clara en la consola.
     */
    void transition(CBState newState) {
        std::string old = stateStr(state_);
        state_ = newState;
        std::cout << "  [CircuitBreaker Local:" << name_ << "] Estado cambio: "
                  << old << " -> " << stateStr(newState) << "\n";
    }
};