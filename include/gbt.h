/*! \file
 *  \brief RPC module for GBT methods
 *  \author Laurent Pétré <lpetre@ulb.ac.be>
 */

#ifndef GBT_H
#define GBT_H

#include "hw_constants.h"

#include "utils.h"

/*! \brief Scan the GBT phases of one OptoHybrid.
 *  \param[in] request RPC response message
 *  \param[out] response RPC response message
 *
 *  The method expects the following RPC keys :
 *  - `word ohN` : OptoHybrid index number.
 *  - `word N` : The number of times the scan must performed.
 *  - `word phaseMin` : Lowest phase to scan (min = 0).
 *  - `word phaseMax` : Highest phase to scan (max = 15).
 *  - `word phaseStep` : Step to scan the phases.
 *
 *  The method returns the following RPC keys :
 *  - `word_array OHX.VFATY` : Array of word containing the results of the scans. See details of the `scanGBTPhase` function for a description of the words.
 *  - `string error` : If an error occurs, this keys exists and contains the error message.
 */
void scanGBTPhases(const RPCMsg *request, RPCMsg *response);

/*! \brief Local callable version of `scanGBTPhase`.
 *  \param[in, out] la Local arguments structure.
 *  \param[in] ohN OptoHybrid index number.
 *  \param[in] N The number of times the scan must performed.
 *  \param[in] phaseMin Lowest phase to scan (min = 0).
 *  \param[in] phaseMax Highest phase to scan (max = 15).
 *  \param[in] phaseStep Step to scan the phases.
 *  \return Returns `false` in case of success; `true` in case of error. The precise error is logged and written to the `error` RPC key.
 *
 *  \detail
 *
 *  The scan seeks for valid RX phases for the VFAT's of one OptoHybrid. A phase is considered valid when `LINK_GOOD = 1`, `SYNC_ERR_CNT = 0` and `CFG_RUN != 0xdeaddead`. In order to improve the reliability of the scan, it is repeated `N` times.
 *
 *  The results are returned as RPC keys named `OHX.VFATY` where X is the index of the OptoHyrid and Y the index of the VFAT within this OptoHybrid.
 *  Each key is a word array of 16 elements for the 16 possible phases ordered from phase 0 to phase 15.
 *  Each word is the number of time the scan was "good" out of the total number of scan requested, N.
 *
 */
bool scanGBTPhasesLocal(localArgs *la, const uint32_t ohN, const uint32_t N = 1, const uint8_t phaseMin = gbt::PHASE_MIN, const uint8_t phaseMax = gbt::PHASE_MAX, const uint8_t phaseStep = 1);

/*! \brief Write the GBT configuration of one OptoHybrid.
 *  \param[in] request RPC response message.
 *  \param[out] response RPC response message.
 *
 *  The method expects the following RPC keys :
 *  - `word ohN` : OptoHybrid index number.
 *  - `word gbtN` : Index of the GBT to configure.
 *                  There 3 GBT's per OptoHybrid in the GE1/1 chambers.
 *  - `binarydata config` : Configuration blob of the GBT.
 *                          This is a 366 element long vector of the registers to write.
 *                          The registers are sorted from address 0 to address 365.
 *
 *  The method returns the following RPC keys :
 *  - `string error` : If an error occurs, this keys exists and contains the error message.
 */
void writeGBTConfig(const RPCMsg *request, RPCMsg *response);

/*! \brief Write the GBT configuration of one OptoHybrid.
 *  \param[in] request RPC response message.
 *  \param[out] response RPC response message.
 *
 *  The method expects the following RPC keys :
 *  - `word ohN` : OptoHybrid index number.
 *    EITHER 3 blobs, one for each GBT, 366 8-bit words in an array of 32-bit words (92 words each, including padding)
 *  - `binarydata gbt0` : Configuration blob of GBT0
 *  - `binarydata gbt1` : Configuration blob of GBT1
 *  - `binarydata gbt2` : Configuration blob of GBT2
 *    OR
 *  - `binarydata config` : Configuration blob of all GBTs, 3x366 8-bit words
                            A 3x92 32-bit word array
 *
 *  The method returns the following RPC keys :
 *  - `string error` : If an error occurs, this keys exists and contains the error message.
 */
void writeAllGBTConfigs(const RPCMsg *request, RPCMsg *response);

/*! \brief Local callable version of `writeGBTConfig`
 *  \param[in, out] la Local arguments structure.
 *  \param[in] ohN OptoHybrid index number.
 *  \param[in] gbtN Index of the GBT to write. There 3 GBT's per OptoHybrid in the GE1/1 chambers.
 *  \param[in] config Configuration blob of the GBT.
 *             This is a 366 elements long array whose each element is the value of one register sorted from address 0 to address 365.
 *  \return Returns `false` in case of success; `true` in case of error.
 *          The precise error is logged and written to the `error` RPC key.
 */
bool writeGBTConfigLocal(localArgs *la, const uint32_t ohN, const uint32_t gbtN, const gbt::config_t &config);

/*! \brief Write the phase of a single VFAT.
 *  \param[in] request RPC response message.
 *  \param[out] response RPC response message.
 *
 *  The method expects the following RPC keys :
 *  - `word ohN` : OptoHybrid index number.
 *  - `word vfatN` : Index of the VFAT to configure. There 24 VFAT's per OptoHybrid in the GE1/1 chambers.
 *  - `word phase` : Phase value to write. The values span from 0 to 15.
 *
 *  The method returns the following RPC keys :
 *  - `string error` : If an error occurs, this keys exists and contains the error message.
 */
void writeGBTPhase(const RPCMsg *request, RPCMsg *response);

/*! \brief Local callable version of `writeGBTPhase`
 *  \param[in, out] la Local arguments structure.
 *  \param[in] ohN OptoHybrid index number.
 *  \param[in] vfatN VFAT index of which the phase is changed.
 *  \param[in] phase Phase value to write. Minimal phase is 0 and maximal phase is 15.
 *  \return Returns `false` in case of success; `true` in case of error.
 *          The precise error is logged and written to the `error` RPC key.
 */
bool writeGBTPhaseLocal(localArgs *la, const uint32_t ohN, const uint32_t vfatN, const uint8_t phase);

/*! \brief This function writes a single register in the given GBT of the given OptoHybrid.
 *  \param[in, out] la Local arguments structure.
 *  \param[in] ohN OptoHybrid index number.
 *             Warning : the value of this parameter is not checked because of the cost of a register access.
 *  \param[in] gbtN Index of the GBT to write. There 3 GBT's per OptoHybrid in the GE1/1 chambers.
 *  \param[in] address GBT register address to write. The highest writable address is 365.
 *  \param[in] value Value to write to the GBT register.
 *  \return Returns `false` in case of success; `true` in case of error.
 *          The precise error is logged and written to the `error` RPC key.
 */
bool writeGBTRegLocal(localArgs *la, const uint32_t ohN, const uint32_t gbtN, const uint16_t address, const uint8_t value);

/*!
 * \brief Write the specified register on the selected GBT of the specified OptoHybrid.
 *  \param[in] request RPC response message.
 *  \param[out] response RPC response message.
 *
 *  The method expects the following RPC keys :
 *  - `word ohN` : OptoHybrid index number.
 *  - `word gbtN` : Index of the GBT to configure. There 3 GBT's per OptoHybrid in the GE1/1 chambers.
 *  - `word addr` : Address of the register to write
 *  - `word val` : Value to write to the register
 *
 *  The method returns the following RPC keys :
 *  - `string error` : If an error occurs, this keys exists and contains the error message.
 */
void writeGBTReg(const RPCMsg *request, RPCMsg *response);

/*!
 * \brief Read the specified GBT configuration of one OptoHybrid.
 *  \param[in] request RPC response message.
 *  \param[out] response RPC response message.
 *
 *  The method expects the following RPC keys :
 *  - `word ohN` : OptoHybrid index number.
 *  - `word gbtN` : Index of the GBT to configure. There 3 GBT's per OptoHybrid in the GE1/1 chambers.
 *
 *  The method returns the following RPC keys :
 *  - `binarydata config` : Configuration blob of the GBT. This is a 366 element long vector of the registers read.
                            The registers are sorted from address 0 to address 365.
 *  - `string error` : If an error occurs, this keys exists and contains the error message.
 */
void readGBTConfig(const RPCMsg *request, RPCMsg *response);

/*! \brief Local callable version of `readGBTConfig`
 *  \param[in, out] la Local arguments structure.
 *  \param[in] ohN OptoHybrid index number.
 *  \param[in] gbtN Index of the GBT to read. There 3 GBT's per OptoHybrid in the GE1/1 chambers.
 *  \param[in/out] config Configuration blob of the GBT.
 *                 This is a 366 elements long array storing the values of each GBT register from 0 to 365
 *  \return Returns `false` in case of success; `true` in case of error.
 *          The precise error is logged and written to the `error` RPC key.
 */
bool readGBTConfigLocal(localArgs *la, const uint32_t ohN, const uint32_t gbtN, gbt::config_t &config);

/*! \brief This function reads a single register in the given GBT of the given OptoHybrid.
 *  \param[in, out] la Local arguments structure.
 *  \param[in] ohN OptoHybrid index number. Warning : the value of this parameter is not checked because of the cost of a register access.
 *  \param[in] gbtN Index of the GBT to read. There 3 GBT's per OptoHybrid in the GE1/1 chambers.
 *  \param[in] address GBT register address to read. The highest writable address is 365.
 *  \return Returns `false` in case of success; `true` in case of error. The precise error is logged and written to the `error` RPC key.
 */
uint8_t readGBTRegLocal(localArgs *la, const uint32_t ohN, const uint32_t gbtN, const uint16_t address);

/*!
 * \brief Read the specified register on the selected GBT of the specified OptoHybrid.
 *  \param[in] request RPC response message.
 *  \param[out] response RPC response message.
 *
 *  The method expects the following RPC keys :
 *  - `word ohN` : OptoHybrid index number.
 *  - `word gbtN` : Index of the GBT to configure. There 3 GBT's per OptoHybrid in the GE1/1 chambers.
 *  - `word addr` : Address of the register to read
 *
 *  The method returns the following RPC keys :
 *  - `uint32_t value` : Value of the register
 *  - `string error` : If an error occurs, this keys exists and contains the error message.
 */
void readGBTReg(const RPCMsg *request, RPCMsg *response);

#endif
