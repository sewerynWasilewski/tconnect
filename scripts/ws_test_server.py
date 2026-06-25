#!/usr/bin/env python3
"""
WebSocket test server for tconnect.

Tests:
  - handshake
  - ping/pong (sends a ping on connect, verifies pong arrives)
  - message echo (echoes data frames back to client)
  - graceful close (logs when client sends close frame)

Usage:
  python3 scripts/ws_test_server.py
  # then run: ./build/sandbox/tconnect_sandbox
"""

import asyncio
import websockets

async def handler(ws):
    addr = ws.remote_address
    print(f"[+] client connected from {addr}")

    # ping/pong test
    try:
        pong_waiter = await ws.ping(b"tconnect-ping")
        await asyncio.wait_for(pong_waiter, timeout=5.0)
        print(f"[+] ping/pong OK")
    except asyncio.TimeoutError:
        print(f"[-] ping timed out - client did not send pong")
        return

    # echo loop
    try:
        async for message in ws:
            if isinstance(message, bytes):
                print(f"[<] binary ({len(message)} bytes): {message!r}")
            else:
                print(f"[<] text: {message}")
            await ws.send(message)
            print(f"[>] echoed back")
    except websockets.exceptions.ConnectionClosedOK:
        print(f"[+] client closed gracefully")
    except websockets.exceptions.ConnectionClosedError as e:
        print(f"[-] client closed with error: {e}")

    print(f"[-] client disconnected")

async def main():
    host, port = "localhost", 8081
    async with websockets.serve(handler, host, port):
        print(f"ws test server listening on ws://{host}:{port}")
        print(f"run: ./build/sandbox/tconnect_sandbox")
        print(f"ctrl+c to stop\n")
        await asyncio.Future()

asyncio.run(main())
