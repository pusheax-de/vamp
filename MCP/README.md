# Vampire MCP Server

This directory contains a small C++14 MCP server that exposes files from a single root folder over the standard MCP stdio transport.

Implemented protocol surface:

- `initialize`
- `ping`
- `resources/list`
- `resources/read`
- `resources/templates/list`
- `notifications/initialized`

The server is intentionally narrow:

- transport: `stdio`
- capability: `resources`
- root access is restricted to one configured folder
- files can be returned as text or base64 blobs
- directories are exposed as resources and can be read as generated listings

## Build

```powershell
cmake -S MCP -B MCP/build
cmake --build MCP/build --config Release
```

Executable:

```text
MCP/build/Release/vampire_mcp_server.exe
```

## Run

Default root is the current working directory:

```powershell
MCP/build/Release/vampire_mcp_server.exe
```

Explicit root:

```powershell
MCP/build/Release/vampire_mcp_server.exe --root D:\src\vampire
```

Optional flags:

- `--root <path>`: folder to expose
- `--page-size <n>`: max resources per `resources/list` page
- `--name <value>`: server name reported in `serverInfo`

## Resource Model

The server exposes both directories and files.

- resource URIs use `file:///...`
- directory resources use MIME type `inode/directory`
- reading a directory returns a generated text listing
- text-like files return `text`
- non-text files return `blob` as base64

## Example Codex/Claude MCP Setup

The exact config format depends on the host app, but the command shape is:

```json
{
  "command": "D:/src/vampire/MCP/build/Release/vampire_mcp_server.exe",
  "args": ["--root", "D:/src/vampire"]
}
```

## Notes

- The stdio transport follows the MCP newline-delimited JSON-RPC model from the official MCP transport docs.
- The resource APIs follow the MCP lifecycle/resources docs.
- Batch JSON-RPC requests are not implemented in this first version.
