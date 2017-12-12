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
#include "base64.h"  
#include "dimeCollector.h"
#include "dimeClientInterface.h"
#include "zmqContextManager.h"
#include <stdio.h>
#include "zmqLibrary/zmqContextManager.h"
#include<iostream>
#include<sstream>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#include "json/json.h"
#pragma warning(pop)
#else
#include "json/json.h"
#endif

static std::vector<std::string> param;
std::vector<std::vector<double>> idxreq;
std::string namefor ="sim";

Json::Value u8stojson(std::vector<std::string> a)
{
	Json::Value wr;
	wr["data"] = a[0];
	wr["shape"] = a[1];
	wr["ndarray"] = 1;
	return wr;
}
std::string num2str(double i)

{

	std::stringstream ss;

	ss << i;

	return ss.str();

}

std::string encodedoubletou8(std::vector<double> a)
{

	uint8_t bytes2[10000000];
	for (int i = 0; i < size(a); i++)
	{
		uint8_t bytes[8];
		std::memcpy(bytes, &a[i], sizeof(a[i]));
		for (int j = 0; j < 8; j++)
		{
			bytes2[8 * i+j] = bytes[j];
		}

	}

	std::string aa = utilities::base64_encode(bytes2, 8*size(a));


	return aa;

}

std::string encodedoubletou8(double a)
{	
		uint8_t bytes[8];
		std::memcpy(bytes, &a, sizeof(a));
		std::string aa = utilities::base64_encode(bytes, 8);
     	return aa;

}


std::vector<double> decodeu8todouble(std::string u8)
{
	std::string &v = u8;
	std::vector<uint8_t> xx = utilities::base64_decode(v);
	std::vector<double> inter(xx.size()/8);

	int k = 0;

	for (int ii = 0; ii < xx.size() / 8; ++ii)
	{
		uint8_t *b = &xx[ii * 8];
		memcpy(&inter[k], b, sizeof(b));
		++k;
	}
	return inter;
}

dimeClientInterface::dimeClientInterface(const std::string &dimeName, const std::string &dimeAddress):name(dimeName),address(dimeAddress)
{
	if (address.empty())
	{

#ifdef WIN32
	//address = "tcp://127.0.0.1:5000";
		address = "tcp://10.129.132.192:9999";
#else
		address = "ipc:///tmp/dime";
#endif
	}
}

dimeClientInterface::~dimeClientInterface() = default;


void dimeClientInterface::init()
{
	auto context = zmqlib::zmqContextManager::getContextPointer();

	char buffer[10];

	socket = std::make_unique<zmq::socket_t>(context->getBaseContext(),zmq::socket_type::req);
	socket->connect(address);
		
	Json::Value outgoing;
	outgoing["command"] = "connect";
	outgoing["name"] = namefor;
	outgoing["listen_to_events"] = false;
		
	Json::FastWriter fw;

	std::string out = fw.write(outgoing);
	socket->send(out.c_str(), out.size());

	auto sz=socket->recv(buffer, 10, 0);
	if ((sz != 2) || (buffer[0] != 'O') || (buffer[1] != 'K'))
	{
		throw initFailure();
	}
	
}
	
void dimeClientInterface::close()
{
	if (socket)
	{
		Json::Value outgoing;
		outgoing["command"] = "exit";
		outgoing["name"] = name;

		Json::FastWriter fw;

		std::string out = fw.write(outgoing);
		socket->send(out.c_str(), out.size());

		socket->close();
	}
	socket = nullptr;
}


	
std::string dimeClientInterface::sync()
{
	
	std::string devname;
	int flg=1;
	while (flg)
	{
		
		char buffer[100000];
		Json::Value outgoing;
		outgoing["command"] = "sync";
		outgoing["name"] = namefor;
		outgoing["args"] = ' ';

		Json::FastWriter fw;

		std::string out = fw.write(outgoing);
		socket->send(out.c_str(), out.size());
		auto sz = socket->recv(buffer, 100000, 0);
		if ((sz != 2) || (buffer[0] != 'O') || (buffer[1] != 'K'))
		{
			std::string req(buffer);
			Json::Value request;
			Json::Reader readreq;
			readreq.parse(req, request);

			devname = request["func_args"][1].asString();

			std::vector<std::string>().swap(param);
			for (int ii = 0; ii < request["func_args"][2]["param"].size(); ++ii)
			{
				param.push_back(request["func_args"][2]["param"][ii].asString());
			}
			
			std::string vgsidx = request["func_args"][2]["vgsvaridx"]["data"].asString();
			std::vector<double> idxreqinter;

			idxreqinter = decodeu8todouble(vgsidx);
			idxreq.push_back(idxreqinter);
			flg = 0;
			std::cout << "request for" << devname << " is received" << std::endl;
		}
		if (flg == 1)
		{
			std::cout << "no clients sending request to griddyn" << std::endl;
			std::cout << "sleep 2 sceconds then keep receiving" << std::endl;
			Sleep(2000);
		}

	}

	



	return devname;
}



int dimeClientInterface::syncforcontrol(controlsignal *sp)
{

	
	    int flg=0;


		char buffer[100000];
		Json::Value outgoing;
		outgoing["command"] = "sync";
		outgoing["name"] = namefor;
		outgoing["args"] = "";

		Json::FastWriter fw;
		std::string out;
		size_t sz;
		Json::Value controlsigj;
		Json::Reader r;
		out = fw.write(outgoing);
		socket->send(out.c_str(), out.size());
		sz = socket->recv(buffer, 100000, 0);
		std::string controlsig = static_cast<std::string>(buffer);
		r.parse(controlsig, controlsigj);
		while (controlsigj["func_args"][1] != "Event")
		{
				out = fw.write(outgoing);
				socket->send(out.c_str(), out.size());
				sz = socket->recv(buffer, 100000, 0);
				if (sz == 2)
					break;
				std::string controlsig1 = static_cast<std::string>(buffer);
				r.parse(controlsig1, controlsigj);

		}
			if (controlsigj["func_args"][1] == "Event")
			{
				std::cout << "received control signal" << std::endl;
				std::vector<std::string> signamev;
				std::vector<int> flgv;
				for (size_t ii = 0; ii < controlsigj["func_args"][2]["name"].size(); ++ii)
				{
					signamev.push_back(controlsigj["func_args"][2]["name"][(int)ii].asString());
					flgv.push_back(1);
				}
				sp->flgc = flgv;
				sp->name = signamev;
				//std::string signame = controlsigj["func_args"][2]["name"].asString();
				std::string id = controlsigj["func_args"][2]["id"]["data"].asString();
				sp->id= decodeu8todouble(id);
				std::string action = controlsigj["func_args"][2]["action"]["data"].asString();
				sp->action = decodeu8todouble(action);
				std::string duration = controlsigj["func_args"][2]["duration"]["data"].asString();
				sp->duration = decodeu8todouble(duration);
				std::string time = controlsigj["func_args"][2]["time"]["data"].asString();
				sp->timec = decodeu8todouble(time);
				flg = 1;
				return flg;
			}
			else
			{
				return flg;
			}

		
//do something with the control signal;
}	



std::vector<std::string> dimeClientInterface::get_devices()
{
re0:
	std::vector<std::string> dev_list;
	char buffer[100];
	Json::Value outgoing;
	outgoing["command"] = "get_devices";
	outgoing["name"] = namefor;

	Json::FastWriter fw;

	std::string out = fw.write(outgoing);
	socket->send(out.c_str(), out.size());

	socket->recv(buffer, 100, 0);
	std::string devlist(buffer);
	int nu = devlist.find_last_of('}');
	std::string tempc = devlist.substr(0, nu);

	Json::Reader re;
	Json::Value devlistj;
	re.parse(tempc, devlistj);
	std::string finallist = fw.write(devlistj["response"]);
	

	while (1)
	{
		try
		{
			finallist.replace(finallist.find("["), 1, "");
			finallist.replace(finallist.find("]"), 1, "");

		}
		catch (const std::exception&)
		{
		}

		try
		{
			finallist.replace(finallist.find("\""), 1, "");

		}
		catch (const std::exception&)
		{
			finallist.replace(finallist.find("\n"), 1, "");
			break;
		}
	}
	std::cout << finallist+" are connected with server" << std::endl;
	while (1)
	{

		nu = finallist.find_first_of(',');
		if (nu != -1)
		{
			tempc = finallist.substr(0, nu);
			if (tempc== namefor)
			{
				finallist = finallist.substr(nu + 1, finallist.size());
				continue;
			}
			dev_list.push_back(tempc);
			finallist = finallist.substr(nu + 1, finallist.size());
		}
		else
		{
			tempc = finallist.substr(0, nu);
			if (tempc == namefor)
			{
			   break;
			}
			dev_list.push_back(finallist);

			break;
		}
		
	}
	if (dev_list.empty())
	{
		std::cout << "no client is connected" << std::endl;
		std::cout << "sleep 2 second then keep calling" << std::endl;
		Sleep(2000);
		goto re0;	
	}

	return dev_list;


}





/*void encodereqVariableMessage(Json::Value &data, Json::Value reqvar,Json::Value reqvarheader, double t,double coutn)
{
	Json::Value wr;

	Json::Value addforshow;



	addforshow.append(reqvar);

	wr["k"] = coutn;
	wr["t"] = t;
	wr["vars"] = addforshow;
	wr["accurate"] = addforshow;
	Json::FastWriter fw;
	Json::Value content;
	content["datadir"] = "/tmp MatlabData/";

	Json::Value response;
	response["content"] = content;
	response["result"] = wr;
	response["success"] = true;
	data["args"] = fw.write(response);
}
*/
/*void encodeIdxvgs(Json::Value &data, Json::Value nbusvolk, Json::Value nlinepk, Json::Value nbusfreqk, Json::Value nbusthetak, Json::Value nbusgenreactivek, Json::Value nbusgenrealk, Json::Value nbusloadreactivelk, Json::Value nbusloadrealk, Json::Value nsynomegaj, Json::Value nsyndeltaj, Json::Value nlineij, Json::Value nlineqj, Json::Value nexc, Json::Value ne1d, Json::Value ne2d, Json::Value ne1q, Json::Value ne2q)
{
Json::Value wr;

Json::Value exc;
Json::Value syni;
Json::Value line;
Json::Value bus;

bus["V"] = nbusvolk;
bus["freq"] = nbusfreqk;
bus["loadp"] = nbusloadrealk;
bus["loadq"] = nbusloadreactivelk;
bus["genp"] = nbusgenrealk;
bus["genq"] = nbusgenreactivek;

line["I"] = nlineij;
line["p"] = nlinepk;
line["q"] = nlineqj;

syni["e1d"] = ne1d;
syni["e2d"] = ne2d;
syni["e1q"] = ne1q;
syni["e2q"] = ne2q;
syni["delta"] = nsyndeltaj;
syni["omega"] = nsynomegaj;

exc["vm"] = nexc;

wr["Syn"] = syni;
wr["Line"] = line;
wr["Bus"] = bus;
wr["exc"] = exc;


Json::FastWriter fw;
Json::Value content;
content["stdout"] = "";
content["figures"] = "";
content["datadir"] = "/tmp MatlabData/";

Json::Value response;
response["content"] = content;
response["result"] = wr;
response["success"] = true;
data["args"] = fw.write(response);
}
*/


void encodereqVariableMessage(Json::Value &data, std::string reqvar,std::vector<double>shapeforreq, Json::Value reqvarheader, double t, double coutn)
{
	Json::Value wr;

	std::string shape = encodedoubletou8(shapeforreq);

	Json::Value var;
	var["ndarray"] = 1;
	var["data"] = reqvar;
	var["shape"] = shape;
	


	wr["k"] = coutn;
	wr["t"] = t;
	wr["vars"] = var;
	wr["accurate"] = var;
	Json::FastWriter fw;
	Json::Value content;
	
	content["datadir"] = "C:\\User\\pro\\AppData\\Local\\Temp\\MatlabData";
	content["figure"] = "";
	content["stdout"] = "";
	



	Json::Value response;
	response["content"] = content;
	response["result"] = wr;
	response["success"] = true;
	data["args"] = fw.write(response);

}
void encodeVarheader(Json::Value &data, Json::Value Varheader)
{
	Json::FastWriter fw;
	Json::Value content;
	content["stdout"] = "";
	content["figures"] = "";
	content["datadir"] = "/tmp MatlabData/";

	Json::Value response;
	response["content"] = content;
	response["result"] = Varheader;
	response["success"] = true;
	data["args"] = fw.write(response);
}
void encodesysname(Json::Value &data, Json::Value sysname)
{
	Json::Value re;
/*	Json::Value err;
	std::vector<double> rx;
	rx.push_back(1);
	rx.push_back(2);
	std::vector<double> rx1;
	rx1.push_back(2);
	rx1.push_back(1);
	std::vector<std::string> jj;
	jj.push_back(encodedoubletou8(rx));
	jj.push_back(encodedoubletou8(rx1));
	err = u8stojson(jj);

	re["test"] = err;
	*/


	re["Bus"] = sysname;

	Json::FastWriter fw;
	Json::Value content;
	content["stdout"] = "";
	content["figures"] = "";
	content["datadir"] = "/tmp MatlabData/";

	Json::Value response;
	response["content"] = content;
	response["result"] = re;
	response["success"] = true;
	data["args"] = fw.write(response);
}
void encodesysparam(Json::Value &data, std::vector <std::string> BUSd, std::vector <std::string> PQd, std::vector <std::string> PVd, std::vector <std::string> lined,int nbus,int nline, std::vector <std::string> Genroud, std::vector <std::string> Fsd, std::vector <std::string> Swd)
{
	Json::Value re1;
	for (int ii = 0; ii < param.size(); ++ii)
	{


		if (param[ii] == "Bus")
		{
			re1["Bus"] = u8stojson(BUSd);
		}
		if (param[ii] == "PQ")
		{
			re1["PQ"] = u8stojson(PQd);
		}
		if (param[ii] == "PV")
		{
			re1["PV"] = u8stojson(PVd);
		}
		if (param[ii] == "Line")
		{
			re1["Line"] = u8stojson(lined);
		}
		if (param[ii] == "nbus")
		{
			re1["nbus"] = nbus;
		}
		if (param[ii] == "nline")
		{
			re1["nline"] = nline;
		}
		if (param[ii] == "Syn")
		{
			re1["Syn"] = u8stojson(Genroud);
		}
		if (param[ii] == "Fixshunt")
		{
			re1["Shunt"] = u8stojson(Fsd);
		}
		if (param[ii] == "Sw")
		{
			re1["SW"] = u8stojson(Swd);
		}
	}

	Json::FastWriter fw;
	Json::Value content;
	content["stdout"] = "";
	content["figures"] = "";
	content["datadir"] = "/tmp MatlabData/";

	Json::Value response;
	response["content"] = content;
	response["result"] = re1;
	response["success"] = true;
	data["args"] = fw.write(response);
}

void encodeIdxvgs(Json::Value &data, std::vector<std::string> nbusvolk, std::vector<std::string>  nlinepk, std::vector<std::string>  nbusfreqk, std::vector<std::string>  nbusthetak, std::vector<std::string>  nbusgenreactivek, std::vector<std::string>  nbusgenrealk, std::vector<std::string>  nbusloadreactivelk, std::vector<std::string>  nbusloadrealk, std::vector<std::string>  nsynomegaj, std::vector<std::string>  nsyndeltaj, std::vector<std::string>  nlineij, std::vector<std::string>  nlineqj, std::vector<std::string>  nexc, std::vector<std::string>  ne1d, std::vector<std::string>  ne2d, std::vector<std::string>  ne1q, std::vector<std::string>  ne2q)
{
	Json::Value wr;
	Json::Value wrr;

	Json::Value exc;
	Json::Value syni;
	Json::Value line;
	Json::Value bus;

	bus["V"] = u8stojson(nbusvolk);
	bus["theta"] = u8stojson(nbusthetak);
	bus["w_Busfreq"] = u8stojson(nbusfreqk);
	bus["P"] = u8stojson(nbusloadrealk);
	bus["Q"] = u8stojson(nbusloadreactivelk);
	//bus["genp"] = u8stojson(nbusgenrealk);
	//bus["genq"] = u8stojson(nbusgenreactivek);

	line["I"] = u8stojson(nlineij);
	line["p"] = u8stojson(nlinepk);
	line["q"] = u8stojson(nlineqj);

	syni["e1d"] = u8stojson(ne1d);
	syni["e2d"] = u8stojson(ne2d);
	syni["e1q"] = u8stojson(ne1q);
	syni["e2q"] = u8stojson(ne2q);
	syni["delta"] = u8stojson(nsyndeltaj);
	syni["omega"] = u8stojson(nsynomegaj);

	exc["vm"] = u8stojson(nexc);

	wr["Syn"] = syni;
	wr["Line"] = line;
	wr["Bus"] = bus;
	wr["Exc"] = exc;


	Json::FastWriter fw;
	Json::Value content;
	content["stdout"] = "";
	content["figures"] = "";
	content["datadir"] = "/tmp MatlabData/";

	Json::Value response;
	response["content"] = content;
	response["result"] = wr;
	response["success"] = true;
	data["args"] = fw.write(response);
}
/*void encodeIdxvgs(Json::Value &data, Json::Value nbusvolk, Json::Value nlinepk, Json::Value nbusfreqk, Json::Value nbusthetak, Json::Value nbusgenreactivek, Json::Value nbusgenrealk, Json::Value nbusloadreactivelk, Json::Value nbusloadrealk, Json::Value nsynomegaj, Json::Value nsyndeltaj, Json::Value nlineij, Json::Value nlineqj, Json::Value nexc, Json::Value ne1d, Json::Value ne2d, Json::Value ne1q, Json::Value ne2q)
{
	Json::Value wr;

	Json::Value exc;
	Json::Value syni;
	Json::Value line;
	Json::Value bus;

	bus["V"] = nbusvolk;
	bus["freq"] = nbusfreqk;
	bus["loadp"] = nbusloadrealk;
	bus["loadq"] = nbusloadreactivelk;
	bus["genp"] = nbusgenrealk;
	bus["genq"] = nbusgenreactivek;

	line["I"] = nlineij;
	line["p"] = nlinepk;
	line["q"] = nlineqj;

	syni["e1d"] = ne1d;
	syni["e2d"] = ne2d;
	syni["e1q"] = ne1q;
	syni["e2q"] = ne2q;
	syni["delta"] = nsyndeltaj;
	syni["omega"] = nsynomegaj;

	exc["vm"] = nexc;

	wr["Syn"] = syni;
	wr["Line"] = line;
	wr["Bus"] = bus;
	wr["exc"] = exc;


	Json::FastWriter fw;
	Json::Value content;
	content["stdout"] = "";
	content["figures"] = "";
	content["datadir"] = "/tmp MatlabData/";

	Json::Value response;
	response["content"] = content;
	response["result"] = wr;
	response["success"] = true;
	data["args"] = fw.write(response);
}
*/


void dimeClientInterface::send_varname(Json::Value Varheader,  const std::string &recipient)
{
	//outgoing = { 'command': 'send', 'name' : self.name, 'args' : var_name }
	char buffer[10];

	Json::Value outgoing;

	outgoing["command"] = (recipient.empty()) ? "broadcast" : "send";

	outgoing["name"] = namefor;
	outgoing["args"] = "";
	Json::FastWriter fw;

	std::string out = fw.write(outgoing);

	socket->send(out.c_str(), out.size());

	auto sz = socket->recv(buffer, 10, 0);

	Json::Value outgoingData;
	outgoingData["command"] = "response";
	outgoingData["name"] = namefor;
	if (!recipient.empty())
	{
		outgoingData["meta"]["recipient_name"] = "SE";
	}

	outgoingData["meta"]["var_name"] = "Varheader";
	encodeVarheader(outgoingData, Varheader);

	out = fw.write(outgoingData);

	socket->send(out.c_str(), out.size());
	sz = socket->recv(buffer, 10, 0);
	if (sz != 2)
	{
		throw(sendFailure());
	}

}
void dimeClientInterface::send_sysname(Json::Value Sysname,  const std::string &recipient)
{
	//outgoing = { 'command': 'send', 'name' : self.name, 'args' : var_name }
	char buffer[10];

	Json::Value outgoing;

	outgoing["command"] = (recipient.empty()) ? "broadcast" : "send";

	outgoing["name"] = namefor;
	outgoing["args"] = "";
	Json::FastWriter fw;

	std::string out = fw.write(outgoing);

	socket->send(out.c_str(), out.size());

	auto sz = socket->recv(buffer, 10, 0);

	Json::Value outgoingData;
	outgoingData["command"] = "response";
	outgoingData["name"] = namefor;
	if (!recipient.empty())
	{
		outgoingData["meta"]["recipient_name"] = "SE";
	}

	outgoingData["meta"]["var_name"] = "SysName";
	encodesysname(outgoingData, Sysname);

	out = fw.write(outgoingData);

	socket->send(out.c_str(), out.size());
	sz = socket->recv(buffer, 10, 0);
	if (sz != 2)
	{
		throw(sendFailure());
	}

}

void dimeClientInterface::send_sysparam(std::vector <std::string> BUSd, std::vector <std::string> PQd, std::vector <std::string> PVd, std::vector <std::string> lined,int nbus,int nline, std::vector <std::string> Genroud, std::vector <std::string> Fsd, std::vector <std::string> Swd, const std::string &recipient)
{




	//outgoing = { 'command': 'send', 'name' : self.name, 'args' : var_name }
	char buffer[10];

	Json::Value outgoing;

	outgoing["command"] = (recipient.empty()) ? "broadcast" : "send";

	outgoing["name"] = namefor;
	outgoing["args"] = "";
	Json::FastWriter fw;

	std::string out = fw.write(outgoing);

	socket->send(out.c_str(), out.size());

	auto sz = socket->recv(buffer, 10, 0);

	Json::Value outgoingData;
	outgoingData["command"] = "response";
	outgoingData["name"] = namefor;
	if (!recipient.empty())
	{
		outgoingData["meta"]["recipient_name"] = recipient;
	}

	outgoingData["meta"]["var_name"] = "SysParam";
	encodesysparam(outgoingData, BUSd,PQd,PVd,lined,nbus,nline,Genroud,Fsd,Swd);

	out = fw.write(outgoingData);

	socket->send(out.c_str(), out.size());
	sz = socket->recv(buffer, 10, 0);
	if (sz != 2)
	{
		throw(sendFailure());
	}

}
/*void dimeClientInterface::send_sysparam(Json::Value BUSd, Json::Value PQd, Json::Value PVd, Json::Value lined,int nbus,int nline, Json::Value Genroud, Json::Value Fsd, Json::Value Swd, const std::string &recipient)
{




	//outgoing = { 'command': 'send', 'name' : self.name, 'args' : var_name }
	char buffer[10];

	Json::Value outgoing;

	outgoing["command"] = (recipient.empty()) ? "broadcast" : "send";

	outgoing["name"] = namefor;
	outgoing["args"] = "";
	Json::FastWriter fw;

	std::string out = fw.write(outgoing);

	socket->send(out.c_str(), out.size());

	auto sz = socket->recv(buffer, 10, 0);

	Json::Value outgoingData;
	outgoingData["command"] = "response";
	outgoingData["name"] = namefor;
	if (!recipient.empty())
	{
		outgoingData["meta"]["recipient_name"] = recipient;
	}

	outgoingData["meta"]["var_name"] = "SysParam";
	encodesysparam(outgoingData, BUSd,PQd,PVd,lined,nbus,nline,Genroud,Fsd,Swd);

	out = fw.write(outgoingData);

	socket->send(out.c_str(), out.size());
	sz = socket->recv(buffer, 10, 0);
	if (sz != 2)
	{
		throw(sendFailure());
	}

}
*/


void dimeClientInterface::send_Idxvgs(std::vector<std::string>  nbusvolk, std::vector<std::string>  nlinepk, std::vector<std::string>  nbusfreqk, std::vector<std::string>  nbusthetak, std::vector<std::string>  nbusgenreactivek, std::vector<std::string>  nbusgenrealk, std::vector<std::string>  nbusloadreactivelk, std::vector<std::string>  nbusloadrealk, std::vector<std::string>  nsynomegaj, std::vector<std::string>  nsyndeltaj, std::vector<std::string>  nlineij, std::vector<std::string>  nlineqj, std::vector<std::string>  nexc, std::vector<std::string>  ne1d, std::vector<std::string>  ne2d, std::vector<std::string>  ne1q, std::vector<std::string>  ne2q, const std::string &recipient)
{

	//outgoing = { 'command': 'send', 'name' : self.name, 'args' : var_name }
	char buffer[10];

	Json::Value outgoing;

	outgoing["command"] = (recipient.empty()) ? "broadcast" : "send";

	outgoing["name"] = namefor;
	outgoing["args"] = "";
	Json::FastWriter fw;

	std::string out = fw.write(outgoing);

	socket->send(out.c_str(), out.size());

	auto sz = socket->recv(buffer, 10, 0);

	Json::Value outgoingData;
	outgoingData["command"] = "response";
	outgoingData["name"] = namefor;
	if (!recipient.empty())
	{
		outgoingData["meta"]["recipient_name"] = recipient;
	}

	outgoingData["meta"]["var_name"] = "Idxvgs";
	encodeIdxvgs(outgoingData, nbusvolk, nlinepk, nbusfreqk, nbusthetak, nbusgenreactivek, nbusgenrealk, nbusloadreactivelk, nbusloadrealk, nsynomegaj, nsyndeltaj, nlineij, nlineqj, nexc, ne1d, ne2d, ne1q, ne2q);
	out = fw.write(outgoingData);

	socket->send(out.c_str(), out.size());
	sz = socket->recv(buffer, 10, 0);
	if (sz != 2)
	{
		throw(sendFailure());
	}
	





}
/*void dimeClientInterface::send_Idxvgs(Json::Value nbusvolk, Json::Value nlinepk, Json::Value nbusfreqk, Json::Value nbusthetak, Json::Value nbusgenreactivek, Json::Value nbusgenrealk, Json::Value nbusloadreactivelk, Json::Value nbusloadrealk, Json::Value nsynomegaj, Json::Value nsyndeltaj, Json::Value nlineij, Json::Value nlineqj, Json::Value nexc, Json::Value ne1d, Json::Value ne2d, Json::Value ne1q, Json::Value ne2q, const std::string &recipient)
{

//outgoing = { 'command': 'send', 'name' : self.name, 'args' : var_name }
char buffer[10];

Json::Value outgoing;

outgoing["command"] = (recipient.empty()) ? "broadcast" : "send";

outgoing["name"] = namefor;
outgoing["args"] = "";
Json::FastWriter fw;

std::string out = fw.write(outgoing);

socket->send(out.c_str(), out.size());

auto sz = socket->recv(buffer, 10, 0);

Json::Value outgoingData;
outgoingData["command"] = "response";
outgoingData["name"] = namefor;
if (!recipient.empty())
{
outgoingData["meta"]["recipient_name"] = recipient;
}

outgoingData["meta"]["var_name"] = "Idxvgs";
encodeIdxvgs(outgoingData, nbusvolk, nlinepk, nbusfreqk, nbusthetak, nbusgenreactivek, nbusgenrealk, nbusloadreactivelk, nbusloadrealk, nsynomegaj, nsyndeltaj, nlineij, nlineqj, nexc, ne1d, ne2d, ne1q, ne2q);
out = fw.write(outgoingData);

socket->send(out.c_str(), out.size());
sz = socket->recv(buffer, 10, 0);
if (sz != 2)
{
throw(sendFailure());
}






}
*/

void dimeClientInterface::send_reqvar(int coutn,double t,std::string reqvar,std::vector<double>shapeforreq,Json::Value reqvarheader, const std::string &recipient)
{
	//outgoing = { 'command': 'send', 'name' : self.name, 'args' : var_name }
	char buffer[10];

	Json::Value outgoing;
	Json::FastWriter fw;
	outgoing["command"] = (recipient.empty()) ? "broadcast" : "send";
	outgoing["name"] = namefor;
	outgoing["args"] = "";


	std::string out = fw.write(outgoing);

	socket->send(out.c_str(), out.size());

	auto sz = socket->recv(buffer, 10, 0);

	Json::Value outgoingData;
	outgoingData["command"] = "response";
	outgoingData["name"] = namefor;
	if (!recipient.empty())
	{
		outgoingData["meta"]["recipient_name"] = recipient;
	}

	outgoingData["meta"]["var_name"] = "Varvgs";
	encodereqVariableMessage(outgoingData,reqvar,shapeforreq,reqvarheader, t,coutn);


	out = fw.write(outgoingData);

	socket->send(out.c_str(), out.size());
	sz = socket->recv(buffer, 10, 0);
	if (sz != 2)
	{
		throw(sendFailure());
	}


}
/*void dimeClientInterface::send_reqvar(int coutn,double t,Json::Value reqvar,Json::Value reqvarheader, const std::string &recipient)
{
	//outgoing = { 'command': 'send', 'name' : self.name, 'args' : var_name }
	char buffer[10];

	Json::Value outgoing;
	Json::FastWriter fw;
	outgoing["command"] = (recipient.empty()) ? "broadcast" : "send";

	outgoing["name"] = namefor;
	outgoing["args"] = "";


	std::string out = fw.write(outgoing);

	socket->send(out.c_str(), out.size());

	auto sz = socket->recv(buffer, 10, 0);

	Json::Value outgoingData;
	outgoingData["command"] = "response";
	outgoingData["name"] = namefor;
	if (!recipient.empty())
	{
		outgoingData["meta"]["recipient_name"] = recipient;
	}

	outgoingData["meta"]["var_name"] = "Varvgs";
	encodereqVariableMessage(outgoingData,reqvar,reqvarheader, t,coutn);


	out = fw.write(outgoingData);

	socket->send(out.c_str(), out.size());
	sz = socket->recv(buffer, 10, 0);
	if (sz != 2)
	{
		throw(sendFailure());
	}


}
*/



