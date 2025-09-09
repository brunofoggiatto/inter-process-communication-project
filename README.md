# Inter-Process Communication Project

Aplicação em C++23 demonstrando três mecanismos de IPC com uma interface web para visualização e controle em tempo real.

## Visão Geral

Mecanismos implementados:
- Pipes Anônimos (unidirecional)
- Sockets Locais (AF_UNIX, bidirecional)
- Memória Compartilhada (System V IPC + semáforos)

O frontend (HTML/JS) consome uma API REST exposta pelo backend para:
- Iniciar/Parar cada mecanismo (Start/Stop)
- Enviar mensagens (`/ipc/send`)
- Ler status geral (`/ipc/status`) e detalhes do mecanismo (`/ipc/detail/{mechanism}`)

## Requisitos

- Linux (System V IPC + AF_UNIX)
- GCC/G++ com suporte a C++23 (recomendado GCC 13+)
- CMake 3.14+
- GoogleTest instalado no sistema (opcional, para testes)

## Estrutura do Projeto
```
backend/
  src/common/ (logger)
  src/ipc/    (pipe, socket, shmem, coordinator)
  src/server/ (http server)
  src/main.cpp
frontend/ (index.html, styles.css, script.js)
tests/    (unitários e integração)
```

## Build

Quickstart (com testes):
```
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
ctest --output-on-failure   # opcional, roda os testes
```

Binário gerado: `build/bin/ipc_system`

## Execução

1) Inicie o backend em modo servidor (expõe a API e serve o frontend):
```
./build/bin/ipc_system --server [--port 9000]
```
- Porta padrão: 9000 (use `--port <n>` para escolher outra)
- O servidor procura o frontend automaticamente em caminhos relativos ao binário (ex.: `../../frontend`, `../frontend`, `./frontend`).

2) Abra a interface web:
```
http://localhost:9000/
```

3) Use o dashboard:
- Clique em Start/Stop nos cartões de Pipes, Sockets ou Shared Memory.
- Envie uma mensagem selecionando o mecanismo e digitando no campo de texto.
- Os detalhes por mecanismo mostram informações reais:
  - Pipes/Sockets: última mensagem, bytes, tempo(ms), PIDs pai→filho.
  - Shared Memory: conteúdo, tamanho, estado de sincronização e última modificação.

## Endpoints REST (rápido)

- Status geral:
```
GET /ipc/status
```
- Detalhes do mecanismo:
```
GET /ipc/detail/{pipes|sockets|shared_memory}
```
- Iniciar/Parar mecanismo:
```
POST /ipc/start/{pipes|sockets|shared_memory}
POST /ipc/stop/{pipes|sockets|shared_memory}
```
- Enviar mensagem:
```
POST /ipc/send
Content-Type: application/json
{
  "mechanism": "pipes|sockets|shared_memory",
  "message": "Sua mensagem"
}
```

Exemplos curl:
```
curl -X POST http://localhost:9000/ipc/start/shared_memory
curl -X POST http://localhost:9000/ipc/send \
  -H 'Content-Type: application/json' \
  -d '{"mechanism":"shared_memory","message":"hello"}'
curl http://localhost:9000/ipc/detail/shared_memory
```

## Dicas

- Se o frontend não abrir, confirme que o binário está sendo executado a partir do `build/bin` gerado e que a pasta `frontend/` existe no repo. O servidor tenta `../../frontend`, depois `../frontend`, depois `./frontend`.
- Em caso de GCC antigo sem suporte completo a `std::format`, use GCC 13+.

## Autoria

Bruno Henrique Foggiatto — Logger, Socket Manager (C++), Pipe Manager (C++), Frontend

Luiz Felipe Greboge — Shared Memory (C++), IPC Coordinator, HTTP Server, Testes

---
Projeto acadêmico para demonstrar IPC e práticas modernas de C++.
