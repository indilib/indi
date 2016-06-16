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
    while broadcast.keep_running :
        bn = sck.sendto(msg,(host,dport))
        print('Send {0} bytes, sleeping for: {1} seconds'.format(bn, seconds_to_sleep,))
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
        
        
        
        
        
        
#    # First part - the WiFly modules waits for 
#        message = data.decode()
#        print("-> Scope received %r from %r" % (message, addr))
#        if message == '$$$' :
#            broadcast.keep_running=False
#            print('App from {0} connected. Terminating broadcast and starting main loop.'.format(addr))
#            data = b'CMD\r\n'
#            print("<- Scope sending: %r" % data )
#            writer.write(data)
#            await writer.drain()

#    initial = True
#    while initial :
#        data = await reader.read(256)
#        message = data.decode().strip()
#        addr = writer.get_extra_info('peername')
#        print("-> Scope received %r from %r" % (message, addr))
#        print("<- Scope sending: %r" % message)
#        writer.write(data)
#        if message == 'exit' :
#            initial = False
#            data = b'\r\nEXIT\r\n'
#            print("<- Server sending: %r" % data )
#            writer.write(data)                    
#        else :
#            data = b'\r\nAOK\r\n<2.40-CEL> '
#            print("<- Server sending: %r" % data )
#            writer.write(data)        
#        await writer.drain()

#    print('Initial negotiations finished starting scope mode')
#    while True :
#        try :
#            data = await reader.read(256)
#            addr = writer.get_extra_info('peername')
#            if len(data)>1 and data[0]==0x3b :
#                for cmd in split_cmds(data):
#                    print("-> Scope received %s from %r." % (print_command(cmd), addr))
#                    c,f,t,l,d,s=decode_command(cmd)
#                    #print(c,f,t,l,d,s)
#                    if c == 0xfe :
#                        rpl = bytes.fromhex('3b032010fecf3b071020fe070b13ecba')
#                    elif c == 0x05 :
#                        rpl = bytes.fromhex('3b03201005c83b0510200516852b')
#                    else :
#                        rpl = b';' + cmd + ack_cmd(cmd)
#                    for c in split_cmds(rpl):
#                        print("<- Scope sending:", print_command(c))
#                    writer.write(rpl)
#                await writer.drain()
#            else :
#                if len(data):
#                    print("-> Scope received %r from %r. IGNORE" % (data, addr))
#                else :
#                    print("-- Terminating connection")
#                    writer.close()
#                    return
#        except :
#            print("EXception -- Terminating connection")
#            writer.close()
#            raise

def signal_handler(signal, frame):  
    loop.stop()
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)

loop = asyncio.get_event_loop()
scope = asyncio.start_server(handle_port2000, host='', port=2000)

loop.run_until_complete(asyncio.wait([broadcast(), timer(), scope]))
loop.close()
