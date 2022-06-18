/*******************************************************************************
  Copyright(c) 2022 Ludovic Pollet. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#ifndef CONNECTION_MOCK_H_
#define CONNECTION_MOCK_H_ 1

#include <string>
#include <list>

class SharedBuffer;

/**
 * Interface to a mocked connection
 */
class ConnectionMock {
    int fds[2];
    std::list<int> receivedFds;
    bool bufferReceiveAllowed;
    ssize_t read(void * buff, size_t count);
    char readChar(const std::string & expected);

    void release();
    std::string receiveMore();

    // On error, contains data that were not returned
    std::string pendingData;
public:
    ConnectionMock();
    ~ConnectionMock();
    void setFds(int rd, int wr);

    void expect(const std::string & content);
    void expectXml(const std::string & xml);
    void send(const std::string & content);
    void send(const std::string & content, const SharedBuffer & buff);
    void send(const std::string & content, const SharedBuffer ** buffers);

    void allowBufferReceive(bool state);
    void expectBuffer(SharedBuffer & fd);
};


#endif // CONNECTION_MOCK_H_