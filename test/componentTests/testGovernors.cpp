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

#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include "gridDyn.h"
#include "gridDynFileInput.h"
#include "testHelper.h"
#include "objectFactory.h"
#include "simulation/diagnostics.h"

#include "generators/gridDynGenerator.h"
#include <cmath>
//test case for gridCoreObject object


#define GOVERNOR_TEST_DIRECTORY GRIDDYN_TEST_DIRECTORY "/governor_tests/"

BOOST_FIXTURE_TEST_SUITE (governor_tests, gridDynSimulationTestFixture)

BOOST_AUTO_TEST_CASE (gov_stability_test)
{
  std::string fname = std::string (GOVERNOR_TEST_DIRECTORY "test_gov_stability.xml");

  gds->resetObjectCounters ();
  gds = static_cast<gridDynSimulation *> (readSimXMLFile (fname));
  gridDynGenerator *gen = static_cast<gridDynGenerator *> (gds->findByUserID ("gen", 2));

  auto cof = coreObjectFactory::instance ();
  gridCoreObject *obj = cof->createObject ("governor", "basic");

  gen->add (obj);

  int retval = gds->dynInitialize ();
  BOOST_CHECK_EQUAL (retval, 0);
  BOOST_REQUIRE (gds->currentProcessState () == gridDynSimulation::gridState_t::DYNAMIC_INITIALIZED);

  BOOST_CHECK_EQUAL(runJacobianCheck(gds, cDaeSolverMode), 0);
  gds->run (0.005);
  BOOST_CHECK_EQUAL(runJacobianCheck(gds, cDaeSolverMode), 0);

  gds->run (400);
  BOOST_REQUIRE (gds->currentProcessState () == gridDynSimulation::gridState_t::DYNAMIC_COMPLETE);
  std::vector<double> st = gds->getState ();
  gds->run (500);
  gds->saveRecorders ();
  std::vector<double> st2 = gds->getState ();

  //check for stability
  BOOST_REQUIRE_EQUAL (st.size (), st2.size ());
  size_t kk;
  int ncnt = 0;
  double a0 = st2[0];
  for (kk = 0; kk < st.size (); ++kk)
    {
      if (std::abs (st[kk] - st2[kk]) > 0.0001)
        {
          if (std::abs (st[kk] - st2[kk] + a0) > 0.005 * ((std::max)(st[kk],st2[kk])))
            {
              printf ("state[%zd] orig=%f new=%f\n", kk, st[kk], st2[kk]);
              ncnt++;
            }

        }
    }
  BOOST_CHECK_EQUAL (ncnt, 0);

}


BOOST_AUTO_TEST_SUITE_END ()
