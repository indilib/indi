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

#ifndef PROCESS_CONTROLLER_H_
#define PROCESS_CONTROLLER_H_ 1

#include <string>
#include <vector>
#include <list>


/**
 * Interface to a mocked connection
 */
class ProcessController {
    pid_t pid;
    int status;
    std::string cmd;

    void finish();
public:
    ProcessController();
    ~ProcessController();

    void start(const std::string & path, const std::vector<std::string>  & args);

    void expectDone();
    void expectAlive();
    void expectExitCode(int e);
    void join();

    void waitProcessEnd(int expectedExitCode);

    // Returns 0 on some system. Use checkOpenFdCount for actual verification
    int getOpenFdCount();

    void checkOpenFdCount(int expected, const std::string & msg);
};


#endif // PROCESS_CONTROLLER_H_
