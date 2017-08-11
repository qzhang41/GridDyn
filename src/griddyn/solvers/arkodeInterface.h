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

#ifndef _ARKODE_SOLVER_INTERFACE_H_
#define _ARKODE_SOLVER_INTERFACE_H_

#include "sundialsInterface.h"

namespace griddyn
{
namespace solvers
{
/** @brief solverInterface interfacing to the SUNDIALS arkode solver
 */
class arkodeInterface : public sundialsInterface
{
  public:
    count_t icCount = 0;

  private:
    matrixDataSparse<double> a1;  //!< array structure for holding the Jacobian information

    std::vector<double> tempState;  //!<temporary holding location for a state vector
    double maxStep = -1.0;
    double minStep = -1.0;
    double step = 0.0;

  public:
    /** @brief constructor*/
    explicit arkodeInterface (const std::string &objName = "arkode");
    /** @brief alternate constructor
    @param[in] gds  the gridDynSimulation object to connect to
    @param[in] sMode the solverMode to solve For
    */
    arkodeInterface (gridDynSimulation *gds, const solverMode &sMode);
    /** @brief destructor*/
    ~arkodeInterface ();

    virtual std::shared_ptr<solverInterface>
    clone (std::shared_ptr<solverInterface> si = nullptr, bool fullCopy = false) const override;
    virtual void allocate (count_t stateCount, count_t numRoots = 0) override;
    virtual void initialize (coreTime time0) override;
    virtual void setMaxNonZeros (count_t nonZeroCount) override;
    virtual void sparseReInit (sparse_reinit_modes sparseReinitMode) override;
    virtual void getCurrentData () override;
    virtual int solve (coreTime tStop, coreTime &tReturn, step_mode stepMode = step_mode::normal) override;
    virtual void getRoots () override;
    virtual void setRootFinding (count_t numRoots) override;

    virtual void logSolverStats (print_level logLevel, bool iconly = false) const override;
    virtual void logErrorWeights (print_level logLevel) const override;
    virtual void set (const std::string &param, const std::string &val) override;
    virtual void set (const std::string &param, double val) override;
    virtual double get (const std::string &param) const override;
    // declare friend some helper functions
    friend int arkodeFunc (realtype time, N_Vector state, N_Vector dstate_dt, void *user_data);
    friend int arkodeJacDense (long int Neq,
                               realtype time,
                               N_Vector state,
                               N_Vector dstate_dt,
                               DlsMat J,
                               void *user_data,
                               N_Vector tmp1,
                               N_Vector tmp2,
                               N_Vector tmp3);
#ifdef KLU_ENABLE
    friend int arkodeJacSparse (realtype time,
                                N_Vector state,
                                N_Vector dstate_dt,
                                SlsMat J,
                                void *user_data,
                                N_Vector tmp1,
                                N_Vector tmp2,
                                N_Vector tmp3);
#endif  // KLU_ENABLE
    friend int arkodeRootFunc (realtype time, N_Vector state, realtype *gout, void *user_data);

  protected:
    void loadMaskElements ();
};

}//namespace solvers
}//namespace griddyn

#endif