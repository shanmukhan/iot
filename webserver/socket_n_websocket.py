#!/usr/bin/env python3

import base64
import socket
import asyncio
import datetime
import random
import websockets

from asyncio import Queue

q = Queue()

buffer = b''


async def handle_client(client):
    print('client connected')
    loop = asyncio.get_event_loop()
    request = None
    while True:

        request = (await loop.sock_recv(client, 2048))#.decode('utf8')

        global buffer

        if b'\n' in request:
            body, extra = request.split(b'\n')
            data = buffer + body + b'\n'
            buffer = extra
            await q.put(data)
            print('')
            #print('found new data', data)

        elif request:
            print('.', end='')
            #print('buffer data received')
            buffer += request

        await asyncio.sleep(0.1)

        #response = str(eval(request)) + '\n'
        #await loop.sock_sendall(client, response.encode('utf8'))

    client.close()

async def run_server():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    print('listening on 15555')
    server.bind(('192.168.1.5', 15555))
    server.listen(8)
    server.setblocking(False)

    loop = asyncio.get_event_loop()

    while True:
        client, _ = await loop.sock_accept(server)
        loop.create_task(handle_client(client))



async def echo(websocket):
    while True:

        print('Queue size is', q.qsize())
        message = await q.get()


        print(len(message))
        await asyncio.sleep(0.2)
        await websocket.send(message.decode('utf8'))

async def main():
    async with websockets.serve(echo, "localhost", 8000):
        await asyncio.Future()  # run forever

#print('Main starting')
#asyncio.run(main())
#print('Main started')

#start_server = websockets.serve(echo, 'localhost', 8000)

#asyncio.get_event_loop().run_until_complete(start_server)


print('server starting')
asyncio.ensure_future(run_server())
asyncio.ensure_future(main())
print('server started')

asyncio.get_event_loop().run_forever()
