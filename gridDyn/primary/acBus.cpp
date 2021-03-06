/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil;  eval: (c-set-offset 'innamespace 0); -*- */
/*
* LLNS Copyright Start
* Copyright (c) 2014, Lawrence Livermore National Security
* This work was performed under the auspices of the U.S. Department
* of Energy by Lawrence Livermore National Laboratory in part under
* Contract W-7405-Eng-48 and in part under Contract DE-AC52-07NA27344.
* Produced at the Lawrence Livermore National Laboratory.
* All rights reserved.
* For details, see the LICENSE file.
* LLNS Copyright End
*/

// headers
#include "gridArea.h"

#include "generators/gridDynGenerator.h"
#include "loadModels/gridLoad.h"
#include "linkModels/gridLink.h"
#include "gridCoreTemplates.h"

#include "acBus.h"
#include "objectFactoryTemplates.h"
#include "vectorOps.hpp"
#include "submodels/gridControlBlocks.h"
#include "simulation/contingency.h"
//#include "arrayDataSparse.h"
#include "stringOps.h"


#include <iostream>
#include <cmath>
#include <memory>
#include <cassert>


//factory is for the cloning function
static childTypeFactory<acBus, gridBus> gbfac ("bus", stringVec { "psystem" });

using namespace gridUnits;

acBus::acBus (const std::string &objName) : gridBus (objName),busController(this)
{
  // default values

}

acBus::acBus (double vStart, double angleStart, const std::string &objName) : gridBus (vStart,angleStart,objName), aTarget (angleStart), vTarget (vStart),busController(this)
{
  // default values
}

gridCoreObject *acBus::clone (gridCoreObject *obj) const
{
  acBus *nobj = cloneBaseFactory<acBus, gridBus> (this, obj, &gbfac);
  if (nobj == nullptr)
    {
      return obj;
    }

  nobj->vTarget = vTarget;

  nobj->Vmin = Vmin;
  nobj->Vmax = Vmax;
  nobj->prevType = prevType;
  nobj->freq = freq;
  nobj->prevPower = prevPower;
  nobj->participation = participation;
  nobj->Tw = Tw;


  nobj->busController.autogenP = busController.autogenP;
  nobj->busController.autogenQ = busController.autogenQ;
  nobj->busController.autogenDelay = busController.autogenDelay;

  if (opFlags[compute_frequency])
    {
      nobj->fblock.reset (static_cast<basicBlock *> (fblock->clone (nullptr)));
    }
  return nobj;
}

bool acBus::checkCapable ()
{
  double tP = 0;
  if (!opFlags[pFlow_initialized])
    {
      return true;
    }
  for (auto &load : attachedLoads)
    {
      if (load->enabled)
        {
          tP -= load->getRealPower ();
        }
    }
  for (auto &gen : attachedGens)
    {
      if (gen->enabled)
        {
          tP += gen->getPmax ();
        }
    }
  for (auto &link : attachedLinks)
    {
      if (link->enabled)
        {
          tP += link->getMaxTransfer ();
        }
    }
  if (tP <= 0)
    {
      LOG_WARNING ("BUS failed");
    }
  return ((tP < 0) ? false : true);
}

void acBus::disable ()
{
  enabled = false;
  alert (this, STATE_COUNT_CHANGE);
  for (auto &link : attachedLinks)
    {
      link->disable ();
    }
}

// destructor
acBus::~acBus ()
{

}

int acBus::add (gridCoreObject *obj)
{
  gridLoad *ld = dynamic_cast<gridLoad *> (obj);
  if (ld)
    {
      return gridBus::add (ld);
    }

  gridDynGenerator *gen = dynamic_cast<gridDynGenerator *> (obj);
  if (gen)
    {
      return gridBus::add (gen);
    }

  gridLink *lnk = dynamic_cast<gridLink *> (obj);
  if (lnk)
    {
      return gridBus::add (lnk);
    }
  acBus *bus = dynamic_cast<acBus *> (obj);
  if (bus)
    {
      return add (bus);
    }
  return (OBJECT_NOT_RECOGNIZED);
}



int acBus::add (acBus *bus)
{
  bus->busController.directBus = this;
  bus->opFlags.set (directconnect);
  if (getID () > bus->getID ())
    {
      bus->makeNewOID ();                   //update the ID to make it higher
    }
  mergeBus (bus);            //load into the merge controls
  return parent->add (bus);            //now add the bus to the parent object since buses can't directly contain other buses
}
int acBus::remove (gridCoreObject *obj)
{
  gridLoad *ld = dynamic_cast<gridLoad *> (obj);
  if (ld)
    {
      return (remove (ld));
    }

  gridDynGenerator *gen = dynamic_cast<gridDynGenerator *> (obj);
  if (gen)
    {
      return(remove (gen));
    }

  gridLink *lnk = dynamic_cast<gridLink *> (obj);
  if (lnk)
    {
      return(remove (lnk));
    }
  acBus *bus = dynamic_cast<acBus *> (obj);
  if (bus)
    {
      return (remove (bus));
    }
  return OBJECT_NOT_RECOGNIZED;
}

int acBus::remove (acBus *bus)
{
  if (bus->busController.masterBus->getID () == getID ())
    {
      if (bus->checkFlag (directconnect))
        {
          bus->opFlags.reset (directconnect);
          bus->busController.directBus = nullptr;
        }
      unmergeBus (bus);
      return OBJECT_REMOVE_SUCCESS;
    }
  return OBJECT_NOT_FOUND;
}


void acBus::alert (gridCoreObject *obj, int code)
{
  switch (code)
    {
    case VOLTAGE_CONTROL_UPDATE:
      if (opFlags[pFlow_initialized])
        {
          busController.updateVoltageControls ();
        }
      break;
    case VERY_LOW_VOLTAGE_ALERT:
      //set an internal flag
      opFlags.set (prev_low_voltage_alert);
      //forward the alert
      if (parent)
        {
          parent->alert (obj, code);
        }
      break;
    case POWER_CONTROL_UDPATE:
      if (opFlags[pFlow_initialized])
        {
          busController.updatePowerControls ();
        }
      break;
    case PV_CONTROL_UDPATE:
      if (opFlags[pFlow_initialized])
        {
          busController.updateVoltageControls ();
          busController.updatePowerControls ();
        }
      break;
    case OBJECT_NAME_CHANGE:
    case OBJECT_ID_CHANGE:
      break;
    case POTENTIAL_FAULT_CHANGE:
      if (opFlags[disconnected])
        {
          reconnect ();
        }
    //fall through to the primary alert;
    default:
      gridPrimary::alert (obj, code);
    }
}

// initializeB states
void acBus::pFlowObjectInitializeA (double time0, unsigned long flags)
{
  if (Vtol < 0)
    {
      Vtol = find ("root")->get ("voltagetolerance");
    }
  if (Atol < 0)
    {
      Atol = find ("root")->get ("angletolerance");
    }
  //run the subObjects

  int activeSecondary = 0;

  for (auto &gen : attachedGens)
    {
      gen->pFlowInitializeA (time0, flags);
      if (gen->isConnected ())
        {
          ++activeSecondary;
        }
    }
  for (auto &load : attachedLoads)
    {
      load->pFlowInitializeA (time0, flags);
      if (load->isConnected ())
        {
          ++activeSecondary;
        }
    }
  if (!(opFlags[use_autogen]))
    {
      if (CHECK_CONTROLFLAG (flags, auto_bus_disconnect))
        {
          int activeLink = 0;
          for (auto lnk : attachedLinks)
            {
              if (lnk->isConnected ())
                {
                  ++activeLink;
                }
            }

          if (activeSecondary == 0)
            {
              if (activeLink < 2)
                {
                  LOG_WARNING ("No load no gen, 1 active line ,bus is irrelevant disconnecting");
                  disconnect ();
                }
            }
        }
    }
  //load up control objects
  if (type == busType::PV)
    {
      if (busController.vControlObjects.empty ())
        {
          LOG_DEBUG ("PV BUS with no controllers: converting to PQ");
          type = busType::PQ;
        }
    }

  if ((type == busType::PV) || (type == busType::SLK))
    {
      voltage = vTarget;
    }
  if ((type == busType::SLK) || (type == busType::afix))
    {
      angle = aTarget;
      bool Padj = opFlags[use_autogen];

      if (!busController.pControlObjects.empty ())
        {
          Padj = true;                                                 //We have a P control object
        }

      if (!Padj)
        {                 //if there is no generator listed on SLK or afix bus we need one for accounting purposes so add a default one
          if (!CHECK_CONTROLFLAG (flags, no_auto_autogen))
            {
              LOG_WARNING ("No adjustable power elements, enabling auto_gen feature");
              opFlags.set (use_autogen);
            }
        }
    }
  else
    {
      if (CHECK_CONTROLFLAG (flags, auto_bus_disconnect))
        {
          if ((attachedGens.empty ()) && (attachedLoads.empty ()) && (attachedLinks.size () == 1))
            {
              if (opFlags[use_autogen] == false)
                {

                  LOG_WARNING ("No load no gen, 1 line ,bus is irrelevant disabling");
                  disconnect ();
                  return;
                }
            }
        }
    }

  //if there is only a single control then forward the bus max and mins to the control objects

  if ((type == busType::PV) || (type == busType::SLK))
    {
      if (busController.vControlObjects.size () == 1)
        {
          double temp;
          if (busController.Qmax < kHalfBigNum)
            {
              temp = busController.vControlObjects[0]->get ("qmax");
              if (temp > kHalfBigNum)
                {
                  busController.vControlObjects[0]->set ("qmax", busController.Qmax);
                }
            }
          if (busController.Qmin > -kHalfBigNum)
            {
              temp = busController.vControlObjects[0]->get ("qmin");
              if (temp < -kHalfBigNum)
                {
                  busController.vControlObjects[0]->set ("qmin", busController.Qmin);
                }
            }
        }
    }
  if ((type == busType::afix) || (type == busType::SLK))
    {
      if (busController.pControlObjects.size () == 1)
        {
          double temp;
          if (busController.Pmax < kHalfBigNum)
            {
              temp = busController.pControlObjects[0]->get ("pmax");
              if (temp > kHalfBigNum)
                {
                  busController.pControlObjects[0]->set ("pmax", busController.Pmax);
                }
            }
          if (busController.Pmin > -kHalfBigNum)
            {
              temp = busController.pControlObjects[0]->get ("pmin");
              if (temp < -kHalfBigNum)
                {
                  busController.pControlObjects[0]->set ("qmin", busController.Pmin);
                }
            }
        }
    }

  //update the controls
  if ((type == busType::PV) || (type == busType::SLK))
    {
      busController.updateVoltageControls ();
    }
  if ((type == busType::afix) || (type == busType::SLK))
    {
      busController.updatePowerControls ();
    }

  if (CHECK_CONTROLFLAG(flags, low_voltage_checking))
  {
      opFlags.set(low_voltage_check_flag);
  }
  updateFlags ();
}

void acBus::pFlowObjectInitializeB ()
{
  gridBus::pFlowObjectInitializeB ();

  m_dstate_dt.resize (3, 0);
  m_dstate_dt[angleInLocation] = m_baseFreq * (freq - 1.0);
  m_state = { voltage,angle,freq };
  outputs[voltageInLocation] = voltage;
  outputs[angleInLocation] = angle;
  outputs[frequencyInLocation] = freq;
  lastSetTime = prevTime;
  computePowerAdjustments ();
  if (opFlags[use_autogen])
    {
      if (busController.autogenP < kHalfBigNum)
        {
          busController.autogenPact = -(S.loadP + S.genP - busController.autogenP);
          LOG_TRACE ("autogen P=" + std::to_string (busController.autogenPact));
        }
      if (busController.autogenQ < kHalfBigNum)
        {
          busController.autogenQact = -(S.loadQ + S.genQ - busController.autogenQ);
        }
    }
}

//TODO:: transfer these functions to the busController
void acBus::mergeBus (gridBus *mbus)
{
  auto acmbus = dynamic_cast<acBus *> (mbus);
  if (acmbus == nullptr)
    {
      return;
    }
  //bus with the lowest ID is the master
  if (getID () < mbus->getID ())
    {
      if (opFlags[slave_bus])                      //if we are already a slave forward the merge to the master
        {
          busController.masterBus->mergeBus (mbus);
        }
      else
        {
          if (mbus->checkFlag (slave_bus))
            {
              if (getID () != acmbus->busController.masterBus->getID ())
                {
                  mergeBus (acmbus->busController.masterBus);
                }
            }
          else
            {
              //This bus becomes the master of mbus
              acmbus->busController.masterBus = this;
              acmbus->opFlags.set (slave_bus);
              busController.slaveBusses.push_back (acmbus);
              for (auto sb : acmbus->busController.slaveBusses)
                {
                  busController.slaveBusses.push_back (sb);
                  sb->busController.masterBus = this;
                }
              acmbus->busController.slaveBusses.clear ();
            }
        }
    }
  else if (getID () > mbus->getID ())         // mbus is now this buses master
    {
      if (opFlags[slave_bus])                      //if we are already a slave forward the merge to the master
        {
          if (busController.masterBus->getID () != mbus->getID ())
            {
              busController.masterBus->mergeBus (mbus);
            }
        }
      else                     //we were a master now mbus is the master
        {
          if (busController.slaveBusses.empty ())                           //no slave buses
            {
              busController.masterBus = mbus;
              acmbus->busController.slaveBusses.push_back (this);
            }
          else
            {
              if (mbus->checkFlag (slave_bus))
                {
                  acmbus->busController.masterBus->mergeBus (this);
                }
              else
                {
                  busController.masterBus = mbus;
                  acmbus->busController.slaveBusses.push_back (this);
                  for (auto sb : busController.slaveBusses)
                    {
                      acmbus->busController.slaveBusses.push_back (sb);
                      sb->busController.masterBus = mbus;
                    }
                  busController.slaveBusses.clear ();
                }
            }
        }


    }
}


void acBus::unmergeBus (gridBus *mbus)
{
  auto acmbus = dynamic_cast<acBus *> (mbus);
  if (acmbus == nullptr)
    {
      return;
    }
  if (opFlags[slave_bus])
    {
      if (mbus->checkFlag (slave_bus))
        {
          if (acmbus->busController.masterBus->getID () == busController.masterBus->getID ())
            {
              busController.masterBus->unmergeBus (mbus);
            }
        }
      else if (busController.masterBus->getID () == mbus->getID ())
        {
          mbus->unmergeBus (this);                           //flip it around so this bus is unmerged from mbus
        }
    }
  else            //in the masterbus
    {
      if ((mbus->checkFlag (slave_bus)) && (getID () == acmbus->busController.masterBus->getID ()))
        {
          for (auto &eb : busController.slaveBusses)
            {
              eb->opFlags.reset (slave_bus);
            }
          checkMerge ();
          mbus->checkMerge ();
        }
    }
}

void acBus::checkMerge ()
{
  if (!enabled)
    {
      return;
    }
  if (opFlags[directconnect])
    {
      busController.directBus->mergeBus (this);
    }
  for (auto &lnk : attachedLinks)
    {
      lnk->checkMerge ();
    }
}

//function to reset the bus type and voltage

void acBus::reset (reset_levels level)
{

  gridBus::reset (level);
  oCount = 0;
  if (prevType != type)
    {
      type = prevType;
      alert (this, JAC_COUNT_CHANGE);
    }
  switch (level)
    {
    case reset_levels::minimal:
      break;
    case reset_levels::full:
    case reset_levels::voltage_angle:
      if ((type == busType::PV) || (type == busType::SLK))
        {
          voltage = vTarget;
        }
      else
        {
          voltage = 1.0;
        }

      if ((type == busType::SLK) || (type == busType::afix))
        {
          angle = aTarget;
        }
      else
        {
          angle = 0.0;
        }

      break;
    case reset_levels::voltage:
      if ((type == busType::PV) || (type == busType::SLK))
        {
          voltage = vTarget;
        }
      else
        {
          voltage = 1.0;
        }
      break;
    case reset_levels::angle:
      if ((type == busType::SLK) || (type == busType::afix))
        {
          angle = aTarget;
        }
      else
        {
          angle = 0.0;
        }
      break;
    case reset_levels::low_voltage_pflow:
      if (voltage < 0.6)
        {
          voltage = 0.9;
          angle = getAverageAngle ();
        }
      break;
    case reset_levels::low_voltage_dyn0:
      if (prevDynType != dynType)
        {
          dynType = prevDynType;
          double nAngle = static_cast<gridArea *> (parent)->getMasterAngle (nullptr, cLocalSolverMode);
          angle = angle + (nAngle - refAngle);
          alert (this, JAC_COUNT_CHANGE);
        }
      else if (voltage < 0.1)
        {
          voltage = 1.0;
          angle = getAverageAngle ();
        }
      break;
    case reset_levels::low_voltage_dyn1:
      if (prevDynType != dynType)
        {
          dynType = prevDynType;
          double nAngle = static_cast<gridArea *> (parent)->getMasterAngle (nullptr, cLocalSolverMode);
          angle = angle + (nAngle - refAngle);
          alert (this, JAC_COUNT_CHANGE);
        }
      if (!attachedGens.empty ())
        {

        }
      else if (voltage < 0.5)
        {
          voltage = 0.7;
          angle = getAverageAngle ();
        }
      break;
    case reset_levels::low_voltage_dyn2:
      if (prevDynType != dynType)
        {
          dynType = prevDynType;
          double nAngle = static_cast<gridArea *> (parent)->getMasterAngle (nullptr, cLocalSolverMode);
          angle = angle + (nAngle - refAngle);
          alert (this, JAC_COUNT_CHANGE);
        }
      if (!attachedGens.empty ())
        {
          if (voltage < 0.5)
            {
              voltage = 0.7;
              for (auto &gen : attachedGens)
                {
                  gen->algebraicUpdate ({ voltage, angle }, nullptr, nullptr, cLocalSolverMode, 1.0);
                }
            }
        }
      else if (voltage < 0.6)
        {
          voltage = 0.9;
          angle = getAverageAngle ();

        }
      break;
    }

}

double acBus::getAverageAngle ()
{
  if (!attachedLinks.empty ())
    {
      double a = 0.0;
      double rel = 0.0;
      for (auto &lnk : attachedLinks)
        {
          a += lnk->getAbsAngle (getID ());
          rel += 1.0;
        }
      if (rel > 0.9)
        {
          return (a / rel);
        }
    }
  return angle;
}

change_code acBus::powerFlowAdjust (unsigned long flags, check_level_t level)
{


  auto out = change_code::no_change;
  if (level == check_level_t::low_voltage_check)
    {
      if (isConnected () == false)
        {
          return out;
        }
      if (voltage < 1e-8)
        {
          disconnect ();
          out = change_code::jacobian_change;
        }
      if (opFlags[prev_low_voltage_alert])
        {
          disconnect ();
          opFlags.reset (prev_low_voltage_alert);
          out = change_code::jacobian_change;
        }
      return out;
    }


  if (!CHECK_CONTROLFLAG (flags, ignore_bus_limits))
    {
      computePowerAdjustments ();
      S.genQ = S.sumQ ();
      S.genP = S.sumP ();

      switch (type)
        {
        case busType::SLK:

          if (S.genQ < busController.Qmin)
            {
              S.genQ = busController.Qmin;
              for (auto &vco : busController.vControlObjects)
                {
                  vco->set ("q", "min");
                }
              type = busType::afix;
              alert (this, JAC_COUNT_CHANGE);
              out = change_code::jacobian_change;
            }
          else if (S.genQ > busController.Qmax)
            {
              S.genQ = busController.Qmax;
              for (auto &vco : busController.vControlObjects)
                {
                  vco->set ("q", "max");
                }
              type = busType::afix;
              alert (this, JAC_COUNT_CHANGE);
              out = change_code::jacobian_change;
            }

          break;
        case busType::PQ:
          if (prevType == busType::PV)
            {

              if (std::abs (S.genQ - busController.Qmin) < 0.00001)
                {
                  if (voltage < vTarget)
                    {
                      if (oCount < 5)
                        {

                          voltage = vTarget;
                          type = busType::PV;
                          oCount++;
                          alert (this, JAC_COUNT_CHANGE);
                          out = change_code::jacobian_change;
                          LOG_TRACE ("changing from PQ to PV from low voltage");
                        }
                    }
                }
              else
                {
                  if (voltage > vTarget)
                    {
                      if (oCount < 5)
                        {
                          voltage = vTarget;
                          type = busType::PV;
                          oCount++;
                          alert (this, JAC_COUNT_CHANGE);
                          out = change_code::jacobian_change;
                          LOG_TRACE ("changing from PQ to PV from high voltage");
                        }
                    }
                }
            }
          else if (prevType == busType::SLK)
            {
              if (std::abs (S.genQ - busController.Qmin) < 0.00001)
                {
                  if (voltage < vTarget)
                    {
                      if (oCount < 5)
                        {

                          voltage = vTarget;
                          type = busType::SLK;
                          oCount++;
                          alert (this, JAC_COUNT_CHANGE);
                          out = change_code::jacobian_change;
                        }
                    }
                }
              else
                {
                  if (voltage > vTarget)
                    {
                      if (oCount < 5)
                        {
                          voltage = vTarget;
                          type = busType::SLK;
                          oCount++;
                          alert (this, JAC_COUNT_CHANGE);
                          out = change_code::jacobian_change;
                        }
                    }
                }
            }


          break;
        case busType::PV:
          if (S.genQ < busController.Qmin)
            {
              S.genQ = busController.Qmin;
              for (auto &vco : busController.vControlObjects)
                {
                  vco->set ("q", "min");
                }
              type = busType::PQ;
              alert (this, JAC_COUNT_CHANGE);
              out = change_code::jacobian_change;
              LOG_TRACE ("changing from PV to PQ from Qmin");
            }
          else if (S.genQ > busController.Qmax)
            {
              S.genQ = busController.Qmax;
              for (auto &vco : busController.vControlObjects)
                {
                  vco->set ("q", "max");
                }
              type = busType::PQ;
              alert (this, JAC_COUNT_CHANGE);
              out = change_code::jacobian_change;
              LOG_TRACE ("changing from PV to PQ from Qmax");
            }
          break;
        case busType::afix:
          if (prevType == busType::SLK)
            {
              if (std::abs (S.genQ - busController.Qmin) < 0.00001)
                {
                  if (voltage < vTarget)
                    {
                      if (oCount < 5)
                        {

                          voltage = vTarget;
                          type = busType::SLK;
                          oCount++;
                          alert (this, JAC_COUNT_CHANGE);
                          out = change_code::jacobian_change;
                        }
                    }
                }
              else
                {
                  if (voltage > vTarget)
                    {
                      if (oCount < 5)
                        {
                          voltage = vTarget;
                          type = busType::SLK;
                          oCount++;
                          alert (this, JAC_COUNT_CHANGE);
                          out = change_code::jacobian_change;
                        }
                    }
                }
            }

          if (S.genP < busController.Pmin)
            {
              S.genP = busController.Pmin;
              for (auto &pco : busController.pControlObjects)
                {
                  pco->set ("p", "min");
                }
              type = busType::PQ;
              alert (this, JAC_COUNT_CHANGE);
              out = change_code::jacobian_change;
              if (prevType == busType::SLK)
                {
                  alert (this, SLACK_BUS_CHANGE);
                }
            }
          else if (S.genP > busController.Pmax)
            {
              S.genP = busController.Pmax;
              type = busType::PQ;
              for (auto &pco : busController.pControlObjects)
                {
                  pco->set ("p", "max");
                }
              alert (this, JAC_COUNT_CHANGE);
              out = change_code::jacobian_change;
              if (prevType == busType::SLK)
                {
                  alert (this, SLACK_BUS_CHANGE);
                }

            }

        }
      updateLocalCache ();
    }
  change_code pout;
  for (auto &gen : attachedGens)
    {
      if (gen->checkFlag (has_powerflow_adjustments))
        {
          pout = gen->powerFlowAdjust ({ voltage, angle }, flags, level);
          out = (std::max)(pout, out);
        }
    }
  for (auto &ld : attachedLoads)
    {
      if (ld->checkFlag (has_powerflow_adjustments))
        {
          pout = ld->powerFlowAdjust ({ voltage, angle }, flags, level);
          out = (std::max)(pout, out);
        }
    }
  return out;

}
/*function to check the currect status for any limit violations*/
void acBus::pFlowCheck (std::vector<violation> &Violation_vector)
{
  if (voltage > Vmax)
    {
      violation V;
      V.violationCode = VOLTAGE_OVER_LIMIT_VIOLATION;
      V.level = voltage;
      V.limit = Vmax;
      V.m_objectName = name;
      V.percentViolation = (voltage - Vmax) * 100;                     //assumes nominal voltage level at 1.0;
      Violation_vector.push_back (V);
    }
  else if (voltage < Vmin)
    {
      violation V;
      V.violationCode = VOLTAGE_UNDER_LIMIT_VIOLATION;
      V.level = voltage;
      V.limit = Vmin;
      V.m_objectName = name;
      V.percentViolation = (Vmin - voltage) * 100;                     //assumes nominal voltage level at 1.0;
      Violation_vector.push_back (V);
    }
}

// initializeB states for dynamic solution
void acBus::dynObjectInitializeA (double time0, unsigned long flags)
{
  gridBus::dynObjectInitializeA (time0, flags);
  //find a
  if (!(attachedGens.empty ()))
    {
      double mxpower = 0;
      keyGen = nullptr;
      for (auto &gen : attachedGens)
        {
          if (gen->isConnected ())
            {
              if (gen->checkFlag (gridDynGenerator::generator_flags::internal_frequency_calculation))
                {
                  if (gen->getPmax () > mxpower)
                    {
                      keyGen = gen;
                      mxpower = gen->getPmax ();
                    }
                }
            }
        }
    }
  if (opFlags[uses_bus_frequency])
    {
      if (attachedGens.empty ())
        {
          opFlags.set (compute_frequency);
        }
      else if (!keyGen)
        {
          opFlags.set (compute_frequency);
        }
    }
  if (opFlags[compute_frequency])
    {
      LOG_TRACE ("computing bus frequency using frequency block");
      if (!fblock)
        {
          fblock = std::make_shared<derivativeBlock> (Tw);
          fblock->setName ("frequency_calc");
          fblock->set ("k", 1.0 / m_baseFreq);
        }
      fblock->initializeA (time0, flags);
    }
  lastSetTime = time0;
}

// initializeB states for dynamic solution part 2  //final clean up
void acBus::dynObjectInitializeB (IOdata &outputSet)
{
  if (outputSet.size () > 0)
    {
      if (outputSet[voltageInLocation] > 0)
        {
          voltage = outputSet[voltageInLocation];
        }
      if (outputSet[angleInLocation] > -kHalfBigNum)
        {
          angle = outputSet[angleInLocation];
        }
      if (std::abs (outputSet[frequencyInLocation] - 1.0) < 0.5)
        {
          freq = outputSet[frequencyInLocation];
        }
    }
  updateLocalCache ();
  lastSetTime = prevTime;
  m_state[voltageInLocation] = voltage;
  m_state[angleInLocation] = angle;
  m_state[frequencyInLocation] = freq;
  if (opFlags[use_autogen])
    {
      if ((busController.autogenQ > kHalfBigNum) && (attachedGens.empty ()))
        {
          busController.autogenQact = -(S.linkQ + S.loadQ);
        }
      S.genP = busController.autogenPact;
      S.genQ = busController.autogenQact;
    }
  //first get the state size for the internal state ordering
  auto args = getOutputs (nullptr, cLocalSolverMode);
  double Qgap, Pgap;
  int vci = 0, poi = 0;
  auto cid = getID ();
  switch (type)
    {
    case busType::PQ:
      break;
    case busType::PV:
      computePowerAdjustments ();
      Qgap = -S.sumQ ();
      for (auto &vco : busController.vControlObjects)
        {
          if (vco->checkFlag (local_voltage_control))
            {
              vco->set ("q", -Qgap * busController.vcfrac[vci]);

            }
          else
            {
              busController.proxyVControlObject[poi]->fixPower (busController.proxyVControlObject[poi]->getRealPower (cid), Qgap * busController.vcfrac[vci], cid, cid);
              ++poi;
            }
          ++vci;
        }
      break;
    case busType::SLK:

      computePowerAdjustments ();
      Qgap = -(S.sumQ ());
      Pgap = -(S.sumP ());

      if (opFlags[identical_PQ_control_objects])                                         //adjust the power levels together
        {
          for (auto &vco : busController.vControlObjects)
            {
              if (vco->checkFlag (local_voltage_control))
                {
                  vco->set ("q", -Qgap * busController.vcfrac[vci]);
                  vco->set ("p", -Pgap * busController.pcfrac[vci]);
                }
              else
                {                                                             //use both together on fixpower function
                  busController.proxyVControlObject[poi]->fixPower (-Pgap * busController.pcfrac[vci], -Qgap * busController.vcfrac[vci], cid, cid);
                  ++poi;
                }
              ++vci;
            }
        }
      else                                            //adjust the power levels seperately
        {
          //adjust the real power flow
          for (auto &pco : busController.pControlObjects)
            {
              if (pco->checkFlag (local_voltage_control))
                {
                  pco->set ("p", -Pgap * busController.pcfrac[vci]);

                }
              else
                {
                  busController.proxyVControlObject[poi]->fixPower (-Pgap * busController.pcfrac[vci], busController.proxyPControlObject[poi]->getReactivePower (cid), cid, cid);
                  ++poi;
                }
              ++vci;
            }
          //adjust the reactive power

          vci = 0;
          poi = 0;
          for (auto &vco : busController.vControlObjects)
            {
              if (vco->checkFlag (local_voltage_control))
                {
                  vco->set ("q", -Qgap * busController.vcfrac[vci]);

                }
              else
                {
                  busController.proxyVControlObject[poi]->fixPower (busController.proxyVControlObject[poi]->getRealPower (cid), -Qgap * busController.vcfrac[vci], cid, cid);
                  ++poi;
                }
              ++vci;
            }


        }
      break;
    case busType::afix:
      Pgap = -(S.sumP ());
      //adjust the real power flow
      for (auto &pco : busController.pControlObjects)
        {
          if (pco->checkFlag (local_voltage_control))
            {
              pco->set ("p", -Pgap * busController.pcfrac[vci]);

            }
          else
            {
              busController.proxyVControlObject[poi]->fixPower (-Pgap * busController.pcfrac[vci], busController.proxyPControlObject[poi]->getReactivePower (cid), cid, cid);
              ++poi;
            }
          ++vci;
        }
      break;
    }
  IOdata pc;
  for (auto &gen : attachedGens)
    {
      gen->dynInitializeB (args, pc);
    }
  for (auto &load : attachedLoads)
    {
      load->dynInitializeB (args, pc);

    }
  if (opFlags[compute_frequency])
    {
      IOdata iset (2);
      fblock->initializeB ({ angle }, { 0.0 }, iset);
    }
}

void acBus::powerAdjust (double adjustment)
{

  //adjust the real power flow
  int vci = 0;
  for (auto &pco : busController.pControlObjects)
    {                   //don't worry about proxy objects for this purpose
      pco->set ("adjustment", adjustment * busController.pcfrac[vci]);
      ++vci;
    }

}

double acBus::timestep (double ttime, const solverMode &sMode)
{
  double dt = ttime - prevTime;
  if (dt < 1.0)
    {
      voltage += m_dstate_dt[voltageInLocation] * dt;
      if (isDynamic (sMode))
        {
          angle += (freq - 1.0) * m_baseFreq * dt;
        }
    }
  IOdata args {
    voltage,angle,freq
  };
  for (auto &load : attachedLoads)
    {
      load->timestep (ttime, args, sMode);
    }
  for (auto &gen : attachedGens)
    {
      gen->timestep (ttime, args, sMode);
    }
  //localConverge (sMode, 0);
  //updateLocalCache ();
  if (opFlags[compute_frequency])
    {
      fblock->step (ttime, angle);
    }
  prevTime = ttime;
  return 0.0;
}


static const stringVec locNumStrings {
  "vtarget","atarget","p","q",
};
static const stringVec locStrStrings {
  "pflowtype", "dyntype"
};

static const stringVec flagStrings {
  "use_frequency"
};

void acBus::getParameterStrings (stringVec &pstr, paramStringType pstype) const
{
  getParamString<acBus, gridBus> (this, pstr, locNumStrings, locStrStrings, flagStrings, pstype);
}

int acBus::setFlag (const std::string &flag, bool val)
{
  int out = PARAMETER_FOUND;
  if (flag == "compute_frequency")
    {
      if (!opFlags[dyn_initialized])
        {
          opFlags.set (compute_frequency);
          if (!fblock)
            {
              fblock = std::make_shared<derivativeBlock> (Tw);
              fblock->setName ("frequency_calc");
              fblock->set ("k", 1.0 / m_baseFreq);
            }
        }
    }
  else
    {
      out = gridPrimary::setFlag (flag, val);
    }

  return out;
}

// set properties
int acBus::set (const std::string &param, const std::string &vali)
{
  int out = PARAMETER_FOUND;
  auto val = convertToLowerCase (vali);
  if ((param == "type") || (param == "bustype") || (param == "pflowtype"))
    {


      if ((val == "slk") || (val == "swing") || (val == "slack"))
        {
          type = busType::SLK;
          prevType = busType::SLK;
        }
      else if (val == "pv")
        {
          type = busType::PV;
          prevType = busType::PV;
        }
      else if (val == "pq")
        {
          type = busType::PQ;
          prevType = busType::PQ;
        }
      else if ((val == "dynslk") || (val == "inf") || (val == "infinite"))
        {
          type = busType::SLK;
          prevType = busType::SLK;
          dynType = dynBusType::dynSLK;
        }
      else if ((val == "fixedangle") || (val == "fixangle") || (val == "ref"))
        {
          dynType = dynBusType::fixAngle;
        }
      else if ((val == "fixedvoltage") || (val == "fixvoltage"))
        {
          dynType = dynBusType::fixVoltage;
        }
      else if (val == "afix")
        {
          type = busType::afix;
          prevType = busType::afix;
        }
      else if (val == "normal")
        {
          dynType = dynBusType::normal;
        }
      else
        {
          return INVALID_PARAMETER_VALUE;
        }

    }
  else if (param == "dyntype")
    {
      if ((val == "dynslk") || (val == "inf") || (val == "slk"))
        {
          dynType = dynBusType::dynSLK;
          type = busType::SLK;
        }
      else if ((val == "fixedangle") || (val == "fixangle") || (val == "ref"))
        {
          dynType = dynBusType::fixAngle;
        }
      else if ((val == "fixedvoltage") || (val == "fixvoltage"))
        {
          dynType = dynBusType::fixVoltage;
        }
      else if ((val == "normal") || (val == "pq"))
        {
          dynType = dynBusType::normal;
        }
      else
        {
          out = INVALID_PARAMETER_VALUE;
        }
    }
  else if (param == "status")
    {
      if ((val == "out") || (val == "off") || (val == "disconnected"))
        {
          if (enabled)
            {
              disable ();
            }

        }
      else if ((val == "in") || (val == "on"))
        {
          if (!enabled)
            {
              enable ();
            }
        }
      else if (val == "disconnected")
        {
          disconnect ();
        }
      else
        {
          out = INVALID_PARAMETER_VALUE;
        }
    }
  else
    {
      out = gridPrimary::set (param, vali);
    }
  return out;
}

int acBus::set (const std::string &param, double val, units_t unitType)
{
  int out = PARAMETER_FOUND;

  if ((param == "voltage") || (param == "vol"))
    {
      voltage = unitConversion (val, unitType, puV, systemBasePower, baseVoltage);
      if ((type == busType::PV) || (type == busType::SLK))
        {
          vTarget = voltage;
        }
    }
  else if ((param == "angle") || (param == "ang"))
    {
      angle = unitConversion (val, unitType, rad);
      if ((type == busType::SLK) || (type == busType::afix))
        {
          aTarget = angle;
        }
    }
  else if ((param == "basefrequency") || (param == "basefreq"))
    {
      m_baseFreq = unitConversionFreq (val, unitType, rps);

      for (auto &gen : attachedGens)
        {
          gen->set ("basefreq", m_baseFreq);
        }
      for (auto &ld : attachedLoads)
        {
          ld->set ("basefreq", m_baseFreq);
        }
      if (opFlags[compute_frequency])
        {
          fblock->set ("k", 1.0 / m_baseFreq);
        }
    }
  else if (param == "vtarget")
    {
      vTarget = unitConversion (val, unitType, puV, systemBasePower, baseVoltage);
      /*powerFlowAdjust the target in all the generators as well*/
      for (auto &gen : attachedGens)
        {
          gen->set (param, vTarget);
        }

    }
  else if (param == "atarget")
    {
      aTarget = unitConversion (val, unitType, rad);
    }
  else if (param == "qmax")
    {
      if (opFlags[pFlow_initialized])
        {
          if (busController.vControlObjects.size () == 1)
            {
              busController.vControlObjects[0]->set ("qmax", val, unitType);
            }
          else
            {
              out = PARAMETER_NOT_FOUND;
            }
        }
      else
        {
          busController.Qmax = unitConversion (val, unitType, puMW, systemBasePower, baseVoltage);
        }

    }
  else if (param == "qmin")
    {
      if (opFlags[pFlow_initialized])
        {

          if (busController.vControlObjects.size () == 1)
            {
              busController.vControlObjects[0]->set ("qmin", val, unitType);
            }
          else
            {
              out = PARAMETER_NOT_FOUND;
            }
        }
      else
        {
          busController.Qmin = unitConversion (val, unitType, puMW, systemBasePower, baseVoltage);
        }
    }
  else if (param == "pmax")
    {
      if (opFlags[pFlow_initialized])
        {
          if (busController.pControlObjects.size () == 1)
            {
              busController.pControlObjects[0]->set ("pmax", val, unitType);
            }
          else
            {
              out = PARAMETER_NOT_FOUND;
            }
        }
      else
        {
          busController.Pmax = unitConversion (val, unitType, puMW, systemBasePower, baseVoltage);
        }
    }
  else if (param == "pmin")
    {
      if (opFlags[pFlow_initialized])
        {

          if (busController.pControlObjects.size () == 1)
            {
              busController.pControlObjects[0]->set ("pmin", val, unitType);
            }
          else
            {
              out = PARAMETER_NOT_FOUND;
            }
        }
      else
        {
          busController.Pmin = unitConversion (val, unitType, puMW, systemBasePower, baseVoltage);
        }
    }
  else if (param == "vmax")
    {
      Vmax = val;
    }
  else if (param == "vmin")
    {
      Vmin = val;
    }
  else if (param == "autogenp")
    {

      busController.autogenP = unitConversion (val, unitType, puMW, systemBasePower, baseVoltage);
      opFlags.set (use_autogen);
    }
  else if (param == "autogenq")
    {
      busController.autogenQ = unitConversion (val, unitType, puMW, systemBasePower, baseVoltage);
      opFlags.set (use_autogen);
    }
  else if (param == "autogendelay")
    {
      busController.autogenDelay = val;
    }
  else if ((param == "voltagetolerance") || (param == "vtol"))
    {
      Vtol = val;
    }
  else if ((param == "angletolerance") || (param == "atol"))
    {
      Atol = val;
    }
  else if (param == "tw")
    {
      Tw = val;
      if (opFlags[compute_frequency])
        {
          fblock->set ("t1", Tw);
        }
    }
  else if (param == "lowvdisconnect")
    {
      if (voltage <= val)
        {
          disconnect ();
        }
    }
  else
    {
      out = gridBus::set (param, val, unitType);
    }


  return out;
}

void acBus::setVoltageAngle (double Vnew, double Anew)
{
  voltage = Vnew;
  angle = Anew;
  switch (type)
    {
    case busType::PQ:
      break;
    case busType::PV:
      vTarget = voltage;
      break;
    case busType::SLK:
      vTarget = voltage;
      aTarget = angle;
      break;
    case busType::afix:
      aTarget = angle;
      break;
	default:
		break;
    }
}

static const IOdata kNullVec;

IOdata acBus::getOutputs (const stateData *sD, const solverMode &sMode)
{
  if ((isLocal (sMode)) || (!sD))
    {
      return {
               voltage,angle,freq
      };
    }
  else
    {
      return {
               getVoltage (sD,sMode),getAngle (sD,sMode),getFreq (sD,sMode)
      };
    }
}

static const IOlocs kNullLocations {
  kNullLocation,kNullLocation,kNullLocation
};

IOlocs acBus::getOutputLocs (const solverMode &sMode) const
{
  if ((!hasAlgebraic (sMode))||(!isConnected()))
    {
      return kNullLocations;
    }
  if (sMode.offsetIndex == lastSmode)
    {
      return outLocs;
    }
  else
    {
      IOlocs newOutLocs (3);
     // auto Aoffset = useAngle(sMode) ? offsets.getAOffset(sMode) : kNullLocation;
     // auto Voffset = useVoltage(sMode) ? offsets.getVOffset(sMode) : kNullLocation;
	  auto Aoffset =  offsets.getAOffset(sMode);
	  auto Voffset =  offsets.getVOffset(sMode);
	  
      newOutLocs[voltageInLocation] = Voffset;
      newOutLocs[angleInLocation] = Aoffset;
      if (opFlags[compute_frequency])
        {
          index_t toff = kNullLocation;
          if (opFlags[compute_frequency])
            {
              toff=fblock->getOutputLoc (sMode);
            }
          else if (keyGen)
            {
              keyGen->getFreq (nullptr, sMode, &toff);
            }

          newOutLocs[frequencyInLocation] = toff;
        }
      else
        {
          newOutLocs[frequencyInLocation] = kNullLocation;
        }
      return newOutLocs;
    }
}

index_t acBus::getOutputLoc (const solverMode &sMode, index_t num) const
{
  if (sMode.offsetIndex == lastSmode)
    {
      if (num < 3)
        {
          return outLocs[num];
        }
      else
        {
          return kNullLocation;
        }
    }
  else
    {
      switch (num)
        {
        case voltageInLocation:
           // return useVoltage(sMode) ? offsets.getVOffset(sMode) : kNullLocation;
			return offsets.getVOffset(sMode);
        case angleInLocation:
          {
            //return useAngle(sMode) ? offsets.getAOffset(sMode) : kNullLocation;
			return offsets.getAOffset(sMode);
          }
          break;
        case frequencyInLocation:
          {
            if (opFlags[compute_frequency])
              {
                return fblock->getOutputLoc (sMode);
              }
            else if (keyGen)
              {
				index_t loc;
                keyGen->getFreq (nullptr, sMode, &loc);
				return loc;
              }
            else
              {
                return kNullLocation;
              }
          }
          break;
        default:
			return kNullLocation;
        }
    }
}

double acBus::getVoltage (const double state[], const solverMode &sMode) const
{
  if (isLocal (sMode))
    {
      return voltage;
    }
	//if (useVoltage(sMode))
	{
		auto Voffset = offsets.getVOffset(sMode);
		return (Voffset != kNullLocation) ? state[Voffset] : voltage;
	}
	//return voltage;
}

double acBus::getAngle (const double state[], const solverMode &sMode) const
{
  if (isLocal (sMode))
    {
      return angle;
    }
	//if (useAngle(sMode))
	{
		auto Aoffset = offsets.getAOffset(sMode);
		return (Aoffset != kNullLocation) ? state[Aoffset] : angle;
	}
	//return angle;
  
}

double acBus::getVoltage (const stateData *sD, const solverMode &sMode) const
{
  if (isLocal (sMode))
    {
      return voltage;
    }
	//if (useVoltage(sMode))
	{
		auto Voffset = offsets.getVOffset(sMode);
		return (Voffset != kNullLocation) ? sD->state[Voffset] : voltage;
	}
	//return voltage;
 
}

double acBus::getAngle (const stateData *sD, const solverMode &sMode) const
{
  if (isLocal (sMode))
    {
      return angle;
    }
//	if (useAngle(sMode))
	{
		auto Aoffset = offsets.getAOffset(sMode);
		return (Aoffset != kNullLocation) ? sD->state[Aoffset] : angle;
	}
//	return angle;
}



double acBus::getFreq (const stateData *sD, const solverMode &sMode) const
{
  double f = freq;
  if (opFlags[uses_bus_frequency])
    {
      if (isDynamic (sMode))
        {
          if (opFlags[compute_frequency])
            {
              f = fblock->getOutput (kNullVec, sD, sMode) + 1.0;
            }
          else if (keyGen)
            {
              f = keyGen->getFreq (sD, sMode);
            }
        }
    }
  return f;
}

int acBus::propogatePower (bool makeSlack)
{
  int ret = 0;
  if (makeSlack)
    {
      prevType = type;
      type = busType::SLK;
    }
  int unfixed_lines = 0;
  gridLink *unfixed_line = nullptr;
  double Pexp = 0;
  double Qexp = 0;
  for (auto &lnk : attachedLinks)
    {
      if (lnk->checkFlag (gridLink::fixed_target_power))
        {
          Pexp += lnk->getRealPower (getID ());
          Qexp += lnk->getReactivePower (getID ());
          continue;
        }
      ++unfixed_lines;
      unfixed_line = lnk;

    }
  if (unfixed_lines > 1)
    {
      return ret;
    }

  int adjPSecondary = 0;
  int adjQSecondary = 0;
  for (auto &ld : attachedLoads)
    {
      if (ld->checkFlag (adjustable_P))
        {
          ++adjPSecondary;
        }
      else
        {
          Pexp += ld->getRealPower ();
        }
      if (ld->checkFlag (adjustable_Q))
        {
          ++adjQSecondary;
        }
      else
        {
          Qexp += ld->getReactivePower ();
        }
    }
  for (auto &gen : attachedGens)
    {
      if (gen->checkFlag (adjustable_P))
        {
          ++adjPSecondary;
        }
      else
        {
          Pexp -= gen->getRealPower ();
        }
      if (gen->checkFlag (adjustable_Q))
        {
          ++adjQSecondary;
        }
      else
        {
          Qexp -= gen->getReactivePower ();
        }
    }
  if (unfixed_lines == 1)
    {
      if ((adjPSecondary == 0) && (adjQSecondary == 0))
        {
          ret = unfixed_line->fixPower (-Pexp, -Qexp, getID (), getID ());
        }
    }
  else               //no lines so adjust the generators and load
    {
      if ((adjPSecondary == 1) && (adjQSecondary == 1))
        {
          int found = 0;
          for (auto &gen : attachedGens)
            {
              if (gen->checkFlag (adjustable_P))
                {
                  gen->set ("p", Pexp);
                  ++found;
                }
              if (gen->checkFlag (adjustable_Q))
                {
                  gen->set ("q", Qexp);
                  ++found;
                }
              if (found == 2)
                {
                  return 1;
                }
            }
          for (auto &ld : attachedLoads)
            {
              if (ld->checkFlag (adjustable_P))
                {
                  ld->set ("p", -Pexp);
                  ++found;
                }
              if (ld->checkFlag (adjustable_Q))
                {
                  ld->set ("q", -Qexp);
                  ++found;
                }
              if (found == 2)
                {
                  return 1;
                }
            }
        }
      else                     //TODO::PT:deal with multiple adjustable controls
        {
          return 0;
        }

    }
  return 0;
}
// -------------------- Power Flow --------------------

void acBus::registerVoltageControl (gridObject *obj)
{
  bool update = ((opFlags[pFlow_initialized]) && (type != busType::PQ));
  busController.addVoltageControlObject (obj, update);
}

void acBus::removeVoltageControl (gridObject *obj)
{

  busController.removeVoltageControlObject (obj->getID (), opFlags[pFlow_initialized]);
}

void acBus::registerPowerControl (gridObject *obj)
{
  bool update = ((opFlags[pFlow_initialized]) && (type != busType::PQ));
  busController.addPowerControlObject (obj, update);
}

void acBus::removePowerControl (gridObject *obj)
{
  busController.removePowerControlObject (obj->getID (), opFlags[pFlow_initialized]);

}


//guess the solution
void acBus::guess (double ttime, double state[], double dstate_dt[], const solverMode &sMode)
{

  auto Voffset = offsets.getVOffset (sMode);
  auto Aoffset = offsets.getAOffset (sMode);
  
  if (!opFlags[slave_bus])
    {
      if (Voffset != kNullLocation)
        {
          state[Voffset] = voltage;

          if (hasDifferential(sMode))
            {
              dstate_dt[Voffset] = 0.0;
            }
        }
      if (Aoffset != kNullLocation)
        {
          state[Aoffset] = angle;
          if (hasDifferential(sMode))
            {
              dstate_dt[Aoffset] = 0.0;
            }
        }
    }
  for (auto &gen : attachedGens)
    {
      if (gen->stateSize (sMode) > 0)
        {
          gen->guess (ttime, state, dstate_dt, sMode);
        }
    }
  for (auto &load : attachedLoads)
    {
      if (load->stateSize (sMode) > 0)
        {
          load->guess (ttime, state, dstate_dt, sMode);
        }
    }
  if (opFlags[compute_frequency])
    {
      fblock->guess (ttime, state, dstate_dt, sMode);
    }
}

// set algebraic and dynamic variables assume preset to differential
void acBus::getVariableType (double sdata[], const solverMode &sMode)
{

  auto Voffset = offsets.getVOffset (sMode);
  if (Voffset != kNullLocation)
    {
      sdata[Voffset] = ALGEBRAIC_VARIABLE;
    }

  auto Aoffset = offsets.getAOffset (sMode);
  if (Aoffset != kNullLocation)
    {
      sdata[Aoffset] = ALGEBRAIC_VARIABLE;
    }

  for (auto &gen : attachedGens)
    {
      if (gen->enabled)
        {
          gen->getVariableType (sdata, sMode);
        }
    }

  for (auto &load : attachedLoads)
    {
      if ((load->checkFlag (has_dyn_states)) && (load->enabled))
        {
          load->getVariableType (sdata, sMode);
        }
    }
  if (opFlags[compute_frequency])
    {
      fblock->getVariableType (sdata, sMode);
    }

}

void acBus::getTols (double tols[], const solverMode &sMode)
{
  auto Voffset = offsets.getVOffset (sMode);
  if (Voffset != kNullLocation)
    {
      tols[Voffset] = Vtol;
    }
  auto Aoffset = offsets.getAOffset (sMode);
  if (Aoffset != kNullLocation)
    {

      tols[Aoffset] = Atol;
    }


  for (auto &gen : attachedGens)
    {
      if (gen->enabled)
        {
          gen->getTols (tols, sMode);
        }
    }

  for (auto &load : attachedLoads)
    {
      if ((load->stateSize (sMode) > 0) && (load->enabled))
        {
          load->getTols (tols, sMode);
        }
    }
  if (opFlags[compute_frequency])
    {
      fblock->getTols (tols, sMode);
    }
}

// pass the solution
void acBus::setState (double ttime, const double state[], const double dstate_dt[], const solverMode &sMode)
{

  auto Aoffset = offsets.getAOffset (sMode);
  auto Voffset = offsets.getVOffset (sMode);


  if (isDAE (sMode))
    {
      if (Voffset != kNullLocation)
        {
          voltage = state[Voffset];
          m_dstate_dt[voltageInLocation] = dstate_dt[Voffset];
        }
      if (Aoffset != kNullLocation)
        {
          angle = state[Aoffset];
          m_dstate_dt[angleInLocation] = dstate_dt[Aoffset];
        }
    }
  else if (hasAlgebraic (sMode))
    {
      if (Voffset != kNullLocation)
        {
          if (ttime > prevTime)
            {
              m_dstate_dt[voltageInLocation] = (state[Voffset] - m_state[voltageInLocation]) / (ttime - lastSetTime);
            }
          voltage = state[Voffset];
        }
      if (Aoffset != kNullLocation)
        {
          if (ttime > prevTime)
            {
              m_dstate_dt[angleInLocation] = (state[Aoffset] - -m_state[angleInLocation]) / (ttime - lastSetTime);
            }
          angle = state[Aoffset];
        }
      lastSetTime = ttime;
    }
  gridBus::setState (ttime, state, dstate_dt, sMode);

  if (opFlags[compute_frequency])
    {
      fblock->setState (ttime, state, dstate_dt, sMode);
    }
  else if ((isDynamic (sMode)) && (keyGen))
    {
      freq = keyGen->getFreq (nullptr,sMode);
    }
  //	assert(voltage > 0.0);

}

// residual
void acBus::residual (const stateData *sD, double resid[], const solverMode &sMode)
{
  gridBus::residual (sD, resid, sMode);

  auto Aoffset = offsets.getAOffset (sMode);
  auto Voffset = offsets.getVOffset (sMode);
 
  // output
  if (hasAlgebraic (sMode))
    {
      if (Voffset != kNullLocation)
        {
          if (useVoltage (sMode))
            {
              assert (!std::isnan (S.linkQ));

              resid[Voffset] = S.sumQ ();
#ifdef TRACE_LOG_ENABLE
              if (std::abs (resid[Voffset]) > 0.5)
                {
                  LOG_TRACE ("sid=" + std::to_string (sD->seqID) + "::high voltage resid = " + std::to_string (resid[Voffset]));
                }
#endif

            }
          else
            {
              resid[Voffset] = sD->state[Voffset] - voltage;
            }
        }
      if (Aoffset != kNullLocation)
        {
          if (useAngle (sMode))
            {
              assert (!std::isnan (S.linkP));
              resid[Aoffset] = S.sumP ();
#ifdef TRACE_LOG_ENABLE
              if (std::abs (resid[Aoffset]) > 0.5)
                {
                  LOG_TRACE ("sid=" + std::to_string (sD->seqID) + "::high angle resid = " + std::to_string (resid[Aoffset]));
                }
#endif
              // assert(std::abs(resid[Aoffset])<0.1);
            }
          else
            {
              resid[Aoffset] = sD->state[Aoffset] - angle;
            }
        }
      if (isExtended (sMode))
        {
          auto offset = offsets.getAlgOffset (sMode);
          //there is no function for this, the control must come from elsewhere in the state
          resid[offset] = 0;
          resid[offset + 1] = 0;
        }
    }

  if ((fblock) && (isDynamic (sMode)))
    {
      fblock->residElements (getAngle (sD,sMode), 0, sD, resid, sMode);
    }
}

void acBus::derivative (const stateData *sD, double deriv[], const solverMode &sMode)
{
  gridBus::derivative (sD, deriv, sMode);
  if (opFlags[compute_frequency])
    {
      fblock->derivElements (getAngle (sD,sMode), 0, sD, deriv, sMode);
    }
}

// Jacobian
void acBus::jacobianElements (const stateData *sD, arrayData<double> *ad, const solverMode &sMode)
{
  gridBus::jacobianElements (sD, ad, sMode);

  //deal with the frequency block
  auto Aoffset = offsets.getAOffset (sMode);
  if ((fblock) && (isDynamic (sMode)))
    {
      fblock->jacElements (outputs[angleInLocation], 0, sD, ad, Aoffset, sMode);
    }

  computeDerivatives (sD, sMode);
  if (isDifferentialOnly (sMode))
    {
      return;
    }

  //compute the bus Jacobian elements themselves
  //printf("t=%f,id=%d, dpdt=%f, dpdv=%f, dqdt=%f, dqdv=%f\n", ttime, id, Ptii, Pvii, Qvii, Qtii);

  auto Voffset = offsets.getVOffset (sMode);

  if (Voffset != kNullLocation)
    {
      if (useVoltage (sMode))
        {
          ad->assignCheckCol (Voffset, Aoffset, partDeriv.at (QoutLocation, angleInLocation));
          ad->assign (Voffset, Voffset, partDeriv.at (QoutLocation, voltageInLocation));
          if (opFlags[uses_bus_frequency])
            {
              ad->assignCheckCol (Voffset, outLocs[frequencyInLocation], partDeriv.at (QoutLocation, frequencyInLocation));
            }
        }
      else
        {
          ad->assign (Voffset, Voffset, 1);
        }
    }
  if (Aoffset != kNullLocation)
    {
      if (useAngle (sMode))
        {
          ad->assign (Aoffset, Aoffset, partDeriv.at (PoutLocation, angleInLocation));
          ad->assignCheckCol (Aoffset, Voffset, partDeriv.at (PoutLocation, voltageInLocation));
          if (opFlags[uses_bus_frequency])
            {
              ad->assignCheckCol (Aoffset, outLocs[frequencyInLocation], partDeriv.at (PoutLocation, frequencyInLocation));
            }
        }
      else
        {
          ad->assign (Aoffset, Aoffset, 1);
        }
    }


  if (!isConnected ())
    {
      return;
    }
  od.setArray (ad);

  od.setTranslation (PoutLocation, useAngle (sMode) ? outLocs[angleInLocation] : kNullLocation);
  od.setTranslation (QoutLocation, useVoltage (sMode) ? outLocs[voltageInLocation] : kNullLocation);
  if (!isExtended (sMode))
    {
      for (auto &gen : attachedGens)
        {
          if (gen->jacSize (sMode) > 0)
            {

              gen->outputPartialDerivatives (outputs, sD, &od, sMode);
            }
        }
      for (auto &load : attachedLoads)
        {
          if (load->jacSize (sMode) > 0)
            {
              load->outputPartialDerivatives (outputs, sD, &od, sMode);
            }

        }
    }
  else
    {               //make the assignments for the extended state
      auto offset = offsets.getAlgOffset (sMode);
      od.assign (PoutLocation, offset, 1);
      od.assign (QoutLocation, offset + 1, 1);
    }
  int gid = getID ();
  for (auto &link : attachedLinks)
    {
      link->outputPartialDerivatives (gid, sD, &od, sMode);
    }

}


inline double dVcheck (double dV, double currV, double drFrac = 0.75, double mxRise = 0.2, double cRcheck = 0)
{

  if (currV - dV > cRcheck)
    {
      if (dV < -mxRise)
        {
          dV = -mxRise;
        }
    }
  if (dV > drFrac * currV)
    {
      dV = drFrac * currV;
    }
  return dV;
}

inline double dAcheck (double dT, double /*currA*/, double mxch = kPI / 8.0)
{
  if (std::abs (dT) > mxch)
    {
      dT = std::copysign (mxch, dT);
    }
  return dT;
}

void acBus::voltageUpdate (const stateData *sD, double update[], const solverMode &sMode, double alpha)
{
  if (!isConnected ())
    {
      return;
    }
  auto Voffset = offsets.getVOffset (sMode);
  double v1 = getVoltage (sD, sMode);
  if (v1 < Vtol)
    {
      alert (this, VERY_LOW_VOLTAGE_ALERT);
	  lowVtime = sD ->time;
      return;
    }
  if (!((useVoltage (sMode)) && (Voffset != kNullLocation)))
    {
      update[Voffset] = v1;
      return;
    }
  bool uA = useAngle (sMode);

  updateLocalCache (sD, sMode);
  computeDerivatives (sD, sMode);

  double DP = S.sumP ();
  double DQ = (uA) ? (S.sumQ ()) : 0;

  double Pvii = (uA) ? (partDeriv.at (PoutLocation, voltageInLocation)) : 1.0;        //so not to divide by 0
  double Qvii = partDeriv.at (QoutLocation, voltageInLocation);

  double dV = DQ / Qvii + DP / Pvii;
  if (!std::isfinite (dV))           //probably means the real power computation is invalid
    {
      dV = DQ / Qvii;
    }
  dV = dVcheck (dV, v1, 0.75, 0.15, 1.05);

  assert (std::isfinite (dV));
  assert (v1 - dV > 0);
  update[Voffset] = v1 - dV * alpha;
}

void acBus::algebraicUpdate (const stateData *sD, double update[], const solverMode &sMode, double alpha)
{


  auto Voffset = offsets.getVOffset (sMode);
  auto Aoffset = offsets.getAOffset (sMode);
  double v1 = getVoltage (sD, sMode);
  double t1 = getAngle (sD, sMode);
  bool uV = (useVoltage (sMode)) && (Voffset != kNullLocation);
  bool uA = (!(opFlags[ignore_angle])) && (useAngle (sMode)) && (Aoffset != kNullLocation);

  if (uV & uA)
    {

      updateLocalCache (sD, sMode);
      computeDerivatives (sD, sMode);

      double DP = S.sumP ();
      double DQ = S.sumQ ();
      double dV, dT;

      double Pvii = partDeriv.at (PoutLocation, voltageInLocation);
      double Ptii = partDeriv.at (PoutLocation, angleInLocation);
      double Qvii = partDeriv.at (QoutLocation, voltageInLocation);
      double Qtii = partDeriv.at (QoutLocation, angleInLocation);
      double detA = solve2x2 (Pvii, Ptii, Qvii, Qtii, DP, DQ, dV, dT);
      if (std::isnormal (detA))
        {
          dV = dVcheck (dV, v1);
          dT = dAcheck (dT, t1);
        }
      else if (Ptii != 0)
        {
          dT = dAcheck (DP / Ptii, t1);
          dV = 0;
        }
      else
        {
          dV = dT = 0;
        }
      if (uV)
        {
          assert (std::isfinite (dV));
          assert (v1 - dV > 0);
          update[Voffset] = v1 - dV * alpha;
        }
      if (uA)
        {
          assert (std::isfinite (dT));
          update[Aoffset] = t1 - dT * alpha;
        }
    }
  else if (uA)
    {
      updateLocalCache (sD, sMode);
      computeDerivatives (sD, sMode);

      double DP = S.sumP ();
      double Ptii = partDeriv.at (PoutLocation, angleInLocation);
      if (Ptii != 0)
        {
          double dT = dAcheck (DP / Ptii, t1);
          assert (std::isfinite (dT));
          update[Aoffset] = t1 - dT * alpha;
        }
      else
        {
          update[Aoffset] = t1;
        }

      if (Voffset != kNullLocation)
        {
          update[Voffset] = v1;
        }
    }
  else if (uV)
    {
      updateLocalCache (sD, sMode);
      computeDerivatives (sD, sMode);

      double DQ = S.sumQ ();
      double Qvii = partDeriv.at (QoutLocation, voltageInLocation);
      if (Qvii != 0)
        {
          double dV = dVcheck (DQ / Qvii, v1);
          assert (std::isfinite (dV));
          update[Voffset] = v1 - dV * alpha;
        }
      else
        {
          update[Aoffset] = t1;
        }
      if (Aoffset != kNullLocation)
        {
          update[Aoffset] = t1;
        }
    }
  else
    {
      if (Aoffset != kNullLocation)
        {
          update[Aoffset] = t1;
        }
      if (Voffset != kNullLocation)
        {
          update[Voffset] = v1;
        }
    }
  gridBus::algebraicUpdate (sD, update, sMode, alpha);
}

void acBus::localConverge (const solverMode &sMode, int mode, double tol)
{
  if (isDifferentialOnly (sMode))
    {
      return;
    }
  double v1 = voltage, t1 = angle;
  double dV, dT;
  double Pvii, Ptii, Qvii, Qtii;
  double err = kBigNum;
  int iteration = 1;


  updateLocalCache ();
  double DP = S.sumP ();
  double DQ = S.sumQ ();
  if ((std::abs (DP) < Atol) && (std::abs (DQ) < Vtol))
    {
      return;
    }
  if ((S.loadP == 0) && (S.linkP == 0) && (S.loadQ == 0) && (S.linkQ == 0))
    {

      if (checkCapable () == false)
        {
          LOG_WARNING ("Bus disconnected");
          disconnect ();
        }
      return;
    }
  computeDerivatives (nullptr, sMode);
  if (mode == 0)
    {
      Pvii = partDeriv.at (PoutLocation, voltageInLocation);
      Ptii = partDeriv.at (PoutLocation, angleInLocation);
      Qvii = partDeriv.at (QoutLocation, voltageInLocation);
      Qtii = partDeriv.at (QoutLocation, angleInLocation);
      double detA = solve2x2 (Pvii, Ptii, Qvii, Qtii, DP, DQ, dV, dT);
      if (std::isnormal (detA))
        {
          dV = dVcheck (dV, voltage);
          dT = dAcheck (dT, angle);
        }
      else if (Ptii != 0)
        {
          dT = dAcheck (DP / Ptii, angle);
          dV = 0;
        }
      else
        {
          dV = dT = 0;
        }
      voltage -= dV;
      angle -= dT;
    }
  else if (mode == 1)
    {
      bool not_converged = true;
      while (not_converged)
        {
          if (iteration > 1)
            {
              v1 = voltage;
              t1 = angle;

              updateLocalCache ();
              computeDerivatives (nullptr, sMode);
              DP = S.sumP ();
              DQ = S.sumQ ();
            }
          switch (getMode (sMode))
            {
            case 0:
              err = std::abs (DP) + std::abs (DQ);
              break;
            case 1:                                     //fixA
              err = std::abs (DQ);
              break;
            case 2:
              err = std::abs (DP);
              break;
            }
          if (err > tol)
            {
              Pvii = partDeriv.at (PoutLocation, voltageInLocation);
              Ptii = partDeriv.at (PoutLocation, angleInLocation);
              Qvii = partDeriv.at (QoutLocation, voltageInLocation);
              Qtii = partDeriv.at (QoutLocation, angleInLocation);
              double detA = solve2x2 (Pvii, Ptii, Qvii, Qtii, DP, DQ, dV, dT);
              if (std::isnormal (detA))
                {
                  dV = dVcheck (dV, v1);
                  dT = dAcheck (dT, t1);
                }
              else if (Ptii != 0)
                {

                  dT = dAcheck (DP / Ptii, t1);
                  dV = 0;
                }
              else
                {
                  dV = dT = 0;
                  not_converged = false;
                }
              voltage -= dV;
              angle -= -dT;
              if (++iteration > 10)
                {
                  not_converged = false;
                  voltage = v1;
                  angle = t1;
                }
            }
          else
            {
              not_converged = false;
            }
        }
    }
}

void acBus::converge (double ttime, double state[], double dstate_dt[], const solverMode &sMode, converge_mode mode, double tol)
{
  if ((!enabled) || (isDifferentialOnly (sMode)) || (opFlags[disconnected]))            //nothing to do if differential
    {
      return;
    }


  double dV;
  auto Voffset = offsets.getVOffset (sMode);
  auto Aoffset = offsets.getAOffset (sMode);

  bool uV = useVoltage (sMode) & (Voffset != kNullLocation);
  bool uA = useAngle (sMode) & (Aoffset != kNullLocation);
  stateData sD;
  sD.state = state;
  sD.dstate_dt = dstate_dt;
  sD.time = ttime;
  double v1 = uV ? state[Voffset] : voltage;
  double t1 = uA ? state[Aoffset] : angle;
  double v2 = voltage, t2 = angle;
  double f = getFreq (&sD, sMode);
  if (v1 <= 0.0)
    {
      v1 = std::abs (v1 - 0.001);
      if (Voffset != kNullLocation)
        {
          state[Voffset] = v1;
        }

    }
  double currentModeVlimit = 0.02 * vTarget;
  bool forceUp = false;
  int iteration = 1;
  if (isDAE (sMode))
    {
      currentModeVlimit = (attachedGens.size () > 0) ? 0.4 : 0.05;
      currentModeVlimit *= vTarget;
    }
  if (v1 < currentModeVlimit)
    {
      mode = converge_mode::voltage_only;
    }

  double err = computeError (&sD, sMode);
  if ((S.loadP == 0) && (S.linkP == 0) && (S.loadQ == 0) && (S.linkQ == 0))
    {

      if (checkCapable () == false)
        {
          LOG_WARNING ("Bus disconnected");
          disconnect ();
        }
      return;
    }
  switch (mode)
    {
    case converge_mode::high_error_only:
      if (err > 0.5)
        {
          if (err > 2.0)
            {
              algebraicUpdate (&sD, state, sMode, 1.0);
              err = computeError (&sD, sMode);
              int loopcnt = 0;
              while ((err > tol) && (loopcnt < 6))
                {
                  voltageUpdate (&sD, state, sMode, 1.0);
                  err = computeError (&sD, sMode);
                  ++loopcnt;
                }
            }
          else
            {
              //do the algebraic update twice
              algebraicUpdate (&sD, state, sMode, 1.0);
              algebraicUpdate (&sD, state, sMode, 1.0);
            }
        }
      break;
    case converge_mode::single_iteration:
    case converge_mode::block_iteration:
      algebraicUpdate (&sD, state, sMode, 1.0);
      break;
    case converge_mode::local_iteration:
    case converge_mode::strong_iteration:
      while (err > tol)
        {
          v1 = uV ? state[Voffset] : voltage;
          t1 = uA ? state[Aoffset] : angle;
          if (v1 < currentModeVlimit)
            {
              mode = converge_mode::voltage_only;
              converge (ttime, state, dstate_dt, sMode, mode, tol);
              break;
            }
          algebraicUpdate (&sD, state, sMode, 1.0);
          v2 = uV ? state[Voffset] : voltage;
          t2 = uA ? state[Aoffset] : angle;
          if ((std::abs (v2 - v1) < 1e-9) && (std::abs (t2 - t1) < 1e-9))
            {
              break;
            }
          err = computeError (&sD, sMode);
          if (++iteration > 10)
            {
              break;
            }
        }
      break;
    case converge_mode::voltage_only:
      {
        bool not_converged = true;
        if (v1 > 0.6)
          {
            not_converged = false;
          }
        double minV = -kBigNum;
        double pcerr = 120000;
        while (not_converged)
          {
            if (iteration > 1)
              {
                v1 = uV ? state[Voffset] : voltage;
                if (v1 > vTarget * 1.1)
                  {
                    converge (ttime, state, dstate_dt, sMode, converge_mode::strong_iteration, tol);
                    break;
                  }

              }
            updateLocalCache (&sD, sMode);
            computeDerivatives (&sD, sMode);
            double DP = S.sumP ();
            double DQ = S.sumQ ();
            double cerr1 = DP / v1;
            double cerr2 = DQ / v1;

            if (iteration == 1)
              {
                pcerr = cerr2;
              }
            double Pvii = partDeriv.at (PoutLocation, voltageInLocation);
            double Qvii = partDeriv.at (QoutLocation, voltageInLocation);
            if (std::abs (cerr1) + std::abs (cerr2) > tol)
              {
                dV = 0.0;
                if (std::abs (cerr2) > tol)
                  {

                    if (cerr2 < 0)
                      {
                        if ((forceUp) || (iteration == 1))
                          {
                            dV = -0.1;
                            forceUp = true;
                            iteration = (iteration > 5) ? 5 : iteration;
                          }
                        else
                          {

                            dV = DQ / Qvii + DP / Pvii;
                            if ((!std::isfinite (dV)) || ((minV > 0.35) && (v1 - dV < minV)))                                                       //probably means the real power computation is invalid
                              {
                                dV = DQ / Qvii;
                              }
                            dV = dVcheck (dV, v1, 0.75, 0.15, 1.05);
                          }

                      }
                    else
                      {
                        if (pcerr < 0)
                          {
                            if (forceUp)
                              {
                                minV = v1 - 0.1;
                              }
                          }
                        forceUp = false;
                        dV = DQ / Qvii + DP / Pvii;
                        if ((!std::isfinite (dV)) || ((minV > 0.35) && (v1 - dV < minV)))                                               //probably means the real power computation is invalid
                          {
                            dV = DQ / Qvii;
                          }
                        dV = dVcheck (dV, v1, 0.75, 0.15, 1.05);
                      }
                  }
                else if (std::abs (cerr1) > tol)
                  {
                    dV = DQ / Qvii + DP / Pvii;
                    if ((!std::isfinite (dV)) || ((minV > 0.35) && (v1 - dV < minV)))                                       //probably means the real power computation is invalid
                      {
                        dV = DQ / Qvii;
                        not_converged = false;
                      }
                    dV = dVcheck (dV, v1, 0.75, 0.15, 1.05);
                  }
                else
                  {
                    not_converged = false;
                  }
                if (uV)
                  {
                    assert (std::isfinite (dV));
                    assert (v1 - dV > 0);
                    state[Voffset] = v1 - dV;
                  }

                if (isDynamic (sMode))
                  {
                    for (auto &gen : attachedGens)
                      {
                        stateData s1;
                        s1.state = state;

                        gen->algebraicUpdate ({ v1 - dV, t1,f }, &s1, state, sMode, 1.0);
                      }
                  }
                if (++iteration > 10)
                  {
                    not_converged = false;
                  }
              }
            else
              {
                not_converged = false;
              }
          }


      }
      break;
    }
}


double acBus::computeError (const stateData *sD, const solverMode &sMode)
{
  updateLocalCache (sD, sMode);
  double err = 0;
  switch (getMode (sMode))
    {
    case 0:      //0 most common
      err = std::abs (S.sumP ()) + std::abs (S.sumQ ());
      break;
    case 2:     //PV bus
      err = std::abs (S.sumP ());
      break;
    case 1:         //fixA
      err = std::abs (S.sumQ ());
      break;
    }
  return err;
}

void acBus::getStateName (stringVec &stNames, const solverMode &sMode, const std::string &prefix) const
{
  if (hasAlgebraic (sMode))
    {
      auto Voffset = offsets.getVOffset (sMode);
      auto Aoffset = offsets.getAOffset (sMode);

      count_t bst = 0;
      if (stNames.size () < std::max (Voffset, Aoffset) + 1)
        {
          stNames.resize (std::max (Voffset, Aoffset) + 1);
        }
      if (Voffset != kNullLocation)
        {
          stNames[Voffset] = name + ":voltage";
          ++bst;
        }
      if (Aoffset != kNullLocation)
        {
          stNames[Aoffset] = name + ":angle";
          ++bst;
        }
      if (stateSize (sMode) == bst)
        {
          return;
        }
    }
  
  gridBus::getStateName(stNames, sMode, prefix);
  if ((fblock) && (isDynamic (sMode)))
    {
      std::string prefix2 = prefix + name + "::";
      fblock->getStateName (stNames, sMode, prefix2);
    }
}


void acBus::setOffsets (const solverOffsets &newOffsets, const solverMode &sMode)
{
  offsets.setOffsets (newOffsets, sMode);
  solverOffsets no (newOffsets);
  no.localIncrement (offsets.getOffsets (sMode));
  for (auto ld : attachedLoads)
    {
      ld->setOffsets (no, sMode);
      no.increment (ld->getOffsets (sMode));
    }
  for (auto gen : attachedGens)
    {
      gen->setOffsets (no, sMode);
      no.increment (gen->getOffsets (sMode));
    }
  if (opFlags[slave_bus])
    {
      auto so = offsets.getOffsets (sMode);
	  auto mboffsets = busController.masterBus->getOffsets(sMode);
      so->vOffset = mboffsets->vOffset;
      so->aOffset = mboffsets->aOffset;;
    }
  else
    {
      if ((fblock) && (isDynamic (sMode)))
        {
          fblock->setOffsets (no, sMode);
          no.increment (fblock->getOffsets (sMode));
        }
    }
}

void acBus::setOffset (index_t offset, const solverMode &sMode)
{
  for (auto ld : attachedLoads)
    {
      ld->setOffset (offset, sMode);
      offset += ld->stateSize (sMode);

    }
  for (auto gen : attachedGens)
    {
      gen->setOffset (offset, sMode);
      offset += gen->stateSize (sMode);
    }
  if (opFlags[slave_bus])
    {
      auto so = offsets.getOffsets (sMode);
	  auto mboffsets = busController.masterBus->getOffsets(sMode);
      so->vOffset = mboffsets->vOffset;
      so->aOffset = mboffsets->aOffset;
    }
  else
    {
      if ((fblock) && (isDynamic (sMode)))
        {
          fblock->setOffset (offset, sMode);
          offset += fblock->stateSize (sMode);
        }
      offsets.setOffset (offset, sMode);
    }

}

void acBus::setRootOffset (index_t Roffset, const solverMode &sMode)
{

  offsets.setRootOffset (Roffset, sMode);
  auto so = offsets.getOffsets (sMode);
  auto nR = so->local.algRoots + so->local.diffRoots;
  for (auto &gen : attachedGens)
    {
      gen->setRootOffset (Roffset + nR, sMode);
      nR += gen->rootSize (sMode);
    }
  for (auto &ld : attachedLoads)
    {
      ld->setRootOffset (Roffset + nR, sMode);
      nR += ld->rootSize (sMode);
    }
  if (opFlags[compute_frequency])
    {
      fblock->setRootOffset (Roffset + nR, sMode);
      nR += fblock->rootSize (sMode);
    }
}

void acBus::reconnect (gridBus *mapBus)
{
  if (opFlags[disconnected])
    {
      gridBus::reconnect (mapBus);
      for (auto &sB : busController.slaveBusses)
        {
          sB->reconnect (this);
        }
    }
  else
    {
      return;

    }

}
bool acBus::useAngle (const solverMode &sMode) const
{
  if ((hasAlgebraic (sMode)) && (isConnected ()))
    {
      if (isDynamic (sMode))
        {
          if ((dynType == dynBusType::normal) || (dynType == dynBusType::fixVoltage))
            {
              return true;
            }
        }
      else if ((type == busType::PQ) || (type == busType::PV))
        {
          return true;
        }
    }
  return false;
}

bool acBus::useVoltage (const solverMode &sMode) const
{
  if ((hasAlgebraic (sMode)) && (isConnected ()) && (!isDC (sMode)))
    {
      if (isDynamic (sMode))
        {
          if ((dynType == dynBusType::normal) || (dynType == dynBusType::fixAngle))
            {
              return true;
            }
        }
      else if ((type == busType::PQ) || (type == busType::afix))
        {
          return true;
        }
    }
  return false;
}

void acBus::loadSizes (const solverMode &sMode, bool dynOnly)
{
  if (isLoaded (sMode, dynOnly))
    {
      return;
    }
  auto so = offsets.getOffsets (sMode);
  if (!enabled)
    {
      so->reset ();
      so->stateLoaded = true;
      so->rjLoaded = true;
      return;
    }
  if (dynOnly)
    {
      so->rootAndJacobianCountReset ();
    }
  else
    {
      so->reset ();
    }
  if (hasAlgebraic (sMode))
    {
      so->local.aSize = 1;
      if (isDC (sMode))
        {
          so->local.jacSize = 1 + 2 * static_cast<count_t> (attachedLinks.size ());
        }
      else
        {
          so->local.vSize = 1;
          so->local.jacSize = 4 + 4 * static_cast<count_t> (attachedLinks.size ());
        }
      //check for slave bus mode
      if (opFlags[slave_bus])
        {
          so->local.vSize = 0;
          so->local.aSize = 0;
          so->local.jacSize -= (isDC (sMode)) ? 1 : 4;
        }
    }


  if (dynOnly)
    {
      so->total.algRoots = so->local.algRoots;
      so->total.diffRoots = so->local.diffRoots;
      so->total.jacSize = so->local.jacSize;
    }
  else
    {
      so->localLoad (false);
    }
  if (isExtended (sMode))                    //in extended state mode we have P and Q as states
    {
      if (isDC (sMode))
        {
          so->total.algSize = so->local.algSize = 1;
        }
      else
        {
          so->total.algSize = so->local.algSize = 2;
        }

    }
  else
    {

      for (auto ld : attachedLoads)
        {
          if (!(ld->isLoaded (sMode, dynOnly)))
            {
              ld->loadSizes (sMode, dynOnly);
            }
          if (dynOnly)
            {
              so->addRootAndJacobianSizes (ld->getOffsets (sMode));
            }
          else
            {
              so->addSizes (ld->getOffsets (sMode));
            }
        }
      for (auto gen : attachedGens)
        {
          if (!(gen->isLoaded (sMode, dynOnly)))
            {
              gen->loadSizes (sMode, dynOnly);
            }
          if (dynOnly)
            {
              so->addRootAndJacobianSizes (gen->getOffsets (sMode));
            }
          else
            {
              so->addSizes (gen->getOffsets (sMode));
            }
        }
    }
  if ((fblock) && (isDynamic (sMode)))
    {
      if (!(fblock->isLoaded (sMode, dynOnly)))
        {
          fblock->loadSizes (sMode, dynOnly);
        }
      if (dynOnly)
        {
          so->addRootAndJacobianSizes (fblock->getOffsets (sMode));
        }
      else
        {
          so->addSizes (fblock->getOffsets (sMode));
        }
    }
  if (!dynOnly)
    {
      so->stateLoaded = true;

    }
  so->rjLoaded = true;
  /*if (!isDynamic(sMode))
  {
  if (stateSize(sMode)>2)
  {
  printf("%s %d has statesize=%d\n",name.c_str(),id, stateSize(sMode));
  }
  }*/
}

int acBus::getMode (const solverMode &sMode) const
{
  if (isDynamic (sMode))
    {
      if (isDifferentialOnly (sMode))
        {
          return 3;
        }
      if (isDC (sMode))
        {
          return (static_cast<int> (dynType) | 2);
        }
      else
        {
          return static_cast<int> (dynType);
        }
    }
  else
    {
      if (isDC (sMode))
        {
          return (static_cast<int> (type) | 2);
        }
      else
        {
          return static_cast<int> (type);
        }
    }
}


void acBus::updateFlags (bool /*dynOnly*/)
{
  opFlags.reset (preEx_requested);
  opFlags.reset (has_powerflow_adjustments);
  if (prevType == busType::SLK)
    {
      //check for P limits
      if ((busController.Pmin > -kHalfBigNum) || (busController.Pmax < kHalfBigNum))
        {
          opFlags[has_powerflow_adjustments] = true;
        }

      //check for Qlimits
      if ((busController.Qmin > -kHalfBigNum) || (busController.Qmax < kHalfBigNum))
        {
          opFlags[has_powerflow_adjustments] = true;
        }
    }



  busController.Qmin = 0;
  busController.Qmax = 0;
  for (auto &gen : attachedGens)
    {
      if (gen->enabled)
        {
          opFlags |= gen->cascadingFlags ();
          busController.Qmin += gen->getQmin ();
          busController.Qmax += gen->getQmax ();
        }

    }
  for (auto &load : attachedLoads)
    {
      if (load->enabled)
        {
          opFlags |= load->cascadingFlags ();
        }
    }
  if (opFlags[compute_frequency])
    {
      opFlags |= fblock->cascadingFlags ();
    }
  if (prevType == busType::PV)
    {
      if ((busController.Qmin > -kHalfBigNum) || (busController.Qmax < kHalfBigNum))
        {
          opFlags[has_powerflow_adjustments] = true;
        }
    }
  else if (prevType == busType::afix)
    {
      if ((busController.Pmin > -kHalfBigNum) || (busController.Pmax < kHalfBigNum))
        {
          opFlags[has_powerflow_adjustments] = true;
        }

    }
}

static const IOlocs inLoc {
  0,1,2
};

void acBus::computeDerivatives (const stateData *sD, const solverMode &sMode)
{

  if (!isConnected ())
    {
      return;
    }
  partDeriv.clear ();

  for (auto &link : attachedLinks)
    {
      if (link->enabled)
        {
          link->updateLocalCache (sD, sMode);
          link->ioPartialDerivatives (getID (), sD, &partDeriv, inLoc, sMode);
        }
    }
  if (!isExtended (sMode))
    {
      for (auto &gen : attachedGens)
        {
          if (gen->isConnected ())
            {
              gen->ioPartialDerivatives (outputs, sD, &partDeriv, inLoc, sMode);
            }
        }
      for (auto &load : attachedLoads)
        {
          if (load->isConnected ())
            {

              load->ioPartialDerivatives (outputs, sD, &partDeriv, inLoc, sMode);
            }
        }
    }

}

// computed power at bus
void acBus::updateLocalCache (const stateData *sD, const solverMode &sMode)
{
    
  if (!S.needsUpdate (sD))
    {
      return;
    }

  if (!isConnected ())
    {
      return;
    }
  gridBus::updateLocalCache (sD, sMode);
  if (sMode.offsetIndex != lastSmode)
    {
      outLocs = getOutputLocs (sMode);
    }

}


// computed power at bus
void acBus::updateLocalCache ()
{

  gridBus::updateLocalCache ();

}

void acBus::computePowerAdjustments ()
{
  //declaring an embedded function
  auto cid = getID ();

  S.reset ();

  for (auto &link : attachedLinks)
    {
      if ((link->isConnected()) && (!busController.hasVoltageAdjustments (link->getID ())))
        {
          S.linkQ += link->getReactivePower (cid);
        }
      if ((link->isConnected()) && (!busController.hasPowerAdjustments (link->getID ())))
        {
          S.linkP += link->getRealPower (cid);
        }

    }
  for (auto &load : attachedLoads)
    {
      if ((load->isConnected ()) && (!busController.hasVoltageAdjustments (load->getID ())))
        {
          S.loadQ += load->getReactivePower (voltage);
        }
      if ((load->isConnected ()) && (!busController.hasPowerAdjustments (load->getID ())))
        {
          S.loadP += load->getRealPower (voltage);
        }
    }
  for (auto &gen : attachedGens)
    {
      if ((gen->isConnected ()) && (!busController.hasVoltageAdjustments (gen->getID ())))
        {

          S.genQ += gen->getReactivePower ();
        }
      if ((gen->isConnected ()) && (!busController.hasPowerAdjustments (gen->getID ())))
        {
          S.genP += gen->getRealPower ();
        }
    }
}

double acBus::getAdjustableCapacityUp (double time) const
{
  return busController.getAdjustableCapacityUp (time);
}

double acBus::getAdjustableCapacityDown (double time) const
{
  return busController.getAdjustableCapacityDown (time);
}



double acBus::get (const std::string &param, units_t unitType) const
{
  double val = kNullVal;
  if (param == "vtarget")
    {
      val = unitConversionPower (vTarget, puV, unitType, systemBasePower, baseVoltage);
    }
  else if (param == "atarget")
    {
      val = unitConversionAngle (aTarget, rad, unitType);
    }
  else if (param == "participation")
    {
      val = participation;
    }
  else if (param == "vmax")
    {
      val = Vmax;
    }
  else if (param == "vmin")
    {
      val = Vmin;
    }
  else if (param == "qmin")
    {
      val = busController.Qmin;
    }
  else if (param == "qmax")
    {
      val = busController.Qmax;
    }
  else if (param == "tw")
    {
      val = Tw;
    }
  else
    {
      return gridBus::get (param,unitType);
    }
  return val;
}

change_code acBus::rootCheck (const stateData *sD, const solverMode &sMode, check_level_t level)
{

  double vcurr = getVoltage(sD,sMode);
  change_code ret = change_code::no_change;
  if (level == check_level_t::low_voltage_check)
    {
      if (isConnected () == false)
        {
          return ret;
        }
      if (vcurr < 1e-8)
        {
          disconnect ();
          ret = change_code::jacobian_change;
          LOG_DEBUG ("Bus low voltage disconnect");
        }
      if (opFlags[prev_low_voltage_alert])
        {
			if (sD->time<=lowVtime)
			{
				disconnect();
				opFlags.reset(prev_low_voltage_alert);
				ret = change_code::jacobian_change;
				LOG_DEBUG("Bus low voltage disconnect");
			}
			else
			{
				opFlags.reset(prev_low_voltage_alert);
			}
         
        }
      return ret;
    }
  if (level == check_level_t::complete_state_check)
    {
      if (vcurr < 1e-5)
        {
          LOG_NORMAL ("bus disconnecting from low voltage");
          disconnect ();
        }
      else if (isDAE (sMode))
        {
          if (dynType == dynBusType::normal)
            {
              if (vcurr < 0.001)
                {
                  prevDynType = dynBusType::normal;
                  refAngle = static_cast<gridArea *> (parent)->getMasterAngle (nullptr, cLocalSolverMode);

                  dynType = dynBusType::fixAngle;
                  alert (this, JAC_COUNT_DECREASE);
                  ret = change_code::jacobian_change;
                }
            }
          else if (dynType == dynBusType::fixAngle)
            {

              if (prevDynType == dynBusType::normal)
                {
                  if (vcurr > 0.1)
                    {
                      dynType = dynBusType::normal;
                      double nAngle = static_cast<gridArea *> (parent)->getMasterAngle (nullptr, cLocalSolverMode);
                      angle = angle + (nAngle - refAngle);
                      alert (this, JAC_COUNT_INCREASE);
                      ret = change_code::jacobian_change;
                    }
                }
            }
        }

    }
  //make sure we are not in a fault condition
  auto iret = gridBus::rootCheck(sD, sMode, level);
    if (iret>ret)
    {
        ret = iret;
    }
  
  return ret;
}



