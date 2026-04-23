# tiny-hyper-router

Native C++ scaffolding for a transcript-first agent runtime inspired by `hyper-router`.

## Current scope

This repository currently provides a small but working C++ agent/runtime foundation with:

- core runtime abstractions
- provider abstraction
- storage abstraction
- shared runtime/message/tool types
- concrete `OpenRouterProvider` implementation
- concrete `StubProvider` for local testing
- concrete `InMemoryStorage` adapter
- concrete `JsonFileStorage` adapter
- optional `SqliteStorage` adapter
- reusable libcurl-based `CurlHttpClient`

## Planned direction

Initial MVP target:

- OpenRouter provider only
- direct HTTP calls
- non-streaming
- transcript-based continuation
- tool calling

## Layout

```txt
include/tiny_hyper_router/
  tiny_hyper_router.hpp
  core/
    agent.hpp
    providers.hpp
    runtime.hpp
    storage.hpp
    tool.hpp
    types.hpp
  storage/
    types.hpp
src/
  core/
    runtime.cpp
    types.cpp
```

## Build

Dependencies are managed with `vcpkg`.

Required now:

- `nlohmann-json`
- `curl`

Optional feature dependency:

- `sqlite3` for `SqliteStorage`

Example configure/build:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
```

If you get any issues run

```powershell
cmake --fresh --preset x64-debug
```

## Included development helpers

Current concrete helpers added for local development:

- `StubProvider`
- `InMemoryStorage`
- `JsonFileStorage`
- `SqliteStorage` when SQLite support is enabled
- `examples/basic.cpp`

## Tests

A small smoke test target is included:

- `tiny_hyper_router_basic_smoke`

After building, you can run normal local tests with:

```powershell
ctest --test-dir out/build/x64-debug --output-on-failure
```

This does not run the live OpenRouter manual test unless you configured with `-DTHR_ENABLE_LIVE_TESTS=ON`.

SQLite storage is also opt-in and requires configuring with `-DTHR_ENABLE_SQLITE_STORAGE=ON`.

## Storage adapters

Current storage adapters:

- `InMemoryStorage`
- `JsonFileStorage`
- `SqliteStorage` when SQLite support is enabled

`JsonFileStorage` stores each full session transcript as structured JSON, including:

- messages
- tool calls
- reasoning metadata and raw reasoning items
- session metadata
- last run status

This preserves OpenRouter reasoning replay data across process restarts.

`SqliteStorage` stores the same structured transcript data in a single SQLite database file, using JSON payload columns for messages, metadata, and last run state.

To enable it at configure time:

```powershell
cmake --preset x64-debug -DTHR_ENABLE_SQLITE_STORAGE=ON
```

## Convenience headers

Added convenience headers similar to the TypeScript package-level export style:

- `tiny_hyper_router/providers/index.hpp`
- `tiny_hyper_router/providers/stub/index.hpp`
- `tiny_hyper_router/providers/openrouter/index.hpp`
- `tiny_hyper_router/storage/index.hpp`

## OpenRouter provider

The project now includes a working non-streaming OpenRouter Responses API provider with:

- provider options/types
- transcript-to-input-item mapping
- tool definition mapping
- request body building
- response body parsing
- generated image extraction
- reusable libcurl HTTP transport

## Shared HTTP transport

The transport abstraction has been moved to a shared layer for future multi-provider support:

- `tiny_hyper_router/http/client.hpp`
- `src/http/client.cpp`

Providers are responsible for:

- building provider-specific request JSON
- constructing provider-specific headers
- parsing provider-specific response JSON

The shared HTTP layer is responsible only for generic request/response transport.

## Curl HTTP client

A reusable provider-agnostic libcurl transport is now included:

- `tiny_hyper_router/http/curl_http_client.hpp`
- `src/http/curl_http_client.cpp`

This implements the shared `HttpClient` abstraction and can be reused by multiple providers.

## OpenRouter reasoning

The OpenRouter provider now supports basic reasoning configuration and parsing:

- request `reasoning` object
- request `include: ["reasoning.encrypted_content"]`
- parsed `output` reasoning blocks
- parsed `usage.output_tokens_details.reasoning_tokens`
- replaying persisted reasoning items back into subsequent request history

## Manual live tests

A manual live test executable is available:

- `tiny_hyper_router_openrouter_live_manual`

It is intentionally not registered in default `ctest` runs.

To opt in during configure:

```powershell
cmake --preset x64-debug -DTHR_ENABLE_LIVE_TESTS=ON
```

It writes request/response logs as JSON under:

- `out/openrouter-live-manual/`

Reasoning metadata is now also attached to assistant transcript messages for persistence, including:

- reasoning item `id`
- `encrypted_content`
- `format`
- `status`
- `summary`

The live test also writes explicit reasoning replay proof files for the second request in tool flows:

- `single_tool_second_request_reasoning_replay_check.json`
- `tool_chain_second_request_reasoning_replay_check.json`

The tool-based live scenarios now return intentionally large deterministic tool payloads so you can inspect:

- transcript persistence under larger tool outputs
- replay/transcription integrity
- prompt-size/cache-threshold behavior

### Running from Windows Developer Command Prompt

Use `set`, not PowerShell `$env:` syntax:

```bat
set OPENROUTER_API_KEY=your_key_here
set OPENROUTER_MODEL=openai/o3
set OPENROUTER_LIVE_TOOL_TEXT_REPEAT=400
out\build\x64-debug\tiny_hyper_router_openrouter_live_manual.exe
```

### Running from PowerShell

```powershell
$env:OPENROUTER_API_KEY="your_key_here"
$env:OPENROUTER_MODEL="openai/o3"
$env:OPENROUTER_LIVE_TOOL_TEXT_REPEAT="400"
.\out\build\x64-debug\tiny_hyper_router_openrouter_live_manual.exe
```

Environment variables:

- `OPENROUTER_API_KEY`: required
- `OPENROUTER_MODEL`: optional, defaults to `openai/o4-mini`
- `OPENROUTER_LIVE_TOOL_TEXT_REPEAT`: optional, defaults to `160`

## Embedding API

A handle-based embedding layer is now included for host apps, including Android and iOS.

Headers:

- `tiny_hyper_router/embed/client.hpp`
- `tiny_hyper_router/embed/c_api.h`

High-level native wrapper:

- `tiny_hyper_router::embed::Client`

C ABI for JNI / Objective-C++ and other FFI bridging:

- `thr_create_client_from_json`
- `thr_destroy_client`
- `thr_send_message_json`
- `thr_get_session_json`
- `thr_free_string`

Design notes:

- the client handle owns agent config, provider, storage, and runtime
- persistent session continuity still comes from storage keyed by `session_id`
- tool execution remains internal C++ only
- JSON strings are used at the embedding boundary for simpler host interop

Example config JSON:

```json
{
  "agent_name": "embed-agent",
  "instructions": "You are helpful.",
  "model": "openai/o4-mini",
  "api_key": "your_key_here",
  "storage_directory": "/app/sandbox/thr"
}
```

Example request JSON:

```json
{
  "session_id": "chat-1",
  "input": "Hello from embed host",
  "max_steps": 5,
  "ephemeral": false
}
```

For local smoke testing, you can configure the embed client to use:

- `use_stub_provider: true`
- `use_in_memory_storage: true`
