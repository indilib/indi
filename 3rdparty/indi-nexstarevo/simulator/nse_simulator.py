#!/bin/env python

import asyncio
import signal
import socket
from socket import SOL_SOCKET, SO_BROADCAST, SO_REUSEADDR
 
async def broadcast(sport=2000, dport=55555, host='255.255.255.255', seconds_to_sleep=5):
    broadcast.keep_running=True
    sck = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sck.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    sck.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)
    msg = 110*b'X'
    sck.bind(('',sport))
    while broadcast.keep_running :
        bn = sck.sendto(msg,(host,dport))
        print('Send {0} bytes, sleeping for: {1} seconds'.format(bn, seconds_to_sleep,))
        await asyncio.sleep(seconds_to_sleep)
    print('App connected - stopping broadcast')
 

def print_command(cmd):
    return 'Command: %02x->%02x [%02x] len:%d: data:%r' % (cmd[1], cmd[2], cmd[3], cmd[0], cmd[4:-1])

def decode_command(cmd):
    return (cmd[3], cmd[1], cmd[2], cmd[0], cmd[4:-1], cmd[-1])

def split_cmds(data):
    return [cmd for cmd in data.split(b';') if len(cmd)]

def make_checksum(data):
    return ((~sum([c for c in bytes(data)]) + 1) ) & 0xFF

ACK_CMDS=[0x02,0x04,0x06,0x24,]

def ack_cmd(cmd):
    c,f,t,l,d,cs=decode_command(cmd)
    rsp=b''
    if c in ACK_CMDS :
        rsp=b';\x03'+bytes((t,f,c))
        rsp+=bytes((make_checksum(rsp[1:]),))
    return rsp

async def handle_port2000(reader, writer):
    data = await reader.read(100)
    message = data.decode()
    addr = writer.get_extra_info('peername')
    print("-> Scope received %r from %r" % (message, addr))
    if message == '$$$' :
        broadcast.keep_running=False
        print('App from {0} connected. Terminating broadcast and starting main loop.'.format(addr))
        data = b'CMD\r\n'
        print("<- Scope sending: %r" % data )
        writer.write(data)
        await writer.drain()

    initial = True
    while initial :
        data = await reader.read(256)
        message = data.decode().strip()
        addr = writer.get_extra_info('peername')
        print("-> Scope received %r from %r" % (message, addr))
        print("<- Scope sending: %r" % message)
        writer.write(data)
        if message == 'exit' :
            initial = False
            data = b'\r\nEXIT\r\n'
            print("<- Server sending: %r" % data )
            writer.write(data)                    
        else :
            data = b'\r\nAOK\r\n<2.40-CEL> '
            print("<- Server sending: %r" % data )
            writer.write(data)        
        await writer.drain()

    print('Initial negotiations finished starting scope mode')
    while True :
        try :
            data = await reader.read(256)
            addr = writer.get_extra_info('peername')
            if len(data)>1 and data[0]==0x3b :
                for cmd in split_cmds(data):
                    print("-> Scope received %s from %r." % (print_command(cmd), addr))
                    c,f,t,l,d,s=decode_command(cmd)
                    #print(c,f,t,l,d,s)
                    if c == 0xfe :
                        rpl = bytes.fromhex('3b032010fecf3b071020fe070b13ecba')
                    elif c == 0x05 :
                        rpl = bytes.fromhex('3b03201005c83b0510200516852b')
                    else :
                        rpl = b';' + cmd + ack_cmd(cmd)
                    for c in split_cmds(rpl):
                        print("<- Scope sending:", print_command(c))
                    writer.write(rpl)
                await writer.drain()
            else :
                if len(data):
                    print("-> Scope received %r from %r. IGNORE" % (data, addr))
                else :
                    print("-- Terminating connection")
                    writer.close()
                    return
        except :
            print("EXception -- Terminating connection")
            writer.close()
            raise

def signal_handler(signal, frame):  
    loop.stop()
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)

loop = asyncio.get_event_loop()
scope = asyncio.start_server(handle_port2000, host='', port=2000)

loop.run_until_complete(asyncio.wait([broadcast(), scope]))
loop.close()
