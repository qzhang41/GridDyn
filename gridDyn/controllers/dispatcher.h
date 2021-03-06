/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil;  eval: (c-set-offset 'innamespace 0); -*- */
/*
 * LLNS Copyright Start
 * Copyright (c) 2016, Lawrence Livermore National Security
 * This work was performed under the auspices of the U.S. Department
 * of Energy by Lawrence Livermore National Laboratory in part under
 * Contract W-7405-Eng-48 and in part under Contract DE-AC52-07NA27344.
 * Produced at the Lawrence Livermore National Laboratory.
 * All rights reserved.
 * For details, see the LICENSE file.
 * LLNS Copyright End
 */

#ifndef DISPATCHER_H_
#define DISPATCHER_H_

#include "basicDefs.h"

#include "submodels/gridControlBlocks.h"

#include <vector>

class gridArea;
class scheduler;


class dispatcher : public gridCoreObject
{
public:
protected:
  double totalDispatch;
  double capacity;
  double period;
  double dispatchTime;
  unsigned int schedCount;

  std::vector<scheduler *> schedList;

public:
  dispatcher (const std::string &objName = "dispatcher_#");

  virtual ~dispatcher ();
  virtual gridCoreObject * clone (gridCoreObject *obj = nullptr) const override;
  void moveSchedulers (dispatcher *dis);
  virtual double initialize (double time0,double dispatch);


  void setTime (double time) override;
  virtual double updateP (double time,double required,double targetTime);
  virtual double testP (double time,double required,double targetTime);
  double currentValue ()
  {
    return totalDispatch;
  }

  virtual int add (gridCoreObject *obj) override;
  virtual int add (scheduler *sched);
  virtual int remove (gridCoreObject *obj) override;
  virtual int remove (scheduler *sched);

  virtual int set (const std::string &param, const std::string &val) override;
  virtual int set (const std::string &param, double val, gridUnits::units_t unitType = gridUnits::defUnit) override;

  virtual void checkGen ();
protected:
  virtual void dispatch (double level);
};


#endif
