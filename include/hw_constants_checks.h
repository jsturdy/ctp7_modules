/*! \file
 *  \brief Header containing helper functions to check the hardware related constants.
 *  \author Laurent Pétré <lpetre@ulb.ac.be>
 */

#ifndef HW_CONSTANTS_CHECKS_H
#define HW_CONSTANTS_CHECKS_H

#include "hw_constants.h"
#include "wiscRPCMsg.h"
#include <sstream>

/*!
  \brief This namespace holds checks for constants related to the AMC
 */
namespace amc {
  inline bool isValidOptoHybrid(uint8_t const ohN, uint8_t const ohMax) {
    if (ohN >= ohMax) {
      std::stringstream errmsg;
      errmsg << "The ohN parameter supplied (" << static_cast<uint32_t>(ohN)
             << ") exceeds the number of OH's supported by the CTP7 ("
             << ohMax << ").";
      throw std::runtime_error(errmsg.str());
      return false; // FIXME, better?
    }
    return true;
  }
}

/*!
  \brief This namespace holds checks for constants related to the OptoHybrid.
 */
namespace oh {
  inline bool isValidVFAT(uint8_t const vfatN) {
    if (vfatN >= oh::VFATS_PER_OH) {
      std::stringstream errmsg;
      errmsg << "The vfatN parameter supplied (" << static_cast<uint32_t>(vfatN)
             << ") exceeds the number of VFAT's per OH ("
             << oh::VFATS_PER_OH << ").";
      throw std::runtime_error(errmsg.str());
      return false; // FIXME, better?
    }
    return true;
  }

  inline bool isValidGBT(uint8_t const gbtN) {
    if (gbtN >= oh::GBTS_PER_OH) {
      std::stringstream errmsg;
      errmsg << "The gbtN parameter supplied (" << gbtN
             << ") exceeds the number of GBT's per OH ("
             << oh::GBTS_PER_OH << ").";
      throw std::runtime_error(errmsg.str());
      return false; // FIXME, better?
    }
    return true;
  }
}

/*!
  \brief This namespace holds checks for constants related to the VFAT.
 */

namespace vfat {
}

/*!
  \brief This namespace holds checks for constants related to the GBT.
 */
namespace gbt {
    /*! // FIXME, better name is isValidPhase
     *  \brief This function checks the phase parameter validity.
     *  \param[in, out] response Pointer to the RPC response object.
     *  \param[in] phase Phase value to check.
     *  \return Returns `false` in case of success; `true` in case of error. The precise error is logged and written to the `error` RPC key.
     */
    inline bool checkPhase(wisc::RPCMsg *response, uint8_t const phase) {
        if (phase < gbt::PHASE_MIN)
            EMIT_RPC_ERROR(response, stdsprintf("The phase parameter supplied (%hhu) is smaller than the minimal phase (%hhu).", phase, gbt::PHASE_MIN), true)
        if (phase > gbt::PHASE_MAX)
            EMIT_RPC_ERROR(response, stdsprintf("The phase parameter supplied (%hhu) is bigger than the maximal phase (%hhu).", phase, gbt::PHASE_MAX), true)
        return false;
    }
}

#endif
