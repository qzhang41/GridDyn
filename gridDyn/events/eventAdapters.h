/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil;  eval: (c-set-offset 'innamespace 0); -*- */
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
#ifndef EVENTADAPTERS_H_
#define EVENTADAPTERS_H_

#include "core/coreObject.h"
#include "core/helperTemplates.h"

#include "events/eventInterface.h"
#include "core/objectOperatorInterface.h"
#include "core/objectInterpreter.h"
#include <functional>
#include <memory>
#include <algorithm>
#include <iostream>
#include <type_traits>


template<class T>
struct is_shared_ptr : std::false_type {};

template<class T>
struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};
/** @brief class for managing events of many types
 class is a wrapper around a number of different kinds of discrete events
*/
class eventAdapter
{
public:
  index_t eventID;                 //!< eventID for searching
  bool m_remove_event = false;              //!<flag to remove the event after execution
  bool two_part_execute = false;              //!< flag if the event has two parts
  bool partB_turn = false;                      //!< if we need to execute the second part only
  bool partB_only = false;                 //!<flag indicating we only run when partB runs
  coreTime m_period;                 //!< the period of the event;
  coreTime m_nextTime;              //!< the next time the event is scheduled to execute
  coreTime partBdelay = timeZero;              //!<the delay between the first and second parts
private:
  static std::atomic<count_t> eventCounter;  //!< counter for generating the unique event ID
public:
  /** @brief constructor
  @param[in] nextTime the trigger time for the event
  @param[in] period the period of the event  (the event will trigger once per period starting at nextTime
  */
  eventAdapter (coreTime nextTime = maxTime, coreTime period = timeZero);

  /** @brief destructor*/
  virtual ~eventAdapter ();
  /** do not allow copy construction (or any other automatic defined functions*/
  eventAdapter(const eventAdapter &ev) = delete;
  /** make a copy of the eventAdapter 
  @param[in] eA  the pointer to copy the event adapter information to
  */
  virtual std::shared_ptr<eventAdapter> clone(std::shared_ptr<eventAdapter> eA = nullptr) const;

  /** Execute the pre-event portion of the event for two part execution events
  @param[in] cTime the current execution time
  */
  virtual void executeA (coreTime cTime);

  /** Execute the event or partB of the event
  @param[in] cTime the current execution time
  */
  virtual change_code execute (coreTime cTime);

  /** @brief update the next event time*/
  virtual void updateTime ();
  /** @brief update the target coreObject
  @param[in] newObject the new object
  @param[in] mode update_mode direct or match
  */
  virtual void updateObject(coreObject *newObject, object_update_mode mode = object_update_mode::direct);
  /** @brief get a list of all referenced objects*/
  virtual void getObjects(std::vector<coreObject *> &objects) const;
  /** initialize the event if needed*/
  virtual void initialize();

};

bool compareEventAdapters (const std::shared_ptr<eventAdapter> &e1, const std::shared_ptr<eventAdapter> &e2);

//TODO:: PT try to merge this adapter with the shared_ptr one
template<class Y>
class eventTypeAdapter : public eventAdapter
{
  static_assert (std::is_base_of<eventInterface, Y>::value, "classes must be inherited from eventInterface");
private:
  Y *m_eventObj;
public:
  eventTypeAdapter() :m_eventObj(nullptr)
  {

  }
  explicit eventTypeAdapter (Y *ge) : m_eventObj (ge)
  {
    m_nextTime = ge->nextTriggerTime ();
    switch (ge->executionMode ())
      {
      case event_execution_mode::normal:
        break;
      case event_execution_mode::two_part_execution:
        two_part_execute = true;
        break;
      case event_execution_mode::delayed:
        two_part_execute = true;
        partB_only = true;
        break;
	  default:
		  break;
      }
  }

  virtual std::shared_ptr<eventAdapter> clone(std::shared_ptr<eventAdapter> eA = nullptr) const override
  {
	  auto newAdapter = cloneBase<eventTypeAdapter<Y>, eventAdapter>(this, eA);
	  if (!newAdapter)
	  {
		  return eA;
	  }
	  newAdapter->m_eventObj = m_eventObj->clone();
	  return newAdapter;
  }

  virtual void updateObject(coreObject * newObject, object_update_mode mode) override
  {
		  m_eventObj->updateObject(newObject, mode);
		  updateTime();
  }
  virtual void getObjects(std::vector<coreObject *> &objects) const override
  {
		  m_eventObj->getObjects(objects);
  }
  virtual change_code execute (coreTime cTime) override
  {
    change_code retval = change_code::not_triggered;              //EVENT_NOT_TRIGGERED
	int excnt = 0;
    while (m_nextTime <= cTime)
      {
        auto ret = m_eventObj->trigger (cTime);
        retval = (std::max)(ret, retval);
        if (!(m_eventObj->isArmed ()))
          {
            m_remove_event = true;
            break;
          }
		auto lastTime = m_nextTime;
		updateTime();
		excnt = (lastTime == m_nextTime) ? excnt + 1 : 0;
		if (excnt > 4)
		{
			std::cerr << "Event time not updating\n";
			m_remove_event = true;
			break;
		}
      }
    return retval;
  }
  virtual void updateTime () override
  {
    m_nextTime = m_eventObj->nextTriggerTime ();
  }

  virtual void initialize() override
  {
	  m_eventObj->initialize();
  }
};


template<class Y>
class eventTypeAdapter<std::shared_ptr<Y> > : public eventAdapter
{
  static_assert (std::is_base_of<eventInterface, Y>::value, "classes must be inherited from eventInterface");
private:
  std::shared_ptr<Y> m_eventObj;
public:
  eventTypeAdapter() :m_eventObj(nullptr)
	{

	}
  explicit eventTypeAdapter (std::shared_ptr<Y> ge) : m_eventObj (std::move(ge))
  {
    m_nextTime = m_eventObj->nextTriggerTime ();
    switch (m_eventObj->executionMode ())
      {
      case event_execution_mode::normal:
        break;
      case event_execution_mode::two_part_execution:
        two_part_execute = true;
        break;
      case event_execution_mode::delayed:
        two_part_execute = true;
        partB_only = true;
        break;
	  default:
		  break;
      }
  }

  std::shared_ptr<eventAdapter> clone(std::shared_ptr<eventAdapter> eA = nullptr) const override
  {
	  auto newAdapter = cloneBase<eventTypeAdapter<std::shared_ptr<Y> >, eventAdapter>(this, eA);
	  if (!newAdapter)
	  {
		  return eA;
	  }
	  newAdapter->m_eventObj = std::static_pointer_cast<Y>(m_eventObj->clone());
	  return newAdapter;
  }

  virtual void updateObject(coreObject * newObject, object_update_mode mode) override
  {
	  m_eventObj->updateObject(newObject, mode);
	  updateTime();
  }

  virtual void getObjects(std::vector<coreObject *> &objects) const override
  {
	  m_eventObj->getObjects(objects);
  }
  change_code execute (coreTime cTime) override
  {
    change_code retval = change_code::not_triggered;                          //EVENT_NOT_TRIGGERED
	int excnt = 0; //!< counter for protection against an event not behaving properly
    while (m_nextTime <= cTime)
      {
        auto ret = m_eventObj->trigger (cTime);
        retval = (std::max) (ret, retval);
        if (!(m_eventObj->isArmed ()))
          {
            m_remove_event = true;
            break;
          }
		auto lastTime = m_nextTime;
        updateTime();
		excnt = (lastTime == m_nextTime) ? excnt + 1 : 0;
		if (excnt > 4)
		{
			std::cerr << "Event time not updating\n";
			m_remove_event = true;
			break;
		}
      }
    return retval;
  }

  virtual void updateTime () override
  {
    m_nextTime = m_eventObj->nextTriggerTime ();
  }
  virtual void initialize() override
  {
	  m_eventObj->initialize();
  }
};

class coreObject;

template<>
class eventTypeAdapter<coreObject> : public eventAdapter
{
private:
	coreObject *targetObject = nullptr;
public:
	eventTypeAdapter() = default;
  explicit eventTypeAdapter (coreObject *gco):targetObject(gco)
  {
    m_nextTime = targetObject->getNextUpdateTime ();
    two_part_execute = true;
  }

  virtual void updateObject(coreObject *newObject, object_update_mode mode) override
  {
	  if (mode == object_update_mode::direct)
	  {
		  targetObject = newObject;
		  
	  }
	  else if (mode==object_update_mode::match)
	  {
		  auto searchRes = findMatchingObject(targetObject, newObject);
		  if (searchRes)
		  {
			  targetObject = searchRes;
		  }
		  else
		  {
			  throw(objectUpdateFailException());
		  }
	  }
	  m_nextTime = (targetObject) ? targetObject->getNextUpdateTime() : maxTime;
  }

  virtual std::shared_ptr<eventAdapter> clone(std::shared_ptr<eventAdapter> eA = nullptr) const override
  {
	  auto newAdapter = cloneBase<eventTypeAdapter<coreObject>, eventAdapter>(this, eA);
	  if (!newAdapter)
	  {
		  return eA;
	  }
	  newAdapter->targetObject = targetObject;
	  return newAdapter;
  }

  virtual void executeA (coreTime cTime) override
  {
    targetObject->updateA (cTime);
  }

  virtual change_code execute (coreTime cTime) override
  {
    targetObject->updateB ();
    coreTime ttime = targetObject->getNextUpdateTime ();
    if ((ttime <= cTime) && (ttime <= m_nextTime))
      {
        m_nextTime = maxTime;
      }
    else
      {
        m_nextTime = ttime;
      }
    return change_code::parameter_change;
  }

  virtual void updateTime () override
  {
    m_nextTime = targetObject->getNextUpdateTime ();
  }


  coreObject* getTarget() const
  {
	  return targetObject;
  }
};
/** eventAdapter with a custom function call
 */
class functionEventAdapter : public eventAdapter
{
public:
	using ccode_function_t = std::function<change_code()>;
private:
	ccode_function_t fptr;            //!< the function to execute
public:
	functionEventAdapter();
	explicit functionEventAdapter(ccode_function_t fcal);
 
  functionEventAdapter(ccode_function_t fcal, coreTime triggerTime, coreTime period = 0.0);
  
  std::shared_ptr<eventAdapter> clone(std::shared_ptr<eventAdapter> eA = nullptr) const override;
 
  virtual change_code execute (coreTime cTime) override;

/** @brief set the function of the event adapter
 *@param[in] nfptr  a std::function which returns a change code and takes 0 arguments*/
  void setfunction(ccode_function_t nfptr);
};


#endif

