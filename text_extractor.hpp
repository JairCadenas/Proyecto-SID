#pragma once
/**
 * text_extractor.hpp
 * Extrae texto plano de archivos .txt, .epub y .pdf
 * Sin dependencias externas — solo STL puro.
 */

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <regex>
#include <filesystem>
#include <iostream>

// Helper: Convierte un texto a minúsculas
inline std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

// Helper: Extrae la extensión del archivo (ej: "libro.txt" -> ".txt")
inline std::string getExtension(const std::string& path) {
    auto pos = path.rfind('.');
    if (pos == std::string::npos) return "";
    return toLower(path.substr(pos));
}

// ── EXTRACTOR PARA ARCHIVOS .TXT ──────────────────────────────────────────
inline std::string extractTxt(const std::string& path) {
    // Abre el archivo plano en modo binario
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) throw std::runtime_error("No se pudo abrir TXT: " + path);
    
    std::stringstream ss;
    ss << f.rdbuf(); // Vuelca todo el archivo de golpe a la memoria
    return ss.str();
}
// ── FUNCIÓN PÚBLICA PRINCIPAL (Punto de acceso para el Worker) ──────────────
inline std::string extractText(const std::string& path) {
    // Valida físicamente que el archivo esté en la carpeta del disco
    if (!std::filesystem::exists(path))
        throw std::runtime_error("Archivo no encontrado: " + path);

    std::string ext = getExtension(path); // Analiza qué tipo de archivo es
    
    // Redirecciona al extractor correspondiente según el formato detectado
    if (ext == ".txt")  return extractTxt(path);
    
    // Error si el usuario pasa un archivo no soportado (ej. .docx o .mp3)
    throw std::runtime_error("Formato de archivo no soportado: " + ext);
}