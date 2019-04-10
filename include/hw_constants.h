/*! \file
 *  \brief Header containing the hardware related constants.
 *  \author Laurent Pétré <lpetre@ulb.ac.be>
 */

#ifndef HW_CONSTANTS_H
#define HW_CONSTANTS_H

#ifndef GEM_VARIANT
#error You must define the GEM_VARIANT constant.
#endif

#include <stdint.h>
#include <array>

/*! \brief This namespace hold the constants related to the AMC.
 */
namespace amc {
    /*! \brief GE1/1 specific namespace.
     */
    namespace ge11 {
        constexpr uint32_t OH_PER_AMC = 12;    ///< The number of OptoHybrids per AMC.
    }

    using namespace GEM_VARIANT;
}

/*! \brief This namespace hold the constants related to the OptoHybrid.
 */
namespace oh {
    /*! \brief GE1/1 specific namespace.
     */
    namespace ge11 {
        constexpr uint32_t VFATS_PER_OH     = 24;    ///< The number of VFAT's per OptoHybrid.
        constexpr size_t OH_SINGLE_RAM_SIZE = 2*100; ///< Per-OH RAM size: for GE1/1 100 32-bit words of configuration per OH plus the corresponding OH local address
    }

    using namespace GEM_VARIANT;
}

/*!
  \brief This namespace hold the constants related to the OptoHybrid.
 */
namespace vfat {
    /*!
      \brief GE1/1 specific namespace.
     */
    constexpr uint16_t CH_CFG_SIZE  = 128; ///< Size of the VFAT channel configuration address space (consecutive 16-bit registers)
    constexpr uint16_t GLB_CFG_SIZE = 17+2; ///< Size of the VFAT global configuration address space (consecutive 16-bit registers)
    constexpr uint16_t CFG_SIZE = CH_CFG_SIZE+GLB_CFG_SIZE; ///< Size of the VFAT configuration address space (consecutive 16-bit registers)
    // FIXME extra 2 registers above

    typedef std::array<uint16_t, CFG_SIZE> config_t; ///< This type defines a VFAT configuration blob.

    constexpr uint8_t CH_MIN = 0;   ///< Minimum channel number
    constexpr uint8_t CH_MAX = 128; ///< Maximum channel number 127?128?129
    constexpr uint8_t CHANNELS_PER_VFAT = 128; ///< Total number of channels on a VFAT

    namespace ge11 {
        constexpr size_t VFAT_SINGLE_RAM_SIZE = 74; ///< Per-VFAT RAM size: for GE1/1 24 VFATs per OH, 74 32-bit words of configuration per VFAT (147 16-bit VFAT configurations -> 32-bit words + padding)
    }

    using namespace GEM_VARIANT;
}

/*!
  \brief This namespace hold the constants related to the GBT.
 */
namespace gbt {

    constexpr uint16_t CONFIG_SIZE = 366; ///< The size of the GBT configuration address space. The corresponding addresses span from 0 to 365.

    typedef std::array<uint8_t, CONFIG_SIZE> config_t; ///< This type defines a GBT configuration blob.

    constexpr uint8_t PHASE_MIN = 0;  ///< Minimal phase for the elink RX GBT phase.
    constexpr uint8_t PHASE_MAX = 15; ///< Maximal phase for the elink RX GBT phase.

    constexpr uint8_t ELINKS_PER_GBT      = 10; ///< Number of elinks per GBTx chip
    constexpr uint8_t REGISTERS_PER_ELINK = 3; ///< Number registers per GBTx elink

    /*!
      \brief GE1/1 specific namespace.
     */
    namespace ge11 {
        constexpr uint32_t GBTS_PER_OH       = 3;  ///< The number of GBT's per OptoHybrid (should be part of the oh namespace, as VFATS_PER_OH)
        constexpr size_t GBT_SINGLE_RAM_SIZE = 92; ///< Per-GBT RAM size: for GE1/1 3 GBTx per OH, 92 32-bit words of configuration per GBT (366 8-bit GBT configurations -> 32-bit words + padding)
    }

    using namespace GEM_VARIANT;

    /*! \brief Mappings between elinks, GBT index and VFAT index.
     */
    namespace elinkMappings {
        namespace ge11 {
            /*!
              \brief Mapping from VFAT index to GBT index.
             */
            constexpr std::array<uint32_t, oh::VFATS_PER_OH> VFAT_TO_GBT {
                { 1, 1, 1, 1, 1, 1, 1, 0, 1, 2, 2, 2, 0, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 0 }
            }; ///< Mapping from VFAT index to GBT index.

            /*!
              \brief Mapping from VFAT index to the elink of its corresponding GBT.
             */
            constexpr std::array<uint8_t, oh::VFATS_PER_OH> VFAT_TO_ELINK {
                { 5, 9, 2, 3, 1, 8, 6, 6, 4, 1, 5, 4, 3, 2, 1, 0, 7, 8, 6, 7, 2, 3, 9, 8 }
            }; ///< Mapping from VFAT index to the elink of its corresponding GBT.

            /*!
              \brief Mapping from elink index to the 3 registers addresses in the GBT.
            */
            constexpr std::array<std::array<uint16_t, REGISTERS_PER_ELINK>, ELINKS_PER_GBT> ELINK_TO_REGISTERS {
                { {69, 73, 77}, {67, 71, 75}, {93, 97, 101}, {91, 95, 99}, {117, 121, 125}, {115, 119, 123}, {141, 145, 149}, {139, 143, 147}, {165, 169, 173}, {163, 167, 171} }
            }; ///< Mapping from elink index to the 3 registers addresses in the GBT.
        }

        using namespace GEM_VARIANT;
    }
}

#endif
