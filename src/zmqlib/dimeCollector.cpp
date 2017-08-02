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

#include "gridBus.h"
#include "Link.h"
#include <iostream>
#include "dimeCollector.h"
#include "dimeClientInterface.h"
#include "core/helperTemplates.hpp"
#include <string>
#include <sstream>
using namespace std;




static Json::Value Busd;
static Json::Value PQd;
static Json::Value PVd;
static Json::Value lined;
static Json::Value Genroud;
static Json::Value Fsd;
static Json::Value Swd;
static Json::Value Busnamed;

static int swidx;
static int swv;
static int nbus;
static int nline;
static std::vector<std::string> dev_list;
static Json::Value Idxvgs;
std::vector<std::string> devname;
#include "core/helperTemplates.hpp"


namespace griddyn
{
namespace dimeLib
{

static std::vector<Link*> dimlinkinfo;
static std::vector<gridBus*> dimbusinfo;
static std::vector<controlsignal> queueforcon;

dimeCollector::dimeCollector(coreTime time0, coreTime period):collector(time0,period)
{

}

dimeCollector::dimeCollector(const std::string &collectorName):collector(collectorName)
{

}

dimeCollector::~dimeCollector()
{
	if (dime)
	{
		dime->close();
	}
	std::cout << "all requested vars at all time are sended" << std::endl;
}

std::shared_ptr<collector> dimeCollector::clone(std::shared_ptr<collector> gr) const
{
	auto nrec = cloneBase<dimeCollector, collector>(this, gr);
	if (!nrec)
	{
		return gr;
	}

	nrec->server = server;
	nrec->processName = processName;

	return nrec;
}

//Idxvgs encode;
Json::Value idxvgsdoubleencod(int a, int b)
{
	Json::Value re;
	for (int ii = a; ii <= b; ++ii)
	{
		Json::Value ninterk;
		ninterk.append(ii);
		re.append(ninterk);
	}

	return re;
}
int idxfindend(int a, int b)
{
	int newend = a + b;
	return newend;

}

void setcontrol(double t)
{
	int sicon = queueforcon.size();
	for (int si = 0; si < sicon; ++si)
	{

		std::vector<std::string> controlname = queueforcon[si].name;
		std::vector<double> controlaction = queueforcon[si].action;
		std::vector<double> controlid = queueforcon[si].id;
		std::vector<double> controltime = queueforcon[si].timec;
		std::vector<double> controlduration = queueforcon[si].duration;
		std::vector<int> flg = queueforcon[si].flgc;
		int k = controlname.size();

		for (int ss = 0; ss < k; ++ss)
		{
			if ((queueforcon[si].flgc)[ss] == -1)
			{
				if (t == controltime[ss] + controlduration[ss]&& controlduration[ss]!=0)
				{
					if (controlaction[ss] == 0)
					{
						if (controlname[ss] == "Line")
						{
							dimlinkinfo[controlid[ss]]->reconnect();
						}
						if (controlname[ss] == "Bus")
						{
							dimbusinfo[controlid[ss]]->reconnect();
						}
					}
					else
					{
						if (controlname[ss] == "Line")
						{
							dimlinkinfo[controlid[ss]]->disconnect();
						}
						if (controlname[ss] == "Bus")
						{
							dimbusinfo[controlid[ss]]->disconnect();
						}
					}
					(queueforcon[si].flgc)[ss] = 0;
				}
			}
			else if ((queueforcon[si].flgc)[ss] == 1)
			{
				if (controltime[ss] == t)
				{
					if (controlaction[ss] == 0)
					{
						if (controlname[ss] == "Line")
						{
							dimlinkinfo[controlid[ss]]->disconnect();
						}
						if (controlname[ss] == "Bus")
						{
							dimbusinfo[controlid[ss]]->disconnect();
						}
					}
					else
					{
						if (controlname[ss] == "Line")
						{
							dimlinkinfo[controlid[ss]]->reconnect();
						}
						if (controlname[ss] == "Bus")
						{
							dimbusinfo[controlid[ss]]->reconnect();
						}
					}
					(queueforcon[si].flgc)[ss] = -1;
				}
			}
			else
			{
				continue;
			}
		}

	}
	std::vector<int> a(3, 0);
	for (std::vector<controlsignal>::iterator sp = queueforcon.begin(); sp !=queueforcon.end();)
	{
		if ((*sp).flgc == a)
		{
			sp=queueforcon.erase(sp);
		}
		else
		{
			sp++;
		}
	}

}



void dimeCollector::sendinfo(std::vector<Link*> linkinfo, std::vector<gridBus*> businfo)
{
	dimlinkinfo = linkinfo;
	dimbusinfo = businfo;
}

//orgnize how to send to clients
change_code dimeCollector::trigger(coreTime time)
{
	double t = time;
	Json::Value Varvgs;
	Json::Value Varheader;
	
	

	if (!dime)
	{
		dime = std::make_unique<dimeClientInterface>(processName, server);
		dime->init();
	}
	auto out=collector::trigger(time);
	//figure out what to do with the data

#pragma region var and name compile
	std::string strv = "V";
	std::vector<double> voltagec;
	std::vector<std::string> voltagename;

	std::string strang = "theta";
	std::vector<double> anglec;
	std::vector<std::string> anglename;

	std::string strfreq = "freq";
	std::vector<double> freqc;
	std::vector<std::string> freqname;

	std::string strreactive = "gen_reactive";
	std::vector<double> reactivec;
	std::vector<std::string> reactivename;

	std::string strreal = "gen_real";
	std::vector<double> realc;
	std::vector<std::string> realname;

	std::string strloadreac = "load_reactive";
	std::vector<double> loadreacc;
	std::vector<std::string> loadreacname;

	std::string strloadreal = "load_real";
	std::vector<double> loadrealc;
	std::vector<std::string> loadrealname;

	std::string stromega = "omega";
	std::vector<double> omegac;
	std::vector<std::string> omeganame;

	std::string strpf = "pf_";
	std::vector<double> pfc;
	std::vector<std::string> pfname;

	std::string stri = "I";
	std::vector<double> Ic;
	std::vector<std::string> Iname;

	std::string strlp = "line_p";
	std::vector<double> linepc;
	std::vector<std::string> linepname;

	std::string strlq = "line_q";
	std::vector<double> lineqc;
	std::vector<std::string> lineqname;

	std::string strexc = "exc_syn";
	std::vector<double> excc;
	std::vector<std::string> excname;

	std::string str1d = "e1d_syn";
	std::vector<double> e1dc;
	std::vector<std::string> e1dname;

	std::string str2d = "e2d_syn";
	std::vector<double> e2dc;
	std::vector<std::string> e2dname;

	std::string str1q = "e1q_syn";
	std::vector<double> e1qc;
	std::vector<std::string> e1qname;

	std::string str2q = "e2q_syn";
	std::vector<double> e2qc;
	std::vector<std::string> e2qname;

	//for each data determine who they are
	for (size_t kk = 0; kk < points.size(); ++kk)
	{
		

		


		if (points[kk].colname.find(strv) != string::npos)
		{
			voltagec.push_back(data[kk]);
			voltagename.push_back(points[kk].colname);
		}

		if (points[kk].colname.find(strlp) != string::npos)
		{
			linepc.push_back(data[kk]);
			linepname.push_back(points[kk].colname);
		}

		if (points[kk].colname.find(strang) != string::npos)
		{
			anglec.push_back(data[kk]);
			anglename.push_back(points[kk].colname);
		}

		if (points[kk].colname.find(strfreq) != string::npos)
		{
			freqc.push_back(data[kk]);
			freqname.push_back(points[kk].colname);
		}

		if (points[kk].colname.find(strreactive) != string::npos)
		{
			reactivec.push_back(data[kk]);
			reactivename.push_back(points[kk].colname);
		}

		if (points[kk].colname.find(strreal) != string::npos)
		{
		    realc.push_back(data[kk]);
			realname.push_back(points[kk].colname);
		}

		if (points[kk].colname.find(strloadreac) != string::npos)
		{
			loadreacc.push_back(data[kk]);
			loadreacname.push_back(points[kk].colname);
		}

		if (points[kk].colname.find(strloadreal) != string::npos)
		{
			loadrealc.push_back(data[kk]);
			loadrealname.push_back(points[kk].colname);
		}

		if (points[kk].colname.find(stromega) != string::npos)
		{
			omegac.push_back(data[kk]);
			omeganame.push_back(points[kk].colname);
		}

		if (points[kk].colname.find(strpf) != string::npos)
		{
			pfc.push_back(data[kk]);
			pfname.push_back(points[kk].colname);
		}
		if (points[kk].colname.find(stri) != string::npos)
		{
			Ic.push_back(data[kk]);
			Iname.push_back(points[kk].colname);
		}

		if (points[kk].colname.find(strlq) != string::npos)
		{
			lineqc.push_back(data[kk]);
			lineqname.push_back(points[kk].colname);
		}

		if (points[kk].colname.find(strexc) != string::npos)
		{
			excc.push_back(data[kk]);
			excname.push_back(points[kk].colname);
		}

		if (points[kk].colname.find(str1d) != string::npos)
		{
			e1dc.push_back(data[kk]);
			e1dname.push_back(points[kk].colname);
		}

		if (points[kk].colname.find(str2d) != string::npos)
		{
			e2dc.push_back(data[kk]);
			e2dname.push_back(points[kk].colname);
		}

		if (points[kk].colname.find(str1q) != string::npos)
		{
			e1qc.push_back(data[kk]);
			e1qname.push_back(points[kk].colname);
		}

		if (points[kk].colname.find(str2q) != string::npos)
		{
			e2qc.push_back(data[kk]);
			e2qname.push_back(points[kk].colname);
		}
	}
	std::vector<double> total;
	std::vector<std::string> totalname;




	total.insert(total.end(), voltagec.begin(), voltagec.end());
	total.insert(total.end(), linepc.begin(), linepc.end());
	total.insert(total.end(), freqc.begin(), freqc.end());
	total.insert(total.end(), anglec.begin(), anglec.end());
	total.insert(total.end(), reactivec.begin(), reactivec.end());
	total.insert(total.end(), realc.begin(), realc.end());
	total.insert(total.end(), loadreacc.begin(), loadreacc.end());
	total.insert(total.end(), loadrealc.begin(), loadrealc.end());
	total.insert(total.end(), omegac.begin(), omegac.end());
	total.insert(total.end(), pfc.begin(), pfc.end());
	total.insert(total.end(), Ic.begin(), Ic.end());
	total.insert(total.end(), lineqc.begin(), lineqc.end());
	total.insert(total.end(), excc.begin(), excc.end());
	total.insert(total.end(), e1dc.begin(), e1dc.end());
	total.insert(total.end(), e2dc.begin(), e2dc.end());
	total.insert(total.end(), e1qc.begin(), e1qc.end());
	total.insert(total.end(), e2qc.begin(), e2qc.end());
	
	totalname.insert(totalname.end(), voltagename.begin(), voltagename.end());
	totalname.insert(totalname.end(), freqname.begin(), freqname.end());
	totalname.insert(totalname.end(), linepname.begin(), linepname.end());
	totalname.insert(totalname.end(), anglename.begin(), anglename.end());
	totalname.insert(totalname.end(), reactivename.begin(), reactivename.end());
	totalname.insert(totalname.end(), realname.begin(), realname.end());
	totalname.insert(totalname.end(), loadreacname.begin(), loadreacname.end());
	totalname.insert(totalname.end(), loadrealname.begin(), loadrealname.end());
	totalname.insert(totalname.end(), omeganame.begin(), omeganame.end());
	totalname.insert(totalname.end(), pfname.begin(), pfname.end());
	totalname.insert(totalname.end(), Iname.begin(), Iname.end());
	totalname.insert(totalname.end(), lineqname.begin(), lineqname.end());
	totalname.insert(totalname.end(), excname.begin(), excname.end());
	totalname.insert(totalname.end(), e1dname.begin(), e1dname.end());
	totalname.insert(totalname.end(), e2dname.begin(), e2dname.end());
	totalname.insert(totalname.end(), e1qname.begin(), e1qname.end());
	totalname.insert(totalname.end(), e2qname.begin(), e2qname.end());
	
	for (size_t kk = 0; kk < points.size(); ++kk)
	{
		
		Json::Value wrvar;
		wrvar.append(total[kk]);
		Varvgs.append(wrvar);



		Json::Value wrvarname;
		Json::Value wagain;
		wrvarname.append(totalname[kk]);
		wagain.append(wrvarname);
		Varheader.append(wagain);
		
	}


#pragma endregion


#pragma region compile Idxvgs
	int nbusvol = voltagec.size();
	int nlinep = linepc.size();
	int nbusfreq = freqc.size();
	int nbustheta = anglec.size();
	int nbusgenreactive = reactivec.size();
	int nbusgenreal = realc.size();
	int nbusloadreactive = loadreacc.size();
	int nbusloadreal = loadrealc.size();
	int nsynomega = omegac.size();
	int nsyndelta = pfc.size();
	int nlineI = Ic.size();
	int nlineq = lineqc.size();
	int nexcv = excc.size();
	int nsyne1d = e1dc.size();
	int nsyne2d = e2dc.size();
	int nsyne1q = e1qc.size();
	int nsyne2q = e2qc.size();


	int linepend = idxfindend(nbusvol, nlinep);
	int busfreqend = idxfindend(linepend, nbusfreq);
	int busthetaend = idxfindend(busfreqend, nbustheta);
	int busgenreactiveend = idxfindend(busthetaend, nbusgenreactive);
	int busgenrealend = idxfindend(busgenreactiveend, nbusgenreal);
	int busloadreactiveend = idxfindend(busgenrealend, nbusloadreactive);
	int busloadrealend = idxfindend(busloadreactiveend, nbusloadreal);
	int synomegaend = idxfindend(busloadrealend, nsynomega);
	int syndeltaend = idxfindend(synomegaend, nsyndelta);
	int LineIend = idxfindend(syndeltaend, nlineI);
	int Lineqend = idxfindend(LineIend, nlineq);
	int excend = idxfindend(Lineqend, nexcv);
	int syne1d = idxfindend(excend, nsyne1d);
	int syne2d = idxfindend(syne1d, nsyne2d);
	int syne1q = idxfindend(syne2d, nsyne1q);
	int syne2q = idxfindend(syne1q, nsyne2q);


	Json::Value nbusvolk = idxvgsdoubleencod(1, nbusvol);
	Json::Value nlinepk = idxvgsdoubleencod(nbusvol + 1, linepend);
	Json::Value nbusfreqk = idxvgsdoubleencod(linepend + 1, busfreqend);
	Json::Value nbusthetak = idxvgsdoubleencod(busfreqend + 1, busthetaend);
	Json::Value nbusgenreactivek = idxvgsdoubleencod(busfreqend + 1, busgenreactiveend);
	Json::Value nbusgenrealk = idxvgsdoubleencod(busgenreactiveend + 1, busgenrealend);
	Json::Value nbusloadreactivelk = idxvgsdoubleencod(busgenrealend + 1, busloadreactiveend);
	Json::Value nbusloadrealk = idxvgsdoubleencod(busloadreactiveend, busloadrealend);
	Json::Value nsynomegaj = idxvgsdoubleencod(busloadrealend + 1, synomegaend);
	Json::Value nsyndeltaj = idxvgsdoubleencod(synomegaend + 1, syndeltaend);
	Json::Value nlineij = idxvgsdoubleencod(syndeltaend + 1, LineIend);
	Json::Value nlineqj = idxvgsdoubleencod(LineIend + 1, Lineqend);
	Json::Value nexc = idxvgsdoubleencod(Lineqend + 1, excend);
	Json::Value ne1d = idxvgsdoubleencod(excend + 1, syne1d);
	Json::Value ne2d = idxvgsdoubleencod(syne1d + 1, syne2d);
	Json::Value ne1q = idxvgsdoubleencod(syne2d + 1, syne1q);
	Json::Value ne2q = idxvgsdoubleencod(syne1q + 1, syne2q);



#pragma endregion 



	if (t == 0)
	{
	  dev_list = dime->get_devices();
	}


		for (int ii = 0; ii < dev_list.size(); ++ii)
		{
			if (t==0&&ii==0)
			{
				dime->send_sysname(Busnamed, "");
				dime->send_Idxvgs(nbusvolk, nlinepk, nbusfreqk, nbusthetak, nbusgenreactivek, nbusgenrealk, nbusloadreactivelk, nbusloadrealk, nsynomegaj, nsyndeltaj, nlineij, nlineqj, nexc, ne1d, ne2d, ne1q, ne2q, "");
				dime->send_varname(Varheader, "");
				std::cout << "boradcast is finished" << std::endl;
			}
		    //	Sleep(1000); //broadcast;

// find request var


				if (t == 0)
				{
					devname.push_back(dime->sync());
				}
				//sync req;

				Json::Value reqvarheader;
				Json::Value reqvar;

				for (int jj = 0; jj < idxreq[ii].size(); ++jj)
				{
					Json::Value inter;
					inter.append(total[idxreq[ii][jj]]);
					reqvar.append(inter);

					Json::Value interh;
					interh.append(totalname[idxreq[ii][jj]]);
					reqvarheader.append(inter);
				}

				if (t == 0)
				{
					dime->send_sysparam(Busd, PQd, PVd, lined, nbus, nline, Genroud, Fsd, Swd, devname[ii]);
					std::cout << "requested param is sended" << std::endl;
					std::cout << "requested var is started to be sended" << std::endl;
				}
				//send sysparam



				dime->send_reqvar(t, reqvar, reqvarheader, devname[ii]);
				
				//dime->send_var(t, Varvgs,dev_list[ii]);

			

		}
	     //sync controlsig no matter from who 
		//get controlsig until there is none
		while (1)
		{
			auto controlsig = std::make_unique<controlsignal>();
			int fg = dime->syncforcontrol(controlsig.get());
			controlsignal ncon = *controlsig;
			if (fg == 1)
			{
				queueforcon.push_back(ncon);
			}
			else
			{
				break;
			}
		}
		if (!queueforcon.empty())
		{
			setcontrol(t);
		}
	return out;
}




//compile the sysparam in order
void dimeCollector::sendsysparam(std::vector<std::string> Busdata, std::vector<std::string> Loaddata, std::vector<std::string> Generatordata, std::vector<std::string> Branchdata, std::vector<std::string> Transformerdata, std::vector <std::vector<double>> Genroudata, std::vector<std::string> Fixshuntdata,std::vector <std::string> sysname,std::vector<int> Baseinfor)
{
	std::unique_ptr<dimeClientInterface> dime = std::make_unique<dimeClientInterface>("", "");
	dime->init();
	nbus = Busdata.size();
	nline = Branchdata.size() + (Transformerdata.size() / 4);


	for (size_t kk = 0; kk < sysname.size(); ++kk)
	{
		Json::Value wsysname;
		Json::Value wagain;
		wsysname.append(sysname[kk]);
		wagain.append(wsysname);
	    Busnamed.append(wagain);
	}
	



	std::vector<std::vector<double>> arr;
	for (size_t kk = 0; kk < Busdata.size(); ++kk)
	{
		std::string businter = Busdata[kk];
		std::vector<double> interv;
		for (int ii = 0; ii < Busdata[kk].length(); ++ii)
		{
			
			if (businter.find(",") != string::npos)
			{
				int nu = businter.find_first_of(',');
				std::string tempc = businter.substr(0, nu);
				try
				{

					istringstream iss(tempc);
					double indexc;
					iss >> indexc;
					interv.push_back(indexc);
					businter = businter.substr(nu + 1, Busdata[kk].length());
				}
				catch (const std::exception&)
				{
					businter = businter.substr(nu + 1, Busdata[kk].length());
					continue;
				}

			}
			else
			{
				double indexc = std::stoul(businter);
				interv.push_back(indexc);
				break;
			}
			
		}
		arr.push_back(interv);
	}
	for (size_t kk = 0; kk < Busdata.size(); ++kk)
	{
		Json::Value Busk;
		if (arr[kk][3]==3)
		{
			swidx = arr[kk][0];
			swv= arr[kk][7];
		}
		int num1[6] = { 0,2,7,8,4,5, };
		for each(int i in num1)
		{
			Busk.append(arr[kk][i]);
		}
		Busd.append(Busk);
	}

	std::vector<std::vector<double>> pqrr;
	for (size_t kk = 0; kk < Loaddata.size(); ++kk)
	{
		std::vector<double> interv;
		std::string loadinter = Loaddata[kk];
		int op = 0;
		for (int ii = 0; ii < Loaddata[kk].length(); ++ii)
		{
			if (loadinter.find(",") != string::npos)
			{
				int nu = loadinter.find_first_of(',');
				std::string tempc = loadinter.substr(0, nu);
				try
				{

					istringstream iss(tempc);
					double indexc;
					iss >> indexc;
				    interv.push_back(indexc);
					loadinter = loadinter.substr(nu + 1, Loaddata[kk].length());
					op = op + 1;
				}
				catch (const std::exception&)
				{
					loadinter = loadinter.substr(nu + 1, Loaddata[kk].length());
					op = op + 1;
					continue;
				}

			}
			else
			{
				double indexc = std::stoul(loadinter);
				interv.push_back(indexc);
				break;
			}

		}
		pqrr.push_back(interv);
	}
	for (size_t kk = 0; kk < Loaddata.size(); ++kk)
	{
		Json::Value PQk;
		int pqidx = pqrr[kk][0];
		double maxv = 1.1;
		double minv = 0.9;
		double paramp = (pqrr[kk][5] + pqrr[kk][7] * arr[pqidx - 1][7] + pqrr[kk][9] * (arr[pqidx - 1][7])* (arr[pqidx - 1][7]/*start from 0 so need -1*/)) / Baseinfor[0];
		double paramq = (pqrr[kk][6] + pqrr[kk][8] * arr[pqidx - 1][7] - pqrr[kk][10] * (arr[pqidx - 1][7])* (arr[pqidx - 1][7])) / Baseinfor[0];
		double pqinter[9] = { pqidx,Baseinfor[0],arr[pqidx - 1][2],paramp,paramq,maxv,minv,1,1 };
		for (int ii = 0; ii < 9; ++ii)
		{
			PQk.append(pqinter[ii]);
		}
		PQd.append(PQk);

	}

	std::vector<std::vector<double>> pvrr;
	for (size_t kk = 0; kk < Generatordata.size(); ++kk)
	{
		std::string geninter = Generatordata[kk];
		int op = 0;
		std::vector<double> interv;
		for (int ii = 0; ii < Generatordata[kk].length(); ++ii)
		{

			if (geninter.find(",") != string::npos)
			{
				int nu = geninter.find_first_of(',');
				std::string tempc = geninter.substr(0, nu);
				try
				{

					istringstream iss(tempc);
					double indexc;
					iss >> indexc;
					interv.push_back(indexc);
					geninter = geninter.substr(nu + 1, Generatordata[kk].length());
					op = op + 1;
				}
				catch (const std::exception&)
				{
					geninter = geninter.substr(nu + 1, Generatordata[kk].length());
					op = op + 1;
					continue;
				}

			}
			else
			{
				double indexc = std::stoul(geninter);
				interv.push_back(indexc);

				break;
			}
	
		}
		pvrr.push_back(interv);
	}
	for (size_t kk = 0; kk < Generatordata.size(); ++kk)
	{
		Json::Value PVk;
		int pvidx = pvrr[kk][0];
		double pg = (pvrr[kk][14] * pvrr[kk][2]) / Baseinfor[0];
		double qmax = pvrr[kk][4] / Baseinfor[0];
		double qmin = pvrr[kk][5] / Baseinfor[0];
		double maxv = 1.1;
		double minv = 0.9;
		double pvinter[11] = { pvidx,pvrr[kk][8],arr[pvidx - 1][2],pg,arr[pvidx - 1][7],qmax,qmin,maxv,minv,1,pvrr[kk][14] };
		for (int ii = 0; ii < 11; ++ii)
		{
			PVk.append(pvinter[ii]);
		}
		PVd.append(PVk);
		
		if (pvidx == swidx)
		{
			double swinter[13]= { swidx,pvrr[kk][8],arr[pvidx - 1][2],swv,arr[pvidx - 1][7],qmax,qmin,maxv,minv,1,1,1,pvrr[kk][14] };
			Json::Value swk;
			Json::Value swkk;
			for (int ii = 0; ii < 13; ++ii)
			{
				swk.append(swinter[ii]);
			}
			Swd.append(swk);
			Swd.append(swkk);
		}

	}
	std::vector<std::vector<double>> branchrr;
	for (size_t kk = 0; kk < Branchdata.size(); ++kk)
	{
		std::string branchinter = Branchdata[kk];
		int op = 0;
		std::vector <double> interv;
		for (int ii = 0; ii < Branchdata[kk].length(); ++ii)
		{
			
			if (branchinter.find(",") != string::npos)
			{
				int nu = branchinter.find_first_of(',');
				std::string tempc = branchinter.substr(0, nu);
				try
				{

					istringstream iss(tempc);
					double indexc;
					iss >> indexc;
					interv.push_back(indexc);
					branchinter = branchinter.substr(nu + 1, Branchdata[kk].length());
					op = op + 1;
				}
				catch (const std::exception&)
				{
					branchinter = branchinter.substr(nu + 1, Branchdata[kk].length());
					op = op + 1;
					continue;
				}

			}
			else
			{
				double indexc = std::stoul(branchinter);
				interv.push_back(indexc);
				break;
			}
			
		}
		branchrr.push_back(interv);
	}
	
	std::vector<std::vector<double>> transrr;

	for (size_t kk = 0; kk < Transformerdata.size(); ++kk)
	{
		std::string transinter = Transformerdata[kk];
		std::vector<double> interv;
		for (int ii = 0; ii < Transformerdata[kk].length(); ++ii)
		{

			if (transinter.find(",") != string::npos)
			{
				int nu = transinter.find_first_of(',');
				std::string tempc = transinter.substr(0, nu);
				try
				{

					istringstream iss(tempc);
					double indexc;
					iss >> indexc;
					interv.push_back(indexc);
					transinter = transinter.substr(nu + 1, Transformerdata[kk].length());
				}
				catch (const std::exception&)
				{
					transinter = transinter.substr(nu + 1, Transformerdata[kk].length());
					continue;
				}

			}
			else
			{
				int jk = kk;
				int mo = jk % 4;
				if (mo == 0)
				{
					continue;
				}
				else
				{
					double indexc = std::stoul(transinter);
					interv.push_back(indexc);
					break;
				}
			}

		}
		transrr.push_back(interv);
	}
	for (size_t kk = 0; kk < Branchdata.size(); ++kk)
	{
		Json::Value linek;
		int lineidxfrom = branchrr[kk][0];
		int lineidxto = branchrr[kk][1];
		double rate_c = branchrr[kk][8];
		double Vnl = arr[lineidxfrom - 1][2];
		double freq = Baseinfor[1];
		double length = branchrr[kk][14];
		double r = branchrr[kk][3];
		double x = branchrr[kk][4];
		double b = branchrr[kk][5];
		double status = branchrr[kk][13];
		double pvinter[16] = { lineidxfrom,lineidxto,rate_c,Vnl,freq,1121,length,r,x,b,1121,1121,1121,1121,1121,status };
		for (int ii = 0; ii < 16; ++ii)
		{
			linek.append(pvinter[ii]);
		}
		lined.append(linek);
	}
	for (size_t kk = 0; kk < (Transformerdata.size()) / 4; ++kk)
	{
		Json::Value linek;
		int lineidxfrom = transrr[kk * 4][0];
		int lineidxto = transrr[kk * 4][1];
		double rate_a = transrr[kk * 4 + 2][3];
		double Vnl = arr[lineidxfrom - 1][7];
		double freq = Baseinfor[1];
		double r = transrr[kk * 4 + 1][0];
		double x = transrr[kk * 4 + 1][1];
		double b = transrr[kk * 4][8];
		double u = transrr[kk * 4][11];
		double pvinter[16] = { lineidxfrom,lineidxto,rate_a,Vnl,freq,1121,1121,r,x,b,1121,1121,1121,1121,1121,u };
		for (int ii = 0; ii < 16; ++ii)
		{
			linek.append(pvinter[ii]);
		}
		lined.append(linek);
	}


	std::vector<std::vector<double>> fshuntrr;
	for (size_t kk = 0; kk < Fixshuntdata.size(); ++kk)
	{
		std::string fsinter = Fixshuntdata[kk];
		std::vector<double> interv;
		for (int ii = 0; ii < Fixshuntdata[kk].length(); ++ii)
		{
			if (fsinter.find(",") != string::npos)
			{
				int nu = fsinter.find_first_of(',');
				std::string tempc = fsinter.substr(0, nu);
				try
				{

					istringstream iss(tempc);
					double indexc;
					iss >> indexc;
					interv.push_back(indexc);
					fsinter = fsinter.substr(nu + 1, Fixshuntdata[kk].length());
				}
				catch (const std::exception&)
				{
					fsinter = fsinter.substr(nu + 1, Loaddata[kk].length());
					continue;
				}

			}
			else
			{
				double indexc = std::stoul(fsinter);
				interv.push_back(indexc);
				break;
			}
		}
		fshuntrr.push_back(interv);
	}
	for (size_t kk = 0; kk < Fixshuntdata.size(); ++kk)
	{
		Json::Value fsk;
		int fsidx = fshuntrr[kk][0];
		double mva = Baseinfor[0];
		double vn = arr[fsidx - 1][2];
		double freq = Baseinfor[1];
		double g = fshuntrr[kk][3] / mva;
		double b= fshuntrr[kk][4] / mva;
		double status = fshuntrr[kk][2];
		double fsinter[7] = {fsidx,mva,vn,freq,g,b,status};
		for (int ii = 0; ii < 7; ++ii)
		{
			fsk.append(fsinter[ii]);
		}
		Fsd.append(fsk);
	}

	//for Genrou
	for (size_t kk = 0; kk < Genroudata.size(); ++kk)
	{
		Json::Value Genrouk;
		int genrouidx = Genroudata[kk][0];
		double ra;
		double sn;
		for (size_t jj = 0; jj < Generatordata.size(); ++jj)
		{
			if (pvrr[jj][0] == genrouidx)
			{
				ra = pvrr[jj][9];
				sn = pvrr[jj][8];
			}
		}
		double vn = arr[genrouidx - 1][2];
		double freq = Baseinfor[1];
		double x1 = Genroudata[kk][14];

		double xd = Genroudata[kk][9];
		double xd1 = Genroudata[kk][11];
		double xd2 = Genroudata[kk][13];
		double td10 = Genroudata[kk][3];
		double td20 = Genroudata[kk][4];
		double xq = Genroudata[kk][10];
		double xq1 = Genroudata[kk][12];
		double xq2 = Genroudata[kk][13];
		double tq10 = Genroudata[kk][5];
		double tq20 = Genroudata[kk][6];
		double M = 2 * Genroudata[kk][7];
		double d = Genroudata[kk][8];


		double genrouinter[] = { genrouidx,sn,vn,freq,6/*type*/,x1,ra,xd,xd1,xd2,td10,td20,xq,xq1,xq2,tq10,tq20,M,d,1121,1121,1121,1121,1121,1121,1121,1121,1 };
		for (int ii = 0; ii < 28; ++ii)
		{
			Genrouk.append(genrouinter[ii]);
		}
		Genroud.append(Genrouk);
	}



	//dime->send_sysparam(Busd, PQd, PVd, lined, nbus, nline, Genroud,Fsd,Swd, "SE");
}

void dimeCollector::set(const std::string &param, double val)
{
	
	collector::set(param, val);
}

void dimeCollector::set(const std::string &param, const std::string &val)
{
	if (param == "server")
	{
		server = val;
	}
	else if (param == "processname")
	{
		processName = val;
	}
	else
	{
		collector::set(param, val);
	}
	
}

const std::string &dimeCollector::getSinkName() const
{
	return server;
}







}//namespace dimeLib
}//namespace griddyn


