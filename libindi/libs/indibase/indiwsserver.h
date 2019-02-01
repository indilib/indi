/*******************************************************************************
 Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

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

#pragma once

#include <set>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/extensions/permessage_deflate/enabled.hpp>

//typedef websocketpp::server<websocketpp::config::asio> server;

struct deflate_server_config : public websocketpp::config::asio
{
    // ... additional custom config if you need it for other things

    /// permessage_compress extension
    struct permessage_deflate_config {};

    typedef websocketpp::extensions::permessage_deflate::enabled
    <permessage_deflate_config> permessage_deflate_type;
};

typedef websocketpp::server<deflate_server_config> server;

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

class INDIWSServer
{
    public:
        INDIWSServer()  {}

        uint16_t generatePort()
        {
            m_global_port++;
            m_port = m_global_port;
            return m_port ;
        }

        void on_open(connection_hdl hdl)
        {
            m_connections.insert(hdl);
        }

        void on_close(connection_hdl hdl)
        {
            m_connections.erase(hdl);
        }

        //    void on_message(connection_hdl hdl, server::message_ptr msg)
        //    {
        //        for (auto it : m_connections)
        //        {
        //            m_server->send(it,msg);
        //        }
        //    }

        void send_binary(void const * payload, size_t len)
        {
            for (auto it : m_connections)
            {
                try
                {
                    m_server->send(it, payload, len, websocketpp::frame::opcode::binary);
                }
                catch (websocketpp::exception const &e)
                {
                    std::cerr << e.what() << std::endl;
                }
                catch (...)
                {
                    std::cerr << "other exception" << std::endl;
                }

            }
        }

        void send_text(const std::string &payload)
        {
            for (auto it : m_connections)
            {
                try
                {
                    m_server->send(it, payload, websocketpp::frame::opcode::text);
                }
                catch (websocketpp::exception const &e)
                {
                    std::cerr << e.what() << std::endl;
                }
                catch (...)
                {
                    std::cerr << "other exception" << std::endl;
                }

            }
        }

        void stop()
        {
            for (auto it : m_connections)
                m_server->close(it, websocketpp::close::status::normal, "Switched off by user.");

            m_connections.clear();
            m_server->stop();
        }

        bool is_running()
        {
            return m_server->is_listening();
        }

        void run()
        {
            try
            {
                m_server.reset(new server());

                m_server->init_asio();
                m_server->set_reuse_addr(true);


                m_server->set_open_handler(bind(&INDIWSServer::on_open, this, ::_1));
                m_server->set_close_handler(bind(&INDIWSServer::on_close, this, ::_1));
                //m_server->set_message_handler(bind(&INDIWSServer::on_message,this,::_1,::_2));

                m_server->listen(m_port);
                m_server->start_accept();
                m_server->run();

            }
            catch (websocketpp::exception const &e)
            {
                std::cerr << e.what() << std::endl;
            }
            catch (...)
            {
                std::cerr << "other exception" << std::endl;
            }

        }

    private:
        typedef std::set<connection_hdl, std::owner_less<connection_hdl>> con_list;

        std::unique_ptr<server> m_server;
        con_list m_connections;
        uint16_t m_port;
        static uint16_t m_global_port;
};

