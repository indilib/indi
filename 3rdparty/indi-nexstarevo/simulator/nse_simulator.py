#!/bin/env python3

import asyncio
import signal
import socket
import sys
from socket import SOL_SOCKET, SO_BROADCAST, SO_REUSEADDR
from nse_telescope import NexStarScope

telescope = NexStarScope()

async def broadcast(sport=2000, dport=55555, host='255.255.255.255', seconds_to_sleep=5):
    broadcast.keep_running=True
    sck = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sck.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    sck.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)
    # Fake msg. The app does not care for the payload
    msg = 110*b'X'
    sck.bind(('',sport))
    print('Broadcasting to port {0}, sleeping for: {1} seconds'.format(dport,seconds_to_sleep))
    while broadcast.keep_running :
        bn = sck.sendto(msg,(host,dport))
        await asyncio.sleep(seconds_to_sleep)
    print('App connected - stopping broadcast')

async def timer(seconds_to_sleep=1):
    timer.keep_running = True
    while timer.keep_running :
        await asyncio.sleep(seconds_to_sleep)
        telescope.tick(seconds_to_sleep)

async def handle_port2000(reader, writer):
    '''
    This function handles initial communication with the WiFly module and
    delegates the real job of simulating the scope to the NexStarScope class.
    It also handles all the dirty details of actual communication.
    '''
    
    # The WiFly module is initially in the transparent mode and just passes
    # the data to the serial connection. Unless the '$$$' sequence is detected.
    # Then it switches to the command mode until the exit command is issued.
    # The $$$ should be guarded by the 1s silence.
    transparent=True
    retry = 5
    # Endless comm loop.
    while True :
        data = await reader.read(1024)
        if not data :
            if retry:
                await asyncio.sleep(1)
                retry -= 1
                continue
            else :
                writer.close()
                timer.keep_running = False
                broadcast.keep_running = False
                return
        retry = 5
        addr = writer.get_extra_info('peername')
        #print("-> Scope received %r from %r." % (data, addr))
        resp = b''
        if transparent :
            if data[:3]==b'$$$' :
                # Enter command mode
                transparent = False
                broadcast.keep_running=False
                print('App from {0} connected. Terminating broadcast and starting main loop.'.format(addr))
                resp = b'CMD\r\n'
            else :
                # pass it on to the scope for handling
                resp = telescope.handle_msg(data)
        else :
            # We are in command mode detect exit and get out.
            # Otherwise just echo what we got and ack.
            try :
                message = data.decode('ascii').strip()
            except UnicodeError :
                # The data is invalid ascii - ignore it
                pass
            if message == 'exit' :
                # get out of the command mode
                transparent = True
                resp = data + b'\r\nEXIT\r\n'
            else :
                resp = data + b'\r\nAOK\r\n<2.40-CEL> '
        if resp :
            #print("<- Server sending: %r" % resp )
            writer.write(resp)
            await writer.drain()

def signal_handler(signal, frame):  
    loop.stop()
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)

loop = asyncio.get_event_loop()
scope = asyncio.start_server(handle_port2000, host='', port=2000)

loop.run_until_complete(asyncio.wait([broadcast(), timer(0.2), scope]))
loop.close()
