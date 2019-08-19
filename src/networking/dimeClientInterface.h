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

#pragma once
#include "Area.h"
#include "gridbus.h"
#include "cppzmq/zmq_addon.hpp"
#include "json/jsoncpp.h"
#include <exception>
#include <memory>
#include <string>
#include <vector>
#include "boost/tuple/tuple.hpp"

class initFailure : public std::exception
{
  public:
    initFailure (){};
};

class sendFailure : public std::exception
{
  public:
    sendFailure (){};
};

class dimeClientInterface
{
  private:
    std::string name;
    std::string address;

  public:
    dimeClientInterface (const std::string &dimeName, const std::string &dimeAddress = "");

    ~dimeClientInterface ();
    /** initialize the connection*/
    void init ();
    /** close the connection*/
    void close ();
    /** sync with the server*/
    std::string sync ();
    /** send a variable to server*/
    void send_var (const std::string &varName, double val, const std::string &recipient = "");
    
	void broadcast (const std::string &varName, double val);

    std::vector<std::string> get_devices ();

	typedef std::vector<boost::tuple<double, double, double>> DDC_list;

	DDC_list get_DR_cmd ();

	void dimeClientInterface::set_control (DDC_list DDC_command);
    void dimeClientInterface::sendinfo (griddyn::Area *gdbus_f);
  private:
    std::unique_ptr<zmq::socket_t> socket;
    std::unique_ptr<Json_gd::StreamWriter> writer{};
};
