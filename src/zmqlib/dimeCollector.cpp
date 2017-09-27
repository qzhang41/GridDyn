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


#include "gridBus.h"
#include "Link.h"
#include <iostream>
#include "dimeCollector.h"
#include "dimeClientInterface.h"
#include "core/helperTemplates.hpp"
#include <string>
#include <sstream>
using namespace std;




/*Json::Value Busd;
Json::Value PQd;
Json::Value PVd;
Json::Value lined;
Json::Value Genroud;
Json::Value Fsd;
Json::Value Swd;
Json::Value Busnamed;*/
std::vector <std::string> Busd;
std::vector <std::string> PQd;
std::vector <std::string> PVd;
std::vector <std::string> lined;
std::vector <std::string> Genroud;
std::vector <std::string> Fsd;
std::vector <std::string> Swd;
Json::Value Busnamed;



std::vector <std::string> sysname2;
std::vector <std::string> Busdata2;
std::vector <std::string> Loaddata2;
std::vector <std::string> Branchdata2;
std::vector <std::string> Transformerdata2;
std::vector <std::string> Generatordata2;
std::vector <std::string> Fixshuntdata2;
std::vector <int> Baseinfor2;
std::vector <std::vector<double>> Genroudata2;
std::vector<double> total;
std::vector<std::string> totalname;
Json::Value Varvgs;
Json::Value Varheader;

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
int coutn = 0;
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


/*Json::Value idxvgsdoubleencod(int a, int b)
{
	Json::Value re;
	for (int ii = a; ii <= b; ++ii)
	{
		Json::Value ninterk;
		ninterk.append(ii);
		re.append(ninterk);
	}

	return re;
}*/
//Idxvgs encode;
std::vector<std::string> idxvgsencode(int a, int b)
{
	std::vector<std::string> re;
	std::vector<double> sha;
	std::string inn;
	std::vector<double> in;
	for (int ii = a; ii <= b; ++ii)
	{
		in.push_back(ii);
	}
	sha.push_back(b - a + 1);
	sha.push_back(1);
	inn = encodedoubletou8(in);
	re.push_back(inn);
	inn = encodedoubletou8(sha);
	re.push_back(inn);
	return re;
}
int idxfindend(int a, int b)
{
	int newend = a + b;
	return newend;

}

void setcontrol(double t)
{
	int sicon =static_cast<int> (queueforcon.size());
	for (int si = 0; si < sicon; ++si)
	{

		std::vector<std::string> controlname = queueforcon[si].name;
		std::vector<double> controlaction = queueforcon[si].action;
		std::vector<double> controlid = queueforcon[si].id;
		std::vector<double> controltime = queueforcon[si].timec;
		std::vector<double> controlduration = queueforcon[si].duration;
		std::vector<int> flg = queueforcon[si].flgc;
		int k =static_cast<int> (controlname.size());

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


//gain control info
void dimeCollector::sendinfo(std::vector<Link*> linkinfo, std::vector<gridBus*> businfo)
{
	dimlinkinfo = linkinfo;
	dimbusinfo = businfo;
}

//gain raw info
void dimeCollector::sendrawinfo(std::vector<std::string> Busdata1, std::vector<std::string> Loaddata1, std::vector<std::string> Generatordata1, std::vector<std::string> branchdata1, std::vector<std::string> transformerdata1, std::vector<std::string> Fixshuntdata1, std::vector <std::string> sysname1, std::vector<int> Baseinfor1)
{
sysname2=sysname1;
Busdata2=Busdata1;
Loaddata2=Loaddata1;
Branchdata2=branchdata1;
Transformerdata2=transformerdata1;
Generatordata2=Generatordata1;
Fixshuntdata2=Fixshuntdata1;
Baseinfor2 = Baseinfor1;
}

//gain dyr info
void dimeCollector::senddyninfo(std::vector <std::vector<double>> Genroudata1)
{
	Genroudata2 = Genroudata1;
}

//compile the sysparam from raw and dyr in order and into json
void dimeCollector::encodesysparam(std::vector<std::string> Busdata, std::vector<std::string> Loaddata, std::vector<std::string> Generatordata, std::vector<std::string> Branchdata, std::vector<std::string> Transformerdata, std::vector <std::vector<double>> Genroudata, std::vector<std::string> Fixshuntdata, std::vector <std::string> sysname, std::vector<int> Baseinfor)
{
	nbus = static_cast<int> (Busdata.size());
	nline = Branchdata.size() + (Transformerdata.size() / 4);


	for (size_t kk = 0; kk < sysname.size(); ++kk)
	{
		Json::Value wsysname;
		wsysname.append(sysname[kk]);
		Busnamed.append(wsysname);
	}



	std::vector<double> Busk;
	std::vector<double> sizebus;
	std::vector<std::vector<double>> arr;
	int num1[8] = { 0,2,7,8,4,5,13,14 };
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
				if (businter.find("/") != string::npos)
				{
					int nu = businter.find_first_of('/');
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

		}
		arr.push_back(interv);
	}
	/*for (size_t kk = 0; kk < Busdata.size(); ++kk)
	{
		Json::Value Busk;
		if (arr[kk][3] == 3)
		{
			swidx = arr[kk][0];
			swv = arr[kk][7];
		}
		int num1[8] = { 0,2,7,8,4,5,13,14};
		for each(int i in num1)
		{
			Busk.append(arr[kk][i]);
		}
		Busd.append(Busk);
	}*/

	for each(int i in num1)
	{
		for (size_t kk = 0; kk < Busdata.size(); ++kk)
		{

			if (arr[kk][3] == 3)
			{
				swidx = arr[kk][0];
				swv = arr[kk][7];
			}
			Busk.push_back(arr[kk][i]);
		}
	}

	sizebus.push_back(size(arr));
	sizebus.push_back(8);
	Busd.push_back(encodedoubletou8(Busk));
	Busd.push_back(encodedoubletou8(sizebus));


	std::vector<double> PQk;
	std::vector<double> sizePQ;
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

	/*	for (size_t kk = 0; kk < Loaddata.size(); ++kk)
	{
		Json::Value PQk;
		int pqidx = pqrr[kk][0];
		double maxv = 1.1;
		double minv = 0.9;
		double inter;
		double inter2;
		for (int jk = 0; jk < arr.size(); jk++)
		{
			if (arr[jk][0] == pqidx)
			{
				 inter = arr[jk][7];
				 inter2 = arr[jk][2];
			}
		}

		double paramp = (pqrr[kk][5] + pqrr[kk][7] * inter + pqrr[kk][9] * (inter)* (inter/??//*start from 0 so need -1///////)) / Baseinfor[0];
	double paramq = (pqrr[kk][6] + pqrr[kk][8] * inter - pqrr[kk][10] * (inter)* (inter)) / Baseinfor[0];
	double pqinter[9] = { pqidx,Baseinfor[0],inter2,paramp,paramq,maxv,minv,1,1 };
	for (int ii = 0; ii < 9; ++ii)
	{
		PQk.append(pqinter[ii]);
	}
	PQd.append(PQk);

	}*/
    for (int ii = 0; ii < 9; ++ii)
{
	for (size_t kk = 0; kk < Loaddata.size(); ++kk)
	{
		int pqidx = pqrr[kk][0];
		double maxv = 1.1;
		double minv = 0.9;
		double inter;
		double inter2;
		for (int jk = 0; jk < arr.size(); jk++)
		{
			if (arr[jk][0] == pqidx)
			{
				 inter = arr[jk][7];
				 inter2 = arr[jk][2];
			}
		}

		double paramp = (pqrr[kk][5] + pqrr[kk][7] * inter + pqrr[kk][9] * (inter)* (inter/*start from 0 so need -1*/)) / Baseinfor[0];
		double paramq = (pqrr[kk][6] + pqrr[kk][8] * inter - pqrr[kk][10] * (inter)* (inter)) / Baseinfor[0];
		double pqinter[9] = { pqidx,Baseinfor[0],inter2,paramp,paramq,maxv,minv,1,1 };
        PQk.push_back(pqinter[ii]);
	}
}


	sizePQ.push_back(size(pqrr));
	sizePQ.push_back(9);
	PQd.push_back(encodedoubletou8(PQk));
	PQd.push_back(encodedoubletou8(sizePQ));





	std::vector<double> SWk;
	std::vector<double> sizeSW;
	std::vector<double> PVk;
	std::vector<double> sizePV;
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
	/*for (size_t kk = 0; kk < Generatordata.size(); ++kk)
	{
	Json::Value PVk;
	int pvidx = pvrr[kk][0];
	double pg = (pvrr[kk][14] * pvrr[kk][2]) / Baseinfor[0];
	double qmax = pvrr[kk][4] / Baseinfor[0];
	double qmin = pvrr[kk][5] / Baseinfor[0];
	double maxv = 1.1;
	double minv = 0.9;

	double inter;
	double inter2;
	for (int jk = 0; jk < arr.size(); jk++)
	{
	if (arr[jk][0] == pvidx)
	{
	inter = arr[jk][7];
	inter2 = arr[jk][2];
	}
	}


	double pvinter[11] = { pvidx,pvrr[kk][8],inter2,pg,inter,qmax,qmin,maxv,minv,1,pvrr[kk][14] };
	for (int ii = 0; ii < 11; ++ii)
	{
	PVk.append(pvinter[ii]);
	}
	PVd.append(PVk);

	if (pvidx == swidx)
	{
	double swinter[13] = { swidx,pvrr[kk][8],inter2,swv,inter,qmax,qmin,maxv,minv,1,1,1,pvrr[kk][14] };
	Json::Value swk;
	Json::Value swkk;
	for (int ii = 0; ii < 13; ++ii)
	{
	swk.append(swinter[ii]);
	}
	Swd.append(swk);
	Swd.append(swkk);
	}

	}*/
	int swn = 0;
	for (int ii = 0; ii < 11; ++ii)
	{
		for (size_t kk = 0; kk < Generatordata.size(); ++kk)
		{
			int pvidx = pvrr[kk][0];
			double pg = (pvrr[kk][14] * pvrr[kk][2]) / Baseinfor[0];
			double qmax = pvrr[kk][4] / Baseinfor[0];
			double qmin = pvrr[kk][5] / Baseinfor[0];
			double maxv = 1.1;
			double minv = 0.9;

			double inter;
			double inter2;
			for (int jk = 0; jk < arr.size(); jk++)
			{
				if (arr[jk][0] == pvidx)
				{
					inter = arr[jk][7];
					inter2 = arr[jk][2];
				}
			}


			double pvinter[11] = { pvidx,pvrr[kk][8],inter2,pg,inter,qmax,qmin,maxv,minv,1,pvrr[kk][14] };

			PVk.push_back(pvinter[ii]);
		}
	}

	sizePV.push_back(size(pvrr));
	sizePV.push_back(11);
	PVd.push_back(encodedoubletou8(PVk));
	PVd.push_back(encodedoubletou8(sizePV));


	for (int ii = 0; ii < 13; ++ii)
	{
		for (size_t kk = 0; kk < Generatordata.size(); ++kk)
		{
			int pvidx = pvrr[kk][0];
			double pg = (pvrr[kk][14] * pvrr[kk][2]) / Baseinfor[0];
			double qmax = pvrr[kk][4] / Baseinfor[0];
			double qmin = pvrr[kk][5] / Baseinfor[0];
			double maxv = 1.1;
			double minv = 0.9;

			double inter;
			double inter2;
			for (int jk = 0; jk < arr.size(); jk++)
			{
				if (arr[jk][0] == pvidx)
				{
					inter = arr[jk][7];
					inter2 = arr[jk][2];
				}
			}
			if (pvidx == swidx)
			{
				double swinter[13] = { swidx,pvrr[kk][8],inter2,swv,inter,qmax,qmin,maxv,minv,1,1,1,pvrr[kk][14] };
				swn++;

				for (int ii = 0; ii < 13; ++ii)
				{
					SWk.push_back(swinter[ii]);
				}
			}

		}
	}



	sizeSW.push_back(swn);
	sizeSW.push_back(13);
	Swd.push_back(encodedoubletou8(SWk));
	Swd.push_back(encodedoubletou8(sizeSW));






	std::vector<double> linek;
	std::vector<double> lineknew;
	std::vector<double> sizeline;

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

		int lineidxfrom = branchrr[kk][0];
		int lineidxto = branchrr[kk][1];
		double rate_c = branchrr[kk][8];


		double inter;
		double inter2;
		for (int jk = 0; jk < arr.size(); jk++)
		{
			if (arr[jk][0] == lineidxfrom)
			{
				inter = arr[jk][7];
				inter2 = arr[jk][2];
			}
		}



		double Vnl = inter2;
		double freq = Baseinfor[1];
		double length = branchrr[kk][14];
		double r = branchrr[kk][3];
		double x = branchrr[kk][4];
		double b = branchrr[kk][5];
		double status = branchrr[kk][13];
		double pvinter[16] = { lineidxfrom,lineidxto,rate_c,Vnl,freq,1121,length,r,x,b,1121,1121,1121,1121,1121,status };
		for (int ii = 0; ii < 16; ++ii)
		{
			linek.push_back(pvinter[ii]);
		}
	}
	for (size_t kk = 0; kk < (Transformerdata.size()) / 4; ++kk)
	{
		int lineidxfrom = transrr[kk * 4][0];
		int lineidxto = transrr[kk * 4][1];
		double rate_a = transrr[kk * 4 + 2][3];


		double inter;
		double inter2;
		for (int jk = 0; jk < arr.size(); jk++)
		{
			if (arr[jk][0] == lineidxfrom)
			{
				inter = arr[jk][7];
				inter2 = arr[jk][2];
			}
		}



		double Vnl = inter;
		double freq = Baseinfor[1];
		double r = transrr[kk * 4 + 1][0];
		double x = transrr[kk * 4 + 1][1];
		double b = transrr[kk * 4][8];
		double u = transrr[kk * 4][11];
		double pvinter[16] = { lineidxfrom,lineidxto,rate_a,Vnl,freq,1121,1121,r,x,b,1121,1121,1121,1121,1121,u };
		for (int ii = 0; ii < 16; ++ii)
		{
			linek.push_back(pvinter[ii]);
		}
	}

	for (int ii = 0; ii < 16; ii++)
	{
		for (size_t kk = 0; kk < (Transformerdata.size() / 4) + Branchdata.size(); ++kk)
		{
			double lk = linek[ii +kk * 16];
			lineknew.push_back(lk);
		}
	}



	sizeline.push_back((Transformerdata.size()/4)+ Branchdata.size());
	sizeline.push_back(16);
	lined.push_back(encodedoubletou8(lineknew));
	lined.push_back(encodedoubletou8(sizeline));




	std::vector<double> fsk;
	std::vector<double> sizefs;

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
	for (int ii = 0; ii < 7; ++ii)
	{
		for (size_t kk = 0; kk < Fixshuntdata.size(); ++kk)
		{
			int fsidx = fshuntrr[kk][0];
			double mva = Baseinfor[0];

			double inter;
			double inter2;
			for (int jk = 0; jk < arr.size(); jk++)
			{
				if (arr[jk][0] == fsidx)
				{
					inter = arr[jk][7];
					inter2 = arr[jk][2];
				}
			}


			double vn = inter2;
			double freq = Baseinfor[1];
			double g = fshuntrr[kk][3] / mva;
			double b = fshuntrr[kk][4] / mva;
			double status = fshuntrr[kk][2];
			double fsinter[7] = { fsidx,mva,vn,freq,g,b,status };
		    fsk.push_back(fsinter[ii]);
		}
	}
	sizefs.push_back(size(fshuntrr));
	sizefs.push_back(7);
	Fsd.push_back(encodedoubletou8(fsk));
	Fsd.push_back(encodedoubletou8(sizefs));



	std::vector<double> Genrouk;
	std::vector<double> sizeGenrou;
	//for Genrou
	for (int ii = 0; ii < 28; ++ii)
	{
		for (size_t kk = 0; kk < Genroudata.size(); ++kk)
		{

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

			double inter;
			double inter2;
			for (int jk = 0; jk < arr.size(); jk++)
			{
				if (arr[jk][0] == genrouidx)
				{
					inter = arr[jk][7];
					inter2 = arr[jk][2];
				}
			}


			double vn = inter2;
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

			Genrouk.push_back(genrouinter[ii]);
		}
	}
	sizeGenrou.push_back(Genroudata.size());
	sizeGenrou.push_back(28);
	Genroud.push_back(encodedoubletou8(Genrouk));
	Genroud.push_back(encodedoubletou8(sizeGenrou));


	//dime->send_sysparam(Busd, PQd, PVd, lined, nbus, nline, Genroud,Fsd,Swd, "SE");
}

//compile idxvgs and varheader
void dimeCollector::total_idxvgs(std::vector<std::string> &nbusvolk, std::vector<std::string> &nlinepk, std::vector<std::string> &nbusfreqk, std::vector<std::string> &nbusthetak, std::vector<std::string> &nbusgenreactivek, std::vector<std::string> &nbusgenrealk, std::vector<std::string> &nbusloadreactivelk, std::vector<std::string> &nbusloadrealk, std::vector<std::string> &nsynomegaj, std::vector<std::string> &nsyndeltaj, std::vector<std::string> &nlineij, std::vector<std::string> &nlineqj, std::vector<std::string> &nexc, std::vector<std::string> &ne1d, std::vector<std::string> &ne2d, std::vector<std::string> &ne1q, std::vector<std::string> &ne2q )
{
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
		wrvarname.append(totalname[kk]);
		Varheader.append(wrvarname);

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


	nbusvolk = idxvgsencode(1, nbusvol);
	nlinepk = idxvgsencode(nbusvol + 1, linepend);
	nbusfreqk = idxvgsencode(linepend + 1, busfreqend);
	nbusthetak = idxvgsencode(busfreqend + 1, busthetaend);
	nbusgenreactivek = idxvgsencode(busfreqend + 1, busgenreactiveend);
	nbusgenrealk = idxvgsencode(busgenreactiveend + 1, busgenrealend);
	nbusloadreactivelk = idxvgsencode(busgenrealend + 1, busloadreactiveend);
    nbusloadrealk = idxvgsencode(busloadreactiveend, busloadrealend);
	nsynomegaj = idxvgsencode(busloadrealend + 1, synomegaend);
	nsyndeltaj = idxvgsencode(synomegaend + 1, syndeltaend);
	nlineij = idxvgsencode(syndeltaend + 1, LineIend);
	nlineqj = idxvgsencode(LineIend + 1, Lineqend);
	nexc = idxvgsencode(Lineqend + 1, excend);
    ne1d = idxvgsencode(excend + 1, syne1d);
    ne2d = idxvgsencode(syne1d + 1, syne2d);
ne1q = idxvgsencode(syne2d + 1, syne1q);
ne2q = idxvgsencode(syne1q + 1, syne2q);



#pragma endregion 
}

//orgnize how to send to clients
change_code dimeCollector::trigger(coreTime time)
{
	
	double t = time;
	if (t != 0)
		coutn += 1;
/*	Json::Value nbusvolk;
	Json::Value nlinepk;
	Json::Value nbusfreqk;
	Json::Value nbusthetak;
	Json::Value nbusgenreactivek;
	Json::Value nbusgenrealk;
	Json::Value nbusloadreactivelk;
	Json::Value nbusloadrealk;
	Json::Value nsynomegaj;
	Json::Value nsyndeltaj;
	Json::Value nlineij;
	Json::Value nlineqj;
	Json::Value nexc;
	Json::Value ne1d;
	Json::Value ne2d;
	Json::Value ne1q;
	Json::Value ne2q;
*/
	std::vector<std::string> nbusvolk;
	std::vector<std::string> nlinepk;
	std::vector<std::string> nbusfreqk;
	std::vector<std::string> nbusthetak;
	std::vector<std::string> nbusgenreactivek;
	std::vector<std::string> nbusgenrealk;
	std::vector<std::string> nbusloadreactivelk;
	std::vector<std::string> nbusloadrealk;
	std::vector<std::string> nsynomegaj;
	std::vector<std::string> nsyndeltaj;
	std::vector<std::string> nlineij;
	std::vector<std::string> nlineqj;
	std::vector<std::string> nexc;
	std::vector<std::string> ne1d;
	std::vector<std::string> ne2d;
	std::vector<std::string> ne1q;
	std::vector<std::string> ne2q;

	if (!dime)
	{
		dime = std::make_unique<dimeClientInterface>(processName, server);
		dime->init();
	}
	auto out=collector::trigger(time);
	//figure out what to do with the data

	encodesysparam(Busdata2, Loaddata2, Generatordata2, Branchdata2, Transformerdata2, Genroudata2, Fixshuntdata2, sysname2, Baseinfor2);
	total_idxvgs(nbusvolk, nlinepk, nbusfreqk, nbusthetak, nbusgenreactivek, nbusgenrealk, nbusloadreactivelk, nbusloadrealk, nsynomegaj, nsyndeltaj, nlineij, nlineqj, nexc, ne1d, ne2d, ne1q, ne2q);




	if (t == 0)
	{
	  dev_list = dime->get_devices();
	  std::vector<std::string>::iterator deb = dev_list.begin();
	  std::vector<std::string>::iterator dee = dev_list.end();
	  for ( ;deb!=dee;deb++)
	  {
		  if (*deb == "griddyn")
		  {
			  deb=dev_list.erase(deb);
			  break;
		  }
		  
	  }
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
				//Json::Value reqvar;


				//prepare data json 2 encode method
				/*
				for (int jj = 0; jj < idxreq[ii].size(); ++jj)
				{

					Json::Value inter;
					inter.append(total[idxreq[ii][jj]-1]);
					reqvar.append(inter);

					Json::Value interh;
					interh.append(totalname[idxreq[ii][jj]-1]);
					reqvarheader.append(inter);

				}
				//*/
				//prepare data base64 method;
				std::vector<double> reqvar;
				for (int jj = 0; jj < idxreq[ii].size(); ++jj)
				{

					
					reqvar.push_back(total[idxreq[ii][jj]-1]);
					

					Json::Value interh;
					interh.append(totalname[idxreq[ii][jj]-1]);
					reqvarheader.append(interh);

				}
				std::string reqvars= encodedoubletou8(reqvar);
				if (t == 0)
				{
					dime->send_sysparam(Busd, PQd, PVd, lined, nbus, nline, Genroud, Fsd, Swd, devname[ii]);
					std::cout << "requested param is sended" << std::endl;
					std::cout << "requested var is started to be sended" << std::endl;
				}
				//send sysparam



				//dime->send_reqvar(coutn,t, reqvar, reqvarheader, devname[ii]);
				std::vector<double> shapeforreq;
				
				shapeforreq.push_back(idxreq[ii].size());
				shapeforreq.push_back(1);

				dime->send_reqvar(coutn, t, reqvars,shapeforreq, reqvarheader, devname[ii]);

			

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


