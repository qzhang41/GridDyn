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

#ifndef GD_OBJECT_FACTORY_H_
#define GD_OBJECT_FACTORY_H_

#include "gridCore.h"
#include <map>
#include <vector>
#include <memory>
#include <type_traits>


//!< typedef for convenience
typedef std::vector<std::string> stringVec;

/** @brief class definitions for the object factories that can create the objects
 cFactory is a virtual base class for object Construction functions
*/
class objectFactory
{
public:
  std::string  name; //!< factory name

  /** @brief constructor
  @param[in] componentName  the name of the type of component this factory is a constructor for
  @param[in] typeName  the name of the specific type of object this factor builds
  */
  objectFactory (const std::string & componentName, const std::string & typeName);

  /** @brief constructor B
  @param[in] componetName  the name of component this factory is a constructor for
  @param[in] typeNames  the names of the specific types of objects this factor builds
  */
  objectFactory (const std::string & componentName, const stringVec & typeNames);

  /** @brief make and object   abstract function
  @return a pointer to a newly constructed object
  */
  virtual gridCoreObject * makeObject () = 0;

  /** @brief make and object   abstract function
  @param[in] objectName  the name of the object to construct
  @return a pointer to a newly constructed object
  */

  virtual gridCoreObject * makeObject (const std::string &objectName) = 0;
  /** @brief prepare to make a certain number of objects
  *@param[in] count  the number of object to prepare for
  @param[in] obj a parent to assign the object to
  */
  virtual void prepObjects (count_t /*count*/, gridCoreObject * /*obj*/);
  /** @brief get the number of unused prepped objects
  @return the number of prepped objects
  */
  virtual count_t remainingPrepped () const;
  /** @brief destructor*/
  virtual ~objectFactory ();
};
//component factory is a template class that inherits from cFactory to actually to the construction of a specific object


typedef std::map<std::string, objectFactory *> cMap;

/** @brief a componentfactory containing a mapping of specific object factories for a specific component
*/
class componentFactory
{
public:
  std::string  name;  //!< name of the component
  componentFactory ()
  {
  }
  componentFactory (const std::string componentName);
  ~componentFactory ();
  stringVec getTypeNames ();
  gridCoreObject * makeObject (const std::string &type,const std::string &objectName);
  gridCoreObject * makeObject (const std::string &type);
  gridCoreObject * makeObject ();
  void registerFactory (std::string typeName, objectFactory *oFac);
  void registerFactory (objectFactory *oFac);
  void setDefault (const std::string &type);
  bool isValidType (const std::string &typeName) const;
  objectFactory * getFactory (const std::string &typeName);
protected:
  cMap m_factoryMap;
  std::string m_defaultType;

};

//create a high level object factory for the coreObject class
typedef std::map<std::string, std::shared_ptr<componentFactory> > fMap;
/** @brief central location for builing objects and storing factories for making all the gridDyn component
 core object Factory class  intended to be a singleton it contains a map from strings to typeFactories
*/
class coreObjectFactory
{
public:
  ~coreObjectFactory ();

  /** @brief get a ahared pointer to the core object factory*/
  static std::shared_ptr<coreObjectFactory> instance ();

  /** @brief register a type factory with the coreObjectFactory
  @param[in] name the string identifier to the factory
  @param[in] tf the type factory to place in the map
  */
  void registerFactory (const std::string  name, std::shared_ptr<componentFactory> tf);

  /** @brief register a type factory with the coreObjectFactory
  gets the name to use in the mapping from the type factory itself
  @param[in] tf the type factory to place in the map
  */
  void registerFactory (std::shared_ptr<componentFactory> tf);

  /** @brief get a listing of the factory names*/
  stringVec getFactoryNames ();

  /** @brief get a listing of the type names available for a given factory*/
  stringVec getTypeNames (const std::string &fname);

  /** @brief create an object from a given objectType and typeName
  @param[in] obType  the name of the category of objects
  @param[in] typeName  the specific type to create
  @return the created gridCoreObject */
  gridCoreObject * createObject (const std::string &componentName, const std::string &typeName);

  /** @brief create an object from a given objectType and typeName
  @param[in] obType  the name of the category of objects
  @param[in] typeName  the specific type to create
  @param[in] objName  the name of the object to create
  @return the created gridCoreObject */
  gridCoreObject * createObject (const std::string &componentName, const std::string &typeName, const std::string &objName);

  /** @brief get a specific type factory
  @param[in] fname the name of the typeFactory to get
  @return a shared pointer to a specfic type Factory
  */
  std::shared_ptr<componentFactory> getFactory (const std::string &componentName);

  /** @brief check if a specific object category is valid*/
  bool isValidObject (const std::string &componentType);

  /** @brief check if a specific type name is valid for a specific category of objects*/
  bool isValidType (const std::string &component, const std::string &typeName);

  /** @brief prepare a number of objects for use later so they can all be constructed at once
  @param[in] obType the category of Object to create
  @param[in] typeName the specific type of object in reference
  @param[in] numObjects  the number of objects to preallocate
  @param[in] obj the object to reference as the owner responsbile for deleting the container
  */
  void prepObjects (const std::string &component, const std::string &typeName, count_t numObjects, gridCoreObject *obj);

  /** @brief prepare a number of objects for use later so they can all be constructed at once of the default type for a given container
  @param[in] obType the category of Object to create
  @param[in] numObjects  the number of objects to preallocate
  @param[in] obj the object to reference as the owner responsbile for deleting the container
  */
  void prepObjects (const std::string &component, count_t numObjects, gridCoreObject *obj);
private:
  coreObjectFactory ();

  fMap m_factoryMap;  //!< the main map from string to the typeFactory

};

#endif