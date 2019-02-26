// celestronauxpacket.cpp

#include "celestronauxpacket.h"
#include "indicom.h"
#include <termios.h>

#include <stdio.h>


#define SHORT_TIMEOUT 1

void dumpMsg(const char * msg, buffer buf)
{
    fprintf(stderr, "%s: ", msg);
    for (unsigned int i = 0; i < buf.size(); i++)
    {
        fprintf(stderr, "%02x ", buf[i]);
    }
    fprintf(stderr, "\n");
}

////////////////////////////////////////////////
////// AuxPacket
////////////////////////////////////////////////

AuxPacket::AuxPacket()
{
}

AuxPacket::AuxPacket (AuxTarget source, AuxTarget destination, AuxCommand command, buffer data)
{
    this->command = command;
    this->source = source;
    this->destination = destination;
    this->data = data;
    this->length = data.size() + 3;
}

void AuxPacket::FillBuffer(buffer& buff)
{
    buff.resize(this->length + 3);
	buff[0] = AUX_HDR;
	buff[1] = (unsigned char)length;
	buff[2] = (unsigned char)source;
	buff[3] = (unsigned char)destination;
	buff[4] = (unsigned char)command;
    for (int i = 0; i < (int)data.size(); i++)
	{
		buff[5 + i] = data[i];
	}

	buff.back() = checksum(buff);
	
	dumpMsg("fillBuffer", buff);
}

bool AuxPacket::Parse(buffer packet)
{
    if (packet.size() < 6)  // must contain header, len, src, dest, cmd and checksum at least
    {
        fprintf(stderr, "Parse size < 6");
		return false;
    }
	if (packet[0] != AUX_HDR)
    {
        fprintf(stderr, "Parse [0] |= 0x3b");
        return false;
    }
	length = packet[1];
    if ((int)packet.size() != length + 3)    // length must be correct
    {
        fprintf(stderr, "Parse size %i |= length+3 %i", (int)packet.size(), length+3);
        return false;
    }
	source = (AuxTarget)packet[2];
	destination = (AuxTarget)packet[3];
	command = (AuxCommand)packet[4];
    data  = buffer(packet.begin() + 5, packet.end() - 1);
	unsigned char cs = checksum(packet);
	unsigned char cb = packet[length + 2];
    if (cs != cb)
    {
        fprintf(stderr, "Parse checksum error cs %i |= cb %i", cs, cb);
    }
	return  cb == cs;
}

unsigned char AuxPacket::checksum(buffer packet)
{
	int cs = 0;
	for (int i = 1; i < packet[1] + 2; i++)
	{
		cs += packet[i];
	}
	return (unsigned char)(-cs & 0xff);
}

/////////////////////////////////////////////
/////////// AuxCommunicator
/////////////////////////////////////////////

AuxCommunicator::AuxCommunicator()
{
    this->source = AuxTarget::NEX_REMOTE;
}

AuxCommunicator::AuxCommunicator(AuxTarget source)
{
    this->source = source;
}

bool AuxCommunicator::sendPacket(int portFD, AuxTarget dest, AuxCommand cmd, buffer data)
{
    AuxPacket pkt(source, dest, cmd, data);

	buffer txbuff;
	pkt.FillBuffer(txbuff);
	//Focuser.LogMessage("SendPacket", "txbuff: {0}", BufStr(txbuff));
	int ns;

    int tty;
    tcflush(portFD, TCIOFLUSH);
    if (tty = tty_write(portFD, (const char *)txbuff.data(), (int)txbuff.size(), &ns) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(tty, errmsg, 256);
        fprintf(stderr, "sendPacket fail tty %i %s, ns %i\n", tty, errmsg, ns);
        return false;
    }
	return true;
}

bool AuxCommunicator::readPacket(int portFD, AuxPacket& reply)
{
	char rxbuf[] = {0};
    int nr;
    int tty;
	// look for header
    while(rxbuf[0] != AuxPacket::AUX_HDR)
	{
        if (tty = tty_read(portFD, rxbuf, 1, SHORT_TIMEOUT, &nr)!= TTY_OK)
        {
            char errmsg[256];
            tty_error_msg(tty, errmsg, 256);
            fprintf(stderr, "readPacket fail read hdr tty %i %s, nr %i\n", tty, errmsg, nr);
            return false;		// read failure is instantly fatal
        }
	}
	// get length
	if (tty_read(portFD, rxbuf, 1, SHORT_TIMEOUT, &nr)!= TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(tty, errmsg, 256);
        fprintf(stderr, "readPacket fail read len tty %i %s, ns %i\n", tty, errmsg, nr);
        return false;
    }
	int len = rxbuf[0];
	buffer packet(2);
    packet[0] = AuxPacket::AUX_HDR;
	packet[1] = (unsigned char)len;

	// get source, destination, command, data and checksum
    unsigned char rxdata[len + 1];
    if (tty_read(portFD, (char *)rxdata, len + 1, SHORT_TIMEOUT, &nr)!= TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(tty, errmsg, 256);
        fprintf(stderr, "readPacket fail data tty %i %s, nr %i\n", tty, errmsg, nr);
        return false;
    }
		
    packet.insert(packet.end(), rxdata, rxdata + len + 1);

    dumpMsg("readPacket", packet);
    //reply = AuxPacket();
	return reply.Parse(packet);
}

// send command with data and reply
bool AuxCommunicator::sendCommand(int portFD, AuxTarget dest, AuxCommand cmd, buffer data, buffer & reply)
{
	int num_tries = 0;

	while (num_tries++ < 3)
	{
		if (!sendPacket(portFD, dest, cmd, data))
			return false;       // failure to send is fatal

        AuxPacket pkt;
		if (!readPacket(portFD, pkt))
			continue;           // try again

		// check the packet is the one we want
        if (pkt.command != cmd || pkt.destination != AuxTarget::APP || pkt.source != dest)
        {
            fprintf(stderr, "sendCommand pkt.command %i cmd %i, pkt.destination %i pkt.source %i dest %i\n",
                    pkt.command, cmd, pkt.destination, pkt.source, dest);
			continue;           // wrong packet, try again
        }

        reply = pkt.data;
		return true;
	}
	return false;
}

// send command with reply but no data
bool AuxCommunicator::sendCommand(int portFD, AuxTarget dest, AuxCommand cmd, buffer& reply)
{
	buffer data(0);
	return sendCommand(portFD, dest, cmd, data, reply);
}

// send command with data but no reply
bool AuxCommunicator::commandBlind(int portFD, AuxTarget dest, AuxCommand cmd, buffer data)
{
	buffer reply;
	return sendCommand(portFD, dest, cmd, data, reply);
}

