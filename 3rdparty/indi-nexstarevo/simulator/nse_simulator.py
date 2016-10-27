#!/bin/env python3

import asyncio
import signal
import socket
import sys
from socket import SOL_SOCKET, SO_BROADCAST, SO_REUSEADDR
from nse_telescope import NexStarScope

import curses

telescope=None

async def broadcast(sport=2000, dport=55555, host='255.255.255.255', seconds_to_sleep=5):
    sck = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sck.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    sck.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)
    # Fake msg. The app does not care for the payload
    msg = 110*b'X'
    sck.bind(('',sport))
    telescope.print_msg('Broadcasting to port {0}'.format(dport))
    telescope.print_msg('sleeping for: {0} seconds'.format(seconds_to_sleep))
    while True :
        bn = sck.sendto(msg,(host,dport))
        await asyncio.sleep(seconds_to_sleep)
    telescope.print_msg('Stopping broadcast')

async def timer(seconds_to_sleep=1,telescope=None):
    while True :
        await asyncio.sleep(seconds_to_sleep)
        if telescope : 
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
    global telescope
    # Endless comm loop.
    connected=False
    while True :
        data = await reader.read(1024)
        if not data :
            writer.close()
            telescope.print_msg('Connection closed. Closing server.')
            return
        elif not connected :
            telescope.print_msg('App from {0} connected.'.format(writer.get_extra_info('peername')))
            connected=True
        retry = 5
        addr = writer.get_extra_info('peername')
        #print("-> Scope received %r from %r." % (data, addr))
        resp = b''
        if transparent :
            if data[:3]==b'$$$' :
                # Enter command mode
                transparent = False
                telescope.print_msg('App from {0} connected.'.format(addr))
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

#def signal_handler(signal, frame):  
#    loop.stop()
#    sys.exit(0)

#signal.signal(signal.SIGINT, signal_handler)

def main(stdscr):

    global telescope 
    if len(sys.argv) >1 and sys.argv[1]=='t':
        telescope = NexStarScope(stdscr=None)
    else :
        telescope = NexStarScope(stdscr=stdscr)

    loop = asyncio.get_event_loop()
    scope = loop.run_until_complete(asyncio.start_server(handle_port2000, host='', port=2000))
    telescope.print_msg('NSE simulator strted on {}.'.format(scope.sockets[0].getsockname()))
    telescope.print_msg('Hit CTRL-C to stop.')
    asyncio.ensure_future(broadcast())
    asyncio.ensure_future(timer(0.1,telescope))
    #asyncio.ensure_future(scope)

    try :
        loop.run_forever()
    except KeyboardInterrupt :
        pass
    telescope.print_msg('Simulator shutting down')
    scope.close()
    loop.run_until_complete(scope.wait_closed())

    #loop.run_until_complete(asyncio.wait([broadcast(), timer(0.2), scope]))
    loop.close()

curses.wrapper(main)
