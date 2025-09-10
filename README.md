# Inter-Process Communication Project

C++23 application demonstrating three IPC mechanisms with a real-time web interface for visualization and control.

## Overview

Implemented mechanisms:
- Anonymous Pipes (unidirectional)
- Local Sockets (AF_UNIX, bidirectional)
- Shared Memory (System V IPC + semaphores)

The frontend (HTML/JS) consumes a REST API exposed by the backend to:
- Start/Stop each mechanism
- Send messages (`/ipc/send`)
- Read general status (`/ipc/status`) and mechanism details (`/ipc/detail/{mechanism}`)

## Requirements

- Linux (System V IPC + AF_UNIX)
- GCC/G++ with C++23 support (recommended GCC 13+)
- CMake 3.14+
- GoogleTest installed on system (optional, for tests)

## Project Structure
```
backend/
  src/common/ (logger)
  src/ipc/    (pipe, socket, shmem, coordinator)
  src/server/ (http server)
  src/main.cpp
frontend/ (index.html, styles.css, script.js)
tests/    (unit and integration)
```

## Build

Quick start (with tests):
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
ctest --output-on-failure   # optional, runs tests
```

Generated binary: `build/bin/ipc_system`

## Execution

1) Start the backend in server mode (exposes API and serves frontend):
```bash
./build/bin/ipc_system --server [--port 9000]
```
- Default port: 9000 (use `--port <n>` to choose another)
- Server automatically searches for frontend in relative paths to binary (e.g.: `../../frontend`, `../frontend`, `./frontend`).

2) Open web interface:
```
http://localhost:9000/
```

3) Use the dashboard:
- Click Start/Stop on Pipes, Sockets or Shared Memory cards.
- Send a message by selecting the mechanism and typing in the text field.
- Mechanism details show real information:
  - Pipes/Sockets: last message, bytes, time(ms), parent→child PIDs.
  - Shared Memory: content, size, synchronization state and last modification.

## REST API Endpoints

- General status:
```
GET /ipc/status
```
- Mechanism details:
```
GET /ipc/detail/{pipes|sockets|shared_memory}
```
- Start/Stop mechanism:
```
POST /ipc/start/{pipes|sockets|shared_memory}
POST /ipc/stop/{pipes|sockets|shared_memory}
```
- Send message:
```
POST /ipc/send
Content-Type: application/json
{
  "mechanism": "pipes|sockets|shared_memory",
  "message": "Your message"
}
```

curl examples:
```bash
curl -X POST http://localhost:9000/ipc/start/shared_memory
curl -X POST http://localhost:9000/ipc/send \
  -H 'Content-Type: application/json' \
  -d '{"mechanism":"shared_memory","message":"hello"}'
curl http://localhost:9000/ipc/detail/shared_memory
```

## Tips

- If frontend doesn't open, confirm the binary is being executed from the generated `build/bin` and the `frontend/` folder exists in the repo. Server tries `../../frontend`, then `../frontend`, then `./frontend`.
- If using older GCC without complete `std::format` support, use GCC 13+.

## Autoria

Bruno Henrique Foggiatto — Logger, Socket Manager (C++), Pipe Manager (C++), Frontend

Luiz Felipe Greboge — Shared Memory (C++), IPC Coordinator, HTTP Server, Testes

---
Projeto acadêmico para demonstrar IPC e práticas modernas de C++.
