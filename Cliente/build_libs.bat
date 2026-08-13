@echo off
echo Compilando librerias (solo una vez)...
g++ -std=c++17 -O3 -c imgui/*.cpp -I./imgui
g++ -std=c++17 -O3 -c imgui-sfml/*.cpp -I./imgui -I./imgui-sfml -I./SFML/include
echo Librerias compiladas.
pause