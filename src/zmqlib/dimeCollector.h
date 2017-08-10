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

#ifndef DIME_COLLECTOR_HEADER_
#define DIME_COLLECTOR_HEADER_

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#include "json/json.h"
#pragma warning(pop)
#else
#include "json/json.h"
#endif

#include "Link.h"
#include "measurement/collector.h"

class dimeClientInterface;

namespace griddyn
{
namespace dimeLib
{
class dimeCollector :public collector
{
private:
	std::string server;
	std::string processName;
	std::unique_ptr<dimeClientInterface> dime;
public:
	dimeCollector(coreTime time0 = timeZero, coreTime period = timeOneSecond);
	explicit dimeCollector(const std::string &name);
	~dimeCollector();

	virtual std::shared_ptr<collector> clone(std::shared_ptr<collector> gr = nullptr) const override;

	void sendinfo(std::vector<Link*> linkinfo, std::vector<gridBus*> businfo);

	void sendrawinfo(std::vector<std::string> Busdata, std::vector<std::string> Loaddata, std::vector<std::string> Generatordata, std::vector<std::string> branchdata, std::vector<std::string> transformerdata,std::vector<std::string> Fixshuntdata, std::vector <std::string> sysname, std::vector<int> Baseinfor);

	void senddyninfo(std::vector<std::vector<double>> Genroudata);

	void total_idxvgs(Json::Value & nbusvolk, Json::Value & nlinepk, Json::Value & nbusfreqk, Json::Value & nbusthetak, Json::Value & nbusgenreactivek, Json::Value & nbusgenrealk, Json::Value & nbusloadreactivelk, Json::Value & nbusloadrealk, Json::Value & nsynomegaj, Json::Value & nsyndeltaj, Json::Value & nlineij, Json::Value & nlineqj, Json::Value & nexc, Json::Value & ne1d, Json::Value & ne2d, Json::Value & ne1q, Json::Value & ne2q);



	virtual change_code trigger(coreTime time) override;

	//void sendsysparam(std::vector<std::string> Busdata, std::vector<std::string> Loaddata, std::vector<std::string> Generatordata, std::vector<std::string> branchdata, std::vector<std::string> transformerdata,std::vector<int> Baseinfor);

	void encodesysparam(std::vector<std::string> Busdata, std::vector<std::string> Loaddata, std::vector<std::string> Generatordata, std::vector<std::string> Branchdata, std::vector<std::string> Transformerdata, std::vector<std::vector<double>> Genroudata, std::vector<std::string> Fixshuntdata, std::vector<std::string> sysname, std::vector<int> Baseinfor);

	void set(const std::string &param, double val) override;
	void set(const std::string &param, const std::string &val) override;

	virtual const std::string &getSinkName() const override;
	

	
};

}//namespace dimeLib
}//namespace griddyn
#endif