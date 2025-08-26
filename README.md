# Inter-Process Communication Project

A comprehensive C++23 application demonstrating three distinct IPC mechanisms with a web-based interface for visualization and control.

## Overview

This project implements and compares three fundamental Inter-Process Communication mechanisms:
- **Anonymous Pipes** - Simple unidirectional communication
- **Local Sockets** - Bidirectional network-style communication
- **Shared Memory** - High-performance direct memory access

The application features a HTML/JavaScript frontend that allows real-time visualization of data flow and performance metrics for each IPC method.

### Project Structure
```
ipc-project/
├── README.md
├── backend/
│   ├── src/
│   │   ├── main.cpp
│   │   ├── server/
│   │   │   ├── http_server.cpp
│   │   │   └── http_server.h
│   │   ├── ipc/
│   │   │   ├── pipe_manager.cpp
│   │   │   ├── pipe_manager.h
│   │   │   ├── socket_manager.cpp
│   │   │   ├── socket_manager.h
│   │   │   ├── shmem_manager.cpp
│   │   │   └── shmem_manager.h
│   │   └── common/
│   │       ├── logger.cpp
│   │       └── logger.h
│   ├── tests/
│   └── 
├── frontend/
│   └── index.html
└── tests/
```

### System Architecture
```
┌─────────────────┐     HTTP/REST     ┌─────────────────┐
│     Frontend    │ ←──────────────→  │     Backend     │
│     (HTML)      │                   │     (C++23)     │
└─────────────────┘                   └─────────────────┘
                                               │
                                               ▼
                                      ┌─────────────────┐
                                      │  IPC Managers   │
                                      │                 │
                                      │  • PipeManager  │
                                      │  • SocketManager│
                                      │  • ShmemMgr     │
                                      └─────────────────┘
```

## Features

### Backend (C++23)

### Frontend (HTML/JavaScript)

## Prerequisites

### Required

### Optional

## Building

### Quick Start


### Development Build

## Usage

1. **Start the backend server**:
 

2. **Open the web interface**:


3. **Test IPC mechanisms**:


### Manual Testing

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Author

**Bruno Henrique Foggiatto**
- GitHub: [@brunofoggiatto](https://github.com/brunofoggiatto)
- Email: brunohfoggiatto@gmail.com

---

**Note**: This project is designed for educational purposes to demonstrate IPC mechanisms and modern C++ development practices.