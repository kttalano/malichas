# 🛍️ Malichas POS & Inventory Management System

*(Lee la versión en español más abajo 🇪🇸)*

An integral, high-performance Point of Sale (POS) and Inventory Management system built from scratch in C++. This project bridges local inventory management with e-commerce platforms (Empretienda) using a robust multithreaded architecture, real-time socket synchronization, and an embedded REST API.

> **⚠️ Disclaimer / Important Notice:**
> This repository contains a **showcase/test version** of the source code. It is provided strictly for code review, architectural analysis, and portfolio purposes. **It is not expected to run out-of-the-box** as sensitive configuration files, API keys (like Gemini AI tokens), and actual database files have been removed for security reasons. 
> 
> *If you are interested in a fully functional version or a commercial implementation, please contact me directly through my GitHub profile.*

## 🚀 Key Features

*   **Offline-First & Real-Time Sync:** Custom SFML-based TCP socket protocol that synchronizes multiple client POS terminals in real-time. Uses an "Outbox Pattern" to queue tasks when offline.
*   **Smart E-Commerce Sync:** A Python-powered module that safely reads, compares, and writes Excel `.xlsx` files to keep the local database and the online store (Empretienda) perfectly synchronized.
*   **In-Memory Atomic Database:** Zero reliance on heavy SQL engines. Uses a thread-safe, lightning-fast JSON in-memory database with atomic disk writes to prevent data corruption.
*   **Integrated Web Dashboard:** Embedded `cpp-httplib` server serving a responsive Progressive Web App (PWA) with Dark Mode support to manage the inventory from any browser.
*   **Multithreading & Concurrency:** Advanced lock management using `std::recursive_mutex` and background threads to prevent UI freezes during heavy I/O operations.

## 📁 Project Structure

The repository is divided into two main environments:

*   `/Client`: Contains the frontend application intended to run on the physical POS terminals (cash registers).
*   `/Server` (or `MalichasServidor`): The core backend written in C++. It hosts the Web Dashboard, handles database I/O, runs the Python sync scripts, and accepts socket connections from the clients.

## 🛠️ How to Compile (For Code Review)

Compilation scripts are provided for both Windows and Linux environments. Ensure you have `g++` (GCC) and the required SFML libraries installed.

**For Linux:**
```bash
chmod +x compile.sh
./compile.sh
```

**For Windows:**
Simply double-click the `compile.bat` file or run it via Command Prompt:
```cmd
compile.bat
```

---
---

# 🇪🇸 Malichas - Sistema POS y Gestión de Inventario

Un sistema integral de Punto de Venta (POS) y gestión de inventario de alto rendimiento construido desde cero en C++. Este proyecto conecta la gestión de stock local con plataformas de e-commerce (Empretienda) utilizando una arquitectura robusta multihilo, sincronización en tiempo real por sockets y una API REST integrada.

> **⚠️ Aviso Importante:**
> Este repositorio contiene una **versión de prueba/demostración** del código fuente. Se proporciona estrictamente para revisión de código, análisis arquitectónico y fines de portfolio. **No se espera que funcione "out-of-the-box"** ya que los archivos de configuración sensibles, claves de API (como los tokens de Gemini AI) y las bases de datos reales han sido eliminados por razones de seguridad.
> 
> *Si estás interesado en una versión completamente funcional o en una implementación comercial, por favor contáctame directamente a través de mi perfil de GitHub.*

## 🚀 Características Principales

*   **Sincronización en Tiempo Real (Offline-First):** Protocolo de sockets TCP personalizado basado en SFML que sincroniza múltiples terminales POS cliente en tiempo real. Utiliza un "Patrón Outbox" para encolar tareas cuando no hay internet.
*   **Sincronización Inteligente E-Commerce:** Un módulo impulsado por Python que lee, compara y escribe archivos Excel `.xlsx` de forma segura para mantener la base de datos local y la tienda online (Empretienda) perfectamente sincronizadas.
*   **Base de Datos Atómica en Memoria:** Sin dependencias de motores SQL pesados. Utiliza una base de datos JSON en memoria ultrarrápida y segura para subprocesos (thread-safe), con escrituras atómicas en disco para evitar la corrupción de datos.
*   **Dashboard Web Integrado:** Servidor `cpp-httplib` embebido que aloja una Aplicación Web Progresiva (PWA) responsiva con soporte para Modo Oscuro, permitiendo administrar el inventario desde cualquier navegador.
*   **Multithreading y Concurrencia:** Gestión avanzada de bloqueos utilizando `std::recursive_mutex` y subprocesos en segundo plano para evitar que la interfaz se congele durante operaciones pesadas de lectura/escritura.

## 📁 Estructura del Proyecto

El repositorio está dividido en dos entornos principales:

*   `/Client` (Cliente): Contiene la aplicación frontend diseñada para ejecutarse en las terminales físicas de punto de venta (cajas registradoras).
*   `/Server` (Servidor): El núcleo backend escrito en C++. Aloja el panel web, maneja la lectura/escritura de la base de datos, ejecuta los scripts de sincronización de Python y acepta las conexiones de red de los clientes.

## 🛠️ Cómo Compilar (Para revisión de código)

Se proporcionan scripts de compilación tanto para entornos Windows como Linux. Asegúrate de tener `g++` (GCC) y las librerías de SFML requeridas instaladas.

**Para Linux:**
```bash
chmod +x compile.sh
./compile.sh
```

**Para Windows:**
Simplemente haz doble clic en el archivo `compile.bat` o ejecútalo a través del Símbolo del sistema:
```cmd
compile.bat
```
