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

#ifndef READERHELPER_H_
#define READERHELPER_H_
#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>


#define READER_DEFAULT_PRINT READER_NO_PRINT

#define LEVELPRINT(LEVEL,X) if (LEVEL <= readerConfig::printMode) (std::cout << X << '\n')


#define WARNPRINT(LEVEL, X) if (LEVEL <= readerConfig::warnMode) (++readerConfig::warnCount,std::cout << "WARNING(" << readerConfig::warnCount << "): " << X << '\n')

//helper function for grabbing parameters and attributes from an xml file

class gridParameter;
class readerInfo;
class gridCoreObject;
class basicReaderInfo;

typedef std::vector<std::string> stringVec;

void paramStringProcess (gridParameter *param, readerInfo *ri);

double convertBV (std::string &bv);

typedef std::vector<std::vector<double> > mArray;


double interpretString (std::string command, readerInfo *ri);


// NOTE:PT I am leaving these as size_t since they are part of file reading and text location types and spread across multiple files
void readMatlabArray (const std::string &text, size_t start, mArray &matA);
bool readMatlabArray (const std::string &Name, const std::string &text, mArray &matA);
stringVec readMatlabCellArray (const std::string &text, size_t start);
void removeMatlabComments (std::string &text);

void loadPSAT (gridCoreObject *parentObject, const std::string &filetext, const basicReaderInfo &bri);
void loadMatPower (gridCoreObject *parentObject, const std::string &filetext, std::string basename, const basicReaderInfo &bri);
void loadMatDyn (gridCoreObject *parentObject, const std::string &filetext, const basicReaderInfo &bri);
void loadMatDynEvent (gridCoreObject *parentObject, const std::string &filetext, const basicReaderInfo &bri);

//TODO::PT replace the calls of these functions with those from stringOps.h

inline void paramRead (const std::string &V, double &val, double def = 0.0)
{
  size_t pos;
  try
    {
      val = std::stod (V, &pos);

      while (pos < V.length ())
        {
          if (!(isspace (V[pos])))
            {
              val = def;
              break;
            }
          ++pos;
        }
    }
  catch (std::invalid_argument)
    {
      val = def;
    }
}

inline void paramRead (const std::string &V, int &val, int def = 0)
{
  size_t pos;
  try
    {
      val = std::stoi (V, &pos);

      while (pos < V.length ())
        {
          if (!(isspace (V[pos])))
            {
              val = def;
              break;
            }
          ++pos;
        }
    }
  catch (std::invalid_argument)
    {
      val = def;
    }
}

#endif
