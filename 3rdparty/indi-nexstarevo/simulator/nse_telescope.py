#!/bin/env python3

import struct
import sys
from math import pi

# ID tables
targets={'ANY':0x00,
         'MB' :0x01,
         'HC' :0x04,
         'UKN1':0x05,
         'HC+':0x0d,
         'AZM':0x10,
         'ALT':0x11,
         'APP':0x20,
         'GPS':0xb0,
         'TRG1': 0xb4,
         'WiFi':0xb5,
         'BAT':0xb6,
         'CHG':0xb7,
         'LIGHT':0xbf
        }
trg_names={value:key for key, value in targets.items()}
        
control={
         'HC' :0x04,
         'HC+':0x0d,
         'APP':0x20,
        }

commands={
          'MC_GET_POSITION':0x01,
          'MC_GOTO_FAST':0x02,
          'MC_SET_POSITION':0x04,
          'MC_GET_???':0x05,
          'MC_SET_POS_GUIDERATE':0x06,
          'MC_SET_NEG_GUIDERATE':0x07,
          'MC_LEVEL_START':0x0b,
          'MC_SET_POS_BACKLASH':0x10,
          'MC_SET_NEG_BACKLASH':0x11,
          'MC_GOTO_SLOW':0x17,
          'MC_AT_INDEX':0x18,
          'MC_SEEK_INDEX':0x19,
          'MC_CMD21':0x21,
          'MC_CMD23':0x23,
          'MC_MOVE_POS':0x24,
          'MC_MOVE_NEG':0x25,
          'MC_GET_POS_BACKLASH':0x40,
          'MC_GET_NEG_BACKLASH':0x41,
          'MC_GET_AUTOGUIDE_RATE':0x47,
          'MC_GET_APPROACH':0xfc,
          'MC_SET_APPROACH':0xfd,
          'GET_VER':0xfe,
         }
cmd_names={value:key for key, value in commands.items()}

ACK_CMDS=[0x02,0x04,0x06,0x24,]

def print_command(cmd):
    if cmd[2] in (0x10, 0x20):
        try :
            return 'Command: %s->%s [%s] len:%d: data:%r' % (
                        trg_names[cmd[1]], 
                        trg_names[cmd[2]], 
                        cmd_names[cmd[3]], cmd[0], cmd[4:-1])
        except KeyError :
            pass
    try :
        return 'Command: %s->%s [%02x] len:%d: data:%r' % (
                    trg_names[cmd[1]], 
                    trg_names[cmd[2]], 
                    cmd[3], cmd[0], cmd[4:-1])
    except KeyError :
        pass
        
    return 'Command: %02x->%02x [%02x] len:%d: data:%r' % (
                cmd[1], cmd[2], cmd[3], cmd[0], cmd[4:-1])
            

def decode_command(cmd):
    return (cmd[3], cmd[1], cmd[2], cmd[0], cmd[4:-1], cmd[-1])

def split_cmds(data):
    # split the data to commands
    # the initial byte b'\03b' is removed from commands
    return [cmd for cmd in data.split(b';') if len(cmd)]

def make_checksum(data):
    return ((~sum([c for c in bytes(data)]) + 1) ) & 0xFF


def ack_cmd(cmd):
    c,f,t,l,d,cs=decode_command(cmd)
    rsp=b''
    if c in ACK_CMDS :
        rsp=b';\x03'+bytes((t,f,c))
        rsp+=bytes((make_checksum(rsp[1:]),))
    return rsp

def f2dms(f):
    '''
    Convert fraction of the full rotation to DMS triple (degrees).
    '''
    d=360*f
    dd=int(d)
    mm=int((d-dd)*60)
    ss=(d-dd-mm/60)*3600
    return dd,mm,ss

def parse_pos(d):
    '''
    Parse first three bytes into the DMS string
    '''
    if len(d)>=3 :
        pos=struct.unpack('!i',b'\x00'+d[:3])[0]/2**24
        return u'%03d°%02d\'%04.1f"' % f2dms(pos)
    else :
        return u''

def repr_pos(alt,azm):
    return u'ALT:%03d°%02d\'%04.1f"  AZM:%03d°%02d\'%04.1f"' % (f2dms(alt) + f2dms(azm))

def unpack_int3(d):
    return struct.unpack('!i',b'\x00'+d[:3])[0]/2**24

def pack_int3(f):
    return struct.pack('!i',int(f*(2**24)))[1:]

class NexStarScope:
    
    __mcfw_ver=(7,11,5100//256,5100%256)
    __hcfw_ver=(5,28,5300//256,5300%256)
    
    trg=('MB', 'HC', 'UKN1', 'HC+', 'AZM', 'ALT', 'APP', 
            'GPS', 'WiFi', 'BAT', 'CHG', 'LIGHT')
    
    def __init__(self, ALT=0.0, AZM=0.0):
        self.__alt=ALT
        self.__azm=AZM
        self.trg_alt=self.__alt
        self.trg_azm=self.__azm
        self.alt_rate=0
        self.azm_rate=0
        self.alt_approach=0
        self.azm_approach=0
        self.last_cmd=''
        self.slewing=False
        self.guiding=False
        self.alt_guiderate=0.0
        self.azm_guiderate=0.0
        self._other_handlers = {
            0x10: NexStarScope.cmd_0x10,
            0x18: NexStarScope.cmd_0x18,
            0xfe: NexStarScope.send_ack,
        }
        self._mc_handlers = {
          0x01 : NexStarScope.get_position,
          0x02 : NexStarScope.goto_fast,
          0x04 : NexStarScope.set_position,
          0x05 : NexStarScope.cmd_0x05,
          0x06 : NexStarScope.set_pos_guiderate,
          0x07 : NexStarScope.set_neg_guiderate,
          0x0b : NexStarScope.level_start,
          0x10 : NexStarScope.set_pos_backlash,
          0x11 : NexStarScope.set_neg_backlash,
          0x13 : NexStarScope.slew_done,
          0x17 : NexStarScope.goto_slow,
          0x18 : NexStarScope.at_index,
          0x19 : NexStarScope.seek_index,
          0x21 : NexStarScope.cmd_0x21,
          0x23 : NexStarScope.cmd_0x23,
          0x24 : NexStarScope.move_pos,
          0x25 : NexStarScope.move_neg,
          0x39 : NexStarScope.send_ack,
          0x3a : NexStarScope.set_cordwrap_pos,
          0x40 : NexStarScope.get_pos_backlash,
          0x41 : NexStarScope.get_neg_backlash,
          0x47 : NexStarScope.get_autoguide_rate,
          0xfc : NexStarScope.get_approach,
          0xfd : NexStarScope.set_approach,
          0xfe : NexStarScope.fw_version,
        }

    def send_ack(self, data, snd, rcv):
        return b''

    def cmd_0x21(self, data, snd, rcv):
        return bytes.fromhex('0fa01194')

    def cmd_0x23(self, data, snd, rcv):
        return b'\x00'

    def cmd_0x10(self, data, snd, rcv):
        if rcv == 0xbf : # LIGHT
            return b'\xff'
        elif rcv == 0xb7 : # CHG
            return b'\x00'
        elif rcv == 0xb6 : # BAT
            return bytes.fromhex('0102009edfe5') # ????!!!!
        else :
            return b''

    def cmd_0x18(self, data, snd, rcv):
        if rcv == 0xb6 : # BAT
            return b'\x07\xd0' # 2000mA ?


    def get_position(self, data, snd, rcv):
        try :
            trg = trg_names[rcv]
        except KeyError :
            trg = '???'
        if trg == 'ALT':
            return pack_int3(self.alt)
        else :
            return pack_int3(self.azm)

    def goto_fast(self, data, snd, rcv):
        self.last_cmd='GOTO_FAST'
        self.slewing=True
        r=0.01
        a=unpack_int3(data)
        if trg_names[rcv] == 'ALT':
            self.trg_alt=a
            if self.alt < a :
                self.alt_rate = r
            else :
                self.alt_rate = -r
        else :
            self.trg_azm=a%1.0
            f = 1 if abs(self.azm - self.trg_azm)<0.5 else -1 
            if self.azm < self.trg_azm :
                self.azm_rate = f*r
            else :
                self.azm_rate = -f*r
        return b''

    def set_position(self,data, snd, rcv):
        return b''

    def cmd_0x05(self, data, snd, rcv):
        return bytes.fromhex('1685')

    def set_pos_guiderate(self, data, snd, rcv):
        a=unpack_int3(data)
        self.guiding = a>0
        if trg_names[rcv] == 'ALT':
            self.alt_guiderate=a
        else :
            self.azm_guiderate=a
        return b''

    def set_neg_guiderate(self, data, snd, rcv):
        a=unpack_int3(data)
        self.guiding = a>0
        if trg_names[rcv] == 'ALT':
            self.alt_guiderate=-a
        else :
            self.azm_guiderate=-a
        return b''

    def level_start(self, data, snd, rcv):
        return b''

    def set_pos_backlash(self, data, snd, rcv):
        return b''

    def set_neg_backlash(self, data, snd, rcv):
        return b''

    def goto_slow(self, data, snd, rcv):
        self.last_cmd='GOTO_SLOW'
        self.slewing=True
        r=0.0005
        a=unpack_int3(data)
        if trg_names[rcv] == 'ALT':
            self.trg_alt=a
            if self.alt < a :
                self.alt_rate = r
            else :
                self.alt_rate = -r
        else :
            self.trg_azm=a%1.0
            f = 1 if abs(self.azm - self.trg_azm)<0.5 else -1 
            if self.azm < self.trg_azm :
                self.azm_rate = f*r
            else :
                self.azm_rate = -f*r
        return b''

    def slew_done(self, data, snd, rcv):
        if self.alt_rate or self.azm_rate :
            return b'\x00'
            self.slewing=True
        else :
            self.slewing=False
            return b'\xff'

    def at_index(self, data, snd, rcv):
        return b'\x00'

    def seek_index(self, data, snd, rcv):
        return b''

    def move_pos(self, data, snd, rcv):
        self.last_cmd='MOVE_POS'
        self.slewing=True
        r=0.0001*int(data[0])
        if trg_names[rcv] == 'ALT':
            self.alt_rate = r
        else :
            self.azm_rate = r
        return b''

    def move_neg(self, data, snd, rcv):
        self.last_cmd='MOVE_NEG'
        self.slewing=True
        r=0.0001*int(data[0])
        if trg_names[rcv] == 'ALT':
            self.alt_rate = -r
        else :
            self.azm_rate = -r
        return b''

    def set_cordwrap_pos(self, data, snd, rcv):
        self.cordwrap_pos=struct.unpack('!i',b'\x00'+data[:3])[0]
        return b''

    def get_pos_backlash(self, data, snd, rcv):
        return b'\x00'

    def get_neg_backlash(self, data, snd, rcv):
        return b'\x00'

    def get_autoguide_rate(self, data, snd, rcv):
        return b'\xf0'

    def get_approach(self, data, snd, rcv):
        try :
            trg = trg_names[rcv]
        except KeyError :
            trg = '???'
        if trg == 'ALT':
            return bytes((self.alt_approach,))
        else :
            return bytes((self.azm_approach,))
            
    def set_approach(self, data, snd, rcv):
        try :
            trg = trg_names[rcv]
        except KeyError :
            trg = '???'
        if trg == 'ALT':
            self.alt_approach=data[0]
        else :
            self.azm_approach=data[0]
        return b''

    def fw_version(self, data, snd, rcv):
        try :
            trg = trg_names[rcv]
        except KeyError :
            trg = '???'
        if trg in ('ALT','AZM'):
            return bytes(NexStarScope.__mcfw_ver)
        elif trg in ('HC', 'HC+'):
            return bytes(NexStarScope.__hcfw_ver)
        else :
            return b''

    def show_status(self):
        mode = 'Idle'
        if self.guiding : mode = 'Guiding'
        if self.slewing : mode = 'Slewing'
        print('\r%8s: %s -> %s (%+5.2f %+5.2f) CMD: %10s' % (
                mode, repr_pos(self.alt, self.azm), 
                repr_pos(self.trg_alt, self.trg_azm),
                self.alt_rate*360, self.azm_rate*360,
                self.last_cmd), end='')
        sys.stdout.flush()

    def tick(self, interval):
        eps=1e-8
        self.alt += (self.alt_rate + self.alt_guiderate)*interval
        self.azm += (self.azm_rate + self.azm_guiderate)*interval
        if self.slewing :
            if abs(self.alt - self.trg_alt) < 2*abs(self.alt_rate*interval) :
                self.alt_rate*=0.5
            if abs(self.azm - self.trg_azm) < 2*abs(self.azm_rate*interval) :
                self.azm_rate*=0.5
        if self.azm_rate < eps and self.alt_rate < eps :
            self.alt_rate=0
            self.azm_rate=0
            self.slewing=False
        self.show_status()

    @property
    def alt(self):
        return self.__alt
        
    @alt.setter
    def alt(self,ALT):
        self.__alt=ALT
        # Altitude movement limits
        self.__alt = min(self.__alt,0.23)
        self.__alt = max(self.__alt,-0.03)
        
    @property
    def azm(self):
        return self.__azm
    
    @azm.setter
    def azm(self,AZM):
        self.__azm=AZM % 1.0
            

    def handle_cmd(self, cmd):
        #print("-> Scope received %s." % (print_command(cmd)))
        try :
            c,f,t,l,d,s=decode_command(cmd)
        except IndexError :
            print("Malformed command: %r" % (cmd,))
            return b''
        resp=b''
        if make_checksum(cmd[:-1]) != s :
            print("Wrong checksum. Ignoring.")
        else :
            resp = b';' + cmd
            if t in (0x10, 0x11):
                handlers=self._mc_handlers
            else :
                handlers=self._other_handlers
                
            if c in handlers :
                r = handlers[c](self,d,f,t)
                r = bytes((len(r)+3,t,f,c)) + r
                resp += b';' + r + bytes((make_checksum(r),))
                #print('Response: %r' % resp)
            else :
                print('Scope got unknown command %02x' % c)
                print("-> Scope received %s." % (print_command(cmd)))
                
        return resp

    def handle_msg(self, msg):
        '''
        React to message. Get AUX command(s) in the message and react to it. 
        Return a message simulating real AUX scope response.
        '''
        return b''.join([self.handle_cmd(cmd) for cmd in split_cmds(msg)])
            



