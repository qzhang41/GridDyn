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
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#include "json/json.h"
#pragma warning(pop)
#else
#include "json/json.h"
#endif

#ifndef DIME_CLIENT_INTERFACE_HEADER_
#define DIME_CLIENT_INTERFACE_HEADER_

#include "cppzmq/zmq_addon.hpp"
#include <string>
#include <exception>
#include <memory>
extern std::vector<std::vector<double>> idxreq;
std::string encodedoubletou8(double a);
std::string encodedoubletou8(std::vector<double> a);
class controlsignal
{
public: std::vector<std::string> name;
		std::vector<double> id;
		std::vector<double> duration;
		std::vector<double> action;
		std::vector<double> timec;
		std::vector<int> flgc;
};
class initFailure :public std::exception
{
public:
		initFailure() {};
};

class sendFailure :public std::exception
{
public:
	sendFailure() {};
};

class dimeClientInterface
{
private:
	std::string name;
	std::string address;
public:


	dimeClientInterface(const std::string &name, const std::string &address = "");

	~dimeClientInterface();
	/** initialize the connection*/
	void init();
	/** close the connection*/
	void close();
	/** sync with the server*/
	std::string sync();
	int syncforcontrol(controlsignal * sp);
	//void send_var(double t,Json::Value Varvgs, const std::string & recipient);
	void send_varname(Json::Value Varheader, const std::string & recipient);
	void send_sysname(Json::Value Sysname, const std::string & recipient);
	void send_sysparam(std::vector<std::string> BUSd, std::vector<std::string> PQd, std::vector<std::string> PVd, std::vector<std::string> lined, int nbus, int nline, std::vector<std::string> Genroud, std::vector<std::string> Fsd, std::vector<std::string> Swd, const std::string & recipient);
	
	//void send_sysparam(Json::Value Busd, Json::Value PQd, Json::Value PVd, Json::Value lined,int nbus,int nline, Json::Value Genroud, Json::Value Fsd, Json::Value Swd, const std::string & recipient);
	
	/** send a variable to server*/
	//void send_Idxvgs(Json::Value nbusvolk, Json::Value nlinepk, Json::Value nbusfreqk, Json::Value nbusthetak, Json::Value nbusgenreactivek, Json::Value nbusgenrealk, Json::Value nbusloadreactivelk, Json::Value nbusloadrealk, Json::Value nsynomegaj, Json::Value nsyndeltaj, Json::Value nlineij, Json::Value nlineqj, Json::Value nexc, Json::Value ne1d, Json::Value ne2d, Json::Value ne1q, Json::Value ne2q, const std::string &recipient);
	void send_Idxvgs(std::vector<std::string> nbusvolk, std::vector<std::string> nlinepk, std::vector<std::string> nbusfreqk, std::vector<std::string> nbusthetak, std::vector<std::string> nbusgenreactivek, std::vector<std::string> nbusgenrealk, std::vector<std::string> nbusloadreactivelk, std::vector<std::string> nbusloadrealk, std::vector<std::string> nsynomegaj, std::vector<std::string> nsyndeltaj, std::vector<std::string> nlineij, std::vector<std::string> nlineqj, std::vector<std::string> nexc, std::vector<std::string> ne1d, std::vector<std::string> ne2d, std::vector<std::string> ne1q, std::vector<std::string> ne2q, const std::string & recipient);

	void send_reqvar(int coutn, double t, std::string reqvar, std::vector<double> shapeforreq, Json::Value reqvarheader, const std::string & recipient);

	//void send_reqvar(int coutn, double t, std::string reqvar, Json::Value reqvarheader, const std::string & recipient);

	//void send_reqvar(int coutn,double t,Json::Value reqvar, Json::Value reqvarheader, const std::string & recipient);


	std::vector<std::string> get_devices();
private:
	std::unique_ptr<zmq::socket_t> socket;
};
#endif