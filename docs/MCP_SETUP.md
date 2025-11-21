# MCP (Model Context Protocol) Setup Guide

## Overview

This project includes MCP configuration to share documentation with Codex and other AI assistants. The documentation covers:

1. **GVRET Protocol** - Binary serial protocol for CAN bus communication
2. **Arduino Uno R3** - Microcontroller specifications and hardware layout
3. **MCP2515 Shield** - CAN Bus shield specifications and pin connections

## Configuration File Location

The MCP configuration file is located at:
- **Windows**: `C:\Users\erikm\.cursor\mcp.json`
- **Linux/Mac**: `~/.cursor/mcp.json`

## Configuration Contents

```json
{
  "mcpServers": {
    "ArduinoCANBusDocs": {
      "url": "file:///home/erikm/arduino-canbus-monitor/docs",
      "description": "Documentation for GVRET protocol, Arduino Uno R3, and MCP2515 CAN Bus Shield"
    }
  }
}
```

## Documentation Files

The following documentation files are available in the `docs/` directory:

1. **GVRET_PROTOCOL.md** - Complete GVRET protocol specification including:
   - Binary packet structure
   - Command set (CAN transmission, reception, bus control)
   - Timestamp format
   - Error handling
   - Advantages over ASCII protocols

2. **ARDUINO_UNO_R3.md** - Arduino Uno R3 specifications including:
   - Technical specifications (microcontroller, memory, power)
   - Pin layout and functions
   - Communication interfaces (Serial, SPI, I2C)
   - Hardware layout diagram
   - Programming information

3. **MCP2515_SHIELD.md** - MCP2515 CAN Bus Shield documentation including:
   - Technical specifications
   - Pin connections to Arduino
   - Hardware layout
   - CAN bus wiring guidelines
   - Library usage examples
   - Common issues and solutions

## Using Documentation in Codex

Once the MCP server is configured, you can reference the documentation in Codex using:

- `@Docs GVRET` - Access GVRET protocol documentation
- `@Docs Arduino` - Access Arduino Uno R3 specifications
- `@Docs MCP2515` - Access MCP2515 shield documentation

## Alternative Setup Methods

### Method 1: GitMCP Server (Recommended for Sharing)

If you want to share documentation via a Git repository:

1. Push documentation to a GitHub repository
2. Use GitMCP service: `https://gitmcp.io/your-username/your-repo`
3. Update `mcp.json` with the GitMCP URL

### Method 2: Local File System

For local development, the current configuration uses a file:// URL pointing to the local docs directory.

### Method 3: HTTP Server

You can also serve documentation via a local HTTP server:

1. Start a simple HTTP server in the docs directory
2. Update the URL in `mcp.json` to point to `http://localhost:port`

## Verification

To verify the MCP setup is working:

1. Open Cursor/Codex
2. Try referencing documentation: `@Docs GVRET`
3. The AI assistant should be able to access and reference the documentation

## Troubleshooting

### MCP Server Not Found
- Verify the path in `mcp.json` is correct
- Check that documentation files exist in the specified directory
- Ensure Cursor has read permissions to the directory

### Documentation Not Accessible
- Verify file paths are correct
- Check file permissions
- Restart Cursor after configuration changes

### Path Issues (WSL/Windows)
- Use absolute paths in `mcp.json`
- For WSL, use `/mnt/c/...` format for Windows paths
- For Windows, use `C:\Users\...` format

## Additional Resources

- [MCP Protocol Specification](https://modelcontextprotocol.io/)
- [Cursor Documentation](https://cursor.sh/docs)
- Project README: See `README.md` for project-specific information

