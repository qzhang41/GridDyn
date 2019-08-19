/*
 * LLNS Copyright Start
 * Copyright (c) 2017, Lawrence Livermore National Security
 * This work was performed under the auspices of the U.S. Department
 * of Energy by Lawrence Livermore National Laboratory in part under
 * Contract W-7405-Eng-48 and in part under Contract DE-AC52-07NA27344.
 * Produced at the Lawrence Livermore National Laboratory.
 * All rights reserved.
 * For details, see the LICENSE file.
 * LLNS Copyright End
 */

#include "dimeClientInterface.h"
#include "utilities/base64.h"
#include "zmqLibrary/zmqContextManager.h"
#include <iostream>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#include "json/jsoncpp.h"
#pragma warning(pop)
#else
#include "json/jsoncpp.h"
#endif

static griddyn::Area *gdbus;
dimeClientInterface::dimeClientInterface (const std::string &dimeName, const std::string &dimeAddress)
    : name (dimeName), address (dimeAddress)
{
    if (address.empty ())
    {
#ifdef WIN32
        address = "tcp://127.0.0.1:5000";
#else
        address = "ipc:///tmp/dime";
#endif
    }
}
dimeClientInterface::DDC_list DDC_command;
dimeClientInterface::~dimeClientInterface () = default;

void dimeClientInterface::init ()
{
    auto context = zmqlib::zmqContextManager::getContextPointer ();

    socket = std::make_unique<zmq::socket_t> (context->getBaseContext (), zmq::socket_type::req);
    socket->connect (address);

    Json_gd::Value outgoing;
    outgoing["command"] = "connect";
    outgoing["name"] = name;
    outgoing["listen_to_events"] = false;

    std::stringstream ss;

    Json_gd::FastWriter fw;
    std::string out = fw.write (outgoing);

    // writer->write(outgoing, &ss);

    socket->send (out.c_str (), out.size ());

    char buffer[3] = {};
    auto sz = socket->recv (buffer, 3, 0);
    if ((sz != 2) || (strncmp (buffer, "OK", 3) != 0))
    {
        throw initFailure ();
    }
}

void dimeClientInterface::close ()
{
    if (socket)
    {
        Json_gd::Value outgoing;
        outgoing["command"] = "exit";
        outgoing["name"] = name;

        std::stringstream ss;
        writer->write (outgoing, &ss);

        socket->send (ss.str ());

        socket->close ();
    }
    socket = nullptr;
}

std::vector<double> decodeu8todouble (std::string u8)
{
    std::string &v = u8;
    std::vector<uint8_t> xx = utilities::base64_decode (v);
    std::vector<double> inter (xx.size () / 8);

    int k = 0;

    for (int ii = 0; ii < xx.size () / 8; ++ii)
    {
        uint8_t *b = &xx[ii * 8];
        memcpy (&inter[k], b, sizeof (b));
        ++k;
    }
    return inter;
}

void decode_DR_msg (Json_gd::Value request)
{
    Json_gd::FastWriter fw;
    std::cout << "receive DR request " << std::endl;
    std::cout << request << std::endl;

    std::vector<double> bus = decodeu8todouble (request["func_args"][2]["id"]["data"].asString ());
    std::vector<double> amount = decodeu8todouble (request["func_args"][2]["action"]["data"].asString ());
    std::vector<double> duration = decodeu8todouble (request["func_args"][2]["duration"]["data"].asString ());

    for (int ii = 0; ii < bus.size (); ++ii)
    {
        DDC_command.push_back (boost::tuple<double, double, double> (bus[ii], amount[ii], duration[ii]));
    }
}

std::string dimeClientInterface::sync ()
{
    std::string cmd_type;
    std::string dev_name;
    int flg = 0;
    while (flg != 1)
    {
        char buffer[100000];
        Json_gd::Value outgoing;
        outgoing["command"] = "sync";
        outgoing["name"] = "GridDyn";
        outgoing["args"] = "";

        Json_gd::FastWriter fw;

        std::string out = fw.write (outgoing);
        socket->send (out.c_str (), out.size ());
        auto sz = socket->recv (buffer, 100000, 0);
        if ((sz != 2) || (buffer[0] != 'O') || (buffer[1] != 'K'))
        {
            std::string req (buffer);
            Json_gd::Value request;
            Json_gd::Reader readreq;
            readreq.parse (req, request);

            dev_name = request["func_args"][1].asString ();
            if (dev_name == "DDC")
            {
                cmd_type = "Demand response";
                decode_DR_msg (request);
                flg = 1;
            }
        }
    }
    return cmd_type;
}

void encodeVariableMessage (Json_gd::Value &data, double val)
{
    Json_gd::Value content;
    Json_gd::FastWriter fw;
    content["stdout"] = "";
    content["figures"] = "";
    content["datadir"] = "/tmp MatlabData/";

    Json_gd::Value response;
    response["content"] = content;
    response["result"] = val;
    response["success"] = true;
    data["args"] = fw.write (response);
}
void dimeClientInterface::send_var (const std::string &varName, double val, const std::string &recipient)
{
    // outgoing = { 'command': 'send', 'name' : self.name, 'args' : var_name }
    Json_gd::Value outgoing;

    outgoing["command"] = (recipient.empty ()) ? "broadcast" : "send";

    outgoing["name"] = name;
    outgoing["args"] = varName;

    Json_gd::FastWriter fw;
    std::string out = fw.write (outgoing);
    socket->send (out.c_str (), out.size ());

    char buffer[100];
    auto sz = socket->recv (buffer, 100, 0);
    // TODO check recv value

    Json_gd::Value outgoingData;
    outgoingData["command"] = "response";
    outgoingData["name"] = name;
    if (!recipient.empty ())
    {
        outgoingData["meta"]["recipient_name"] = recipient;
    }

    outgoingData["meta"]["var_name"] = varName;
    encodeVariableMessage (outgoingData, val);

    out = fw.write (outgoingData);
    socket->send (out.c_str (), out.size ());

    sz = socket->recv (buffer, 3, 0);
    if (sz != 2)  // TODO check for "OK"
    {
        throw (sendFailure ());
    }
}

void dimeClientInterface::broadcast (const std::string &varName, double val) { send_var (varName, val); }

std::vector<std::string> dimeClientInterface::get_devices ()
{
    std::vector<std::string> dev_list;
    char buffer[100];
    Json_gd::Value outgoing;
    outgoing["command"] = "get_devices";
    outgoing["name"] = name;

    Json_gd::FastWriter fw;

    std::string out = fw.write (outgoing);
    socket->send (out.c_str (), out.size ());

    socket->recv (buffer, 100, 0);
    std::string devlist (buffer);
    int nu = devlist.find_last_of ('}');
    std::string tempc = devlist.substr (0, nu);

    Json_gd::Reader re;
    Json_gd::Value devlistj;
    re.parse (tempc, devlistj);
    int dev_count = (int)devlistj["response"].size ();
    for (int ii = 0; ii <= dev_count - 1; ++ii)
    {
        if (devlistj["response"][ii].asString () != name)
        {
            dev_list.push_back (devlistj["response"][ii].asString ());
            std::cout << devlistj["response"][ii].asString () + " are connected with server" << std::endl;
        }
    }
    if (dev_list.empty ())
    {
        std::cout << "no client is connected" << std::endl;
    }

    return dev_list;
}

dimeClientInterface::DDC_list dimeClientInterface::get_DR_cmd () { return DDC_command; }

void dimeClientInterface::sendinfo (griddyn::Area *gdbus_f) { gdbus = gdbus_f; }
void dimeClientInterface::set_control (DDC_list DDC_command)
{
    for (int ii = 0; ii < DDC_command.size (); ii++)
    {
        int bus = boost::get<0> (DDC_command[ii]);
        double amount = boost::get<1> (DDC_command[ii]);
        std::cout << gdbus->m_Buses[bus] << std::endl;
        auto *target_bus = gdbus->m_Buses[bus];
        target_bus->S.loadP = amount;
        // gdbus->m_Buses[bus]->S.loadP = amount;
    }
}
