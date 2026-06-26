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
import ssl
import os
import argparse
import websockets

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

def make_ssl_context():
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(
        certfile=os.path.join(SCRIPT_DIR, "server.crt"),
        keyfile=os.path.join(SCRIPT_DIR, "server.key"),
    )
    return ctx

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
    parser = argparse.ArgumentParser()
    parser.add_argument("--tls", action="store_true", help="enable TLS (wss://)")
    parser.add_argument("--port", type=int, default=8081)
    args = parser.parse_args()

    host = "localhost"
    ssl_ctx = make_ssl_context() if args.tls else None
    scheme  = "wss" if args.tls else "ws"

    async with websockets.serve(handler, host, args.port, ssl=ssl_ctx):
        print(f"test server listening on {scheme}://{host}:{args.port}")
        print(f"run: ./build/sandbox/tconnect_sandbox")
        print(f"ctrl+c to stop\n")
        await asyncio.Future()

asyncio.run(main())
