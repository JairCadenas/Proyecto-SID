@echo off
:: Configura la terminal para que muestre bien los acentos y emojis modernos
chcp 65001 > nul
cls
echo COMPILADOR AUTOMÁTICO
echo.
echo [Paso 1 de 2] Creando el programa del Trabajador...
g++ -std=c++17 -O2 -o worker.exe worker.cpp -lws2_32
echo  ¡Listo! Se creó el archivo "worker.exe".
echo.
echo [Paso 2 de 2] Creando el programa del Jefe (Coordinador)...
g++ -std=c++17 -O2 -o coordinator.exe coordinador.cpp -lws2_32
echo   ¡Listo! Se creó el archivo "coordinator.exe".
echo.
echo   COMPILACIÓN COMPLETADA EXITOSAMENTE
echo   Tus dos programas ya están listos en esta carpeta.
echo.
echo   PASOS A SEGUIR AHORA MISMO PARA PASARLO A CORRER:
echo.
echo   1. Abre 3 ventanas de CMD independientes y enciende los Workers:
echo      • Ventana 1: worker.exe --id worker_1 --port 5001 --file Alice_in_Wonderland.txt
echo      • Ventana 2: worker.exe --id worker_2 --port 5002 --file Alice_in_Wonderland.txt
echo      • Ventana 3: worker.exe --id worker_3 --port 5003 --file Alice_in_Wonderland.txt
echo      • Ventana 3: worker.exe --id worker_4 --port 5004 --file Alice_in_Wonderland.txt
echo      • Ventana 3: worker.exe --id worker_5 --port 5005 --file Alice_in_Wonderland.txt
echo.
echo   2. Abre una 4ta ventana de CMD y dale la orden al Jefe:
echo      • Ventana 4: coordinator.exe Alice_in_Wonderland.txt
echo.