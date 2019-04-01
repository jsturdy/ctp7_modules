/*! \file optohybrid.h
 *  \brief RPC module for optohybrid methods
 *  \author Mykhailo Dalchenko <mykhailo.dalchenko@cern.ch>
 *  \author Cameron Bravo <cbravo135@gmail.com>
 *  \author Brin Dorney <brian.l.dorney@cern.ch>
 */
#ifndef OPTOHYBRID_H
#define OPTOHYBRID_H

#include "utils.h"
#include "vfat_parameters.h"
#include <unistd.h>

/*! \fn void biasAllVFATsLocal(localArgs * la, uint32_t ohN, uint32_t mask = 0xFF000000)
 *  \brief Local callable. Sets default values to VFAT parameters. VFATs will remain in sleep mode
 *  \param la Local arguments structure
 *  \param ohN Optohybrid optical link number (string)
 *  \param mask VFAT mask. Default: no chips will be masked
 */
void biasAllVFATsLocal(localArgs * la, uint32_t ohN, uint32_t mask = 0xFF000000);

/*! \fn void broadcastWriteLocal(localArgs * la, uint32_t ohN, std::string regName, uint32_t value, uint32_t mask = 0xFF000000)
 *  \brief Local callable version of broadcastWrite
 *  \param la Local arguments structure
 *  \param ohN Optohybrid optical link number
 *  \param regName Register name
 *  \param value Register value to write
 *  \param mask VFAT mask. Default: no chips will be masked
 *  \return Bitmask of sync'ed VFATs
 */
void broadcastWriteLocal(localArgs * la, uint32_t ohN, std::string regName, uint32_t value, uint32_t mask = 0xFF000000);
/*! \fn void broadcastWrite(const RPCMsg *request, RPCMsg *response)
 *  \brief Performs broadcast write a given regiser on all the VFAT chips of a given optohybrid
 *  \param request RPC response message
 *  \param response RPC response message
 */
void broadcastWrite(const RPCMsg *request, RPCMsg *response);

/*! \fn void broadcastReadLocal(localArgs * la, uint32_t ohN, std::string regName, uint32_t mask = 0xFF000000)
 *  \brief Local callable version of broadcastRead
 *  \param la Local arguments structure
 *  \param outData pointer to the results of the broadcast read
 *  \param ohN Optohybrid optical link number
 *  \param regName Register name
 *  \param mask VFAT mask. Default: no chips will be masked
 *  \return Bitmask of sync'ed VFATs
 */
void broadcastReadLocal(localArgs * la, uint32_t *outData, uint32_t ohN, std::string regName, uint32_t mask = 0xFF000000);

/*! \fn void broadcastRead(const RPCMsg *request, RPCMsg *response)
 *  \brief Performs broadcast read of a given regiser on all the VFAT chips of a given optohybrid
 *  \param request RPC response message
 *  \param response RPC response message
 */
void broadcastRead(const RPCMsg *request, RPCMsg *response);

/*! \fn configureScanModuleLocal(localArgs * la, uint32_t ohN, uint32_t vfatN, uint32_t scanmode, bool useUltra, uint32_t mask, uint32_t ch, uint32_t nevts, uint32_t dacMin, uint32_t dacMax, uint32_t dacStep)
 *  \brief Local callable version of configureScanModule
 *
     *     Configure the firmware scan controller
     *      mode: 0 Threshold scan
     *            1 Threshold scan per channel
     *            2 Latency scan
     *            3 s-curve scan
     *            4 Threshold scan with tracking data
     *      vfat: for single VFAT scan, specify the VFAT number
     *            for ULTRA scan, specify the VFAT mask
 *
 *  \param la Local arguments structure
 *  \param ohN Optohybrid optical link number
 *  \param vfatN VFAT chip position
 *  \param scammode Scan mode
 *  \param useUltra Set to 1 in order to use the ultra scan
 *  \param mask VFAT chips mask
 *  \param ch Channel to scan
 *  \param nevts Number of events per scan point
 *  \param dacMin Minimal value of scan variable
 *  \param dacMax Maximal value of scan variable
 *  \param dacStep Scan variable change step
 */
void configureScanModuleLocal(localArgs * la, uint32_t ohN, uint32_t vfatN, uint32_t scanmode, bool useUltra, uint32_t mask, uint32_t ch, uint32_t nevts, uint32_t dacMin, uint32_t dacMax, uint32_t dacStep);

/*! \fn void configureScanModule(const RPCMsg *request, RPCMsg *response)
 *  \brief Configures V2b FW scan module
 *  \param request RPC response message
 *  \param response RPC response message
 */
void configureScanModule(const RPCMsg *request, RPCMsg *response);

/*! \fn void configureVFATs(const RPCMsg *request, RPCMsg *response)
 *  \brief Configures VFAT chips (V2B only). Calls biasVFATs, loads VT1 and TRIMDAC values from the config files
 *  \param request RPC response message
 *  \param response RPC response message
 */
void configureVFATs(const RPCMsg *request, RPCMsg *response);

/*! \fn void getUltraScanResultsLocal(localArgs *la, uint32_t *outData, uint32_t ohN, uint32_t nevts, uint32_t dacMin, uint32_t dacMax, uint32_t dacStep)
 *  \brief Local callable version of getUltraScanResults
 *  \param la Local arguments structure
 *  \param outData Pointer to output data array
 *  \param nevts Number of events per scan point
 *  \param dacMin Minimal value of scan variable
 *  \param dacMax Maximal value of scan variable
 *  \param dacStep Scan variable change step
 */
void getUltraScanResultsLocal(localArgs *la, uint32_t *outData, uint32_t ohN, uint32_t nevts, uint32_t dacMin, uint32_t dacMax, uint32_t dacStep);

/*! \fn void getUltraScanResults(const RPCMsg *request, RPCMsg *response)
 *  \brief Returns results of an ultra scan routine
 *  \param request RPC response message
 *  \param response RPC response message
 */
void getUltraScanResults(const RPCMsg *request, RPCMsg *response);

/*! \fn void loadTRIMDACLocal(localArgs * la, uint32_t ohN, std::string config_file)
 *  \brief Local callable version of loadTRIMDAC
 *  \param la Local arguments structure
 *  \param ohN Optohybrid optical link number
 *  \param config_file Configuration file with trimming parameters
 */
void loadTRIMDACLocal(localArgs * la, uint32_t ohN, std::string config_file);

/*! \fn void loadTRIMDAC(const RPCMsg *request, RPCMsg *response)
 *  \brief Sets trimming DAC parameters for each channel of each chip
 *  \param request RPC response message
 *  \param response RPC response message
 */
void loadTRIMDAC(const RPCMsg *request, RPCMsg *response);

/*! \fn void loadVT1Local(localArgs * la, uint32_t ohN, std::string config_file, uint32_t vt1 = 0x64)
 *  \brief Local callable version of loadVT1
 *  \param la Local arguments structure
 *  \param ohN Optohybrid optical link number
 *  \param config_file Configuration file with VT1 and trim values. Optional (could be supplied as an empty string)
 *  \param vt1. Default: 0x64, used if the config_file is not provided
 *  \return Bitmask of sync'ed VFATs
 */
void loadVT1Local(localArgs * la, uint32_t ohN, std::string config_file, uint32_t vt1 = 0x64);

/*! \fn void loadVT1(const RPCMsg *request, RPCMsg *response)
 *  \brief Sets threshold and trim range for each VFAT2 chip
 *  \param request RPC response message
 *  \param response RPC response message
 */
void loadVT1(const RPCMsg *request, RPCMsg *response);

/*! \fn void printScanConfigurationLocal(localArgs * la, uint32_t ohN, bool useUltra)
 *  \brief Local callable version of printScanConfiguration
 *  \param la Local arguments structure
 *  \param ohN Optohybrid optical link number
 *  \param useUltra Set to 1 in order to use the ultra scan
 */
void printScanConfigurationLocal(localArgs * la, uint32_t ohN, bool useUltra);

/*! \fn void printScanConfiguration(const RPCMsg *request, RPCMsg *response)
 *  \brief Prints V2b FW scan module configuration
 *  \param request RPC response message
 *  \param response RPC response message
 */
void printScanConfiguration(const RPCMsg *request, RPCMsg *response);

/*! \fn void setAllVFATsToRunModeLocal(localArgs * la, uint32_t ohN, uint32_t mask = 0xFF000000)
 *  \brief Local callable. Sets VFATs to run mode
 *  \param la Local arguments structure
 *  \param ohN Optohybrid optical link number (string)
 *  \param mask VFAT mask. Default: no chips will be masked
 */
void setAllVFATsToRunModeLocal(localArgs * la, uint32_t ohN, uint32_t mask = 0xFF000000);

/*! \fn void setAllVFATsToSleepModeLocal(localArgs * la, uint32_t ohN, uint32_t mask = 0xFF000000)
 *  \brief Local callable. Sets VFATs to sleep mode
 *  \param la Local arguments structure
 *  \param ohN Optohybrid optical link number (string)
 *  \param mask VFAT mask. Default: no chips will be masked
 */
void setAllVFATsToSleepModeLocal(localArgs * la, uint32_t ohN, uint32_t mask = 0xFF000000);

/*! \fn void startScanModuleLocal(localArgs * la, uint32_t ohN, bool useUltra)
 *  \brief Local callable version of startScanModule
 *  \param la Local arguments structure
 *  \param ohN Optohybrid optical link number
 *  \param useUltra Set to 1 in order to use the ultra scan
 */
void startScanModuleLocal(localArgs * la, uint32_t ohN, bool useUltra);

/*! \fn void startScanModule(const RPCMsg *request, RPCMsg *response)
 *  \brief Starts V2b FW scan module
 *  \param request RPC response message
 *  \param response RPC response message
 */
void startScanModule(const RPCMsg *request, RPCMsg *response);

/*! \fn void statusOHLocal(localArgs * la, uint32_t ohEnMask)
 *  \brief Local callable version of statusOH
 *  \param la Local arguments structure
 *  \param ohEnMask Bit mask of enabled optical links
 */
void statusOHLocal(localArgs * la, uint32_t ohEnMask);

/*! \fn void statusOH(const RPCMsg *request, RPCMsg *response)
 *  \brief Returns a list of the most important monitoring registers of optohybrids
 *  \param request RPC response message
 *  \param response RPC response message
 */
void statusOH(const RPCMsg *request, RPCMsg *response);

/*! \fn void stopCalPulse2AllChannelsLocal(localArgs *la, uint32_t ohN, uint32_t mask, uint32_t ch_min, uint32_t ch_max)
 *  \brief Local callable version of stopCalPulse2AllChannels
 *  \param la Local arguments structure
 *  \param ohN Optohybrid optical link number
 *  \param mask VFAT mask. Default: no chips will be masked
 *  \param chMin Minimal channel number
 *  \param chMax Maximal channel number
 */
void stopCalPulse2AllChannelsLocal(localArgs *la, uint32_t ohN, uint32_t mask, uint32_t ch_min, uint32_t ch_max);

/*! \fn void stopCalPulse2AllChannels(const RPCMsg *request, RPCMsg *response)
 *  \brief Disables calibration pulse in channels between chMin and chMax
 *  \param request RPC response message
 *  \param response RPC response message
 */
void stopCalPulse2AllChannels(const RPCMsg *request, RPCMsg *response);

/*!
 *  \brief Local callable version of readOptoHybridConfig
 *  \param la Local arguments structure
 *  \param ohN Optohybrid optical link number
 *  \param config configuration to read from the OptoHybrid specified
 */
size_t readOptoHybridConfigLocal(localArgs *la, uint32_t const& ohN, uint32_t* config);

/*!
 *  \brief Disables calibration pulse in channels between chMin and chMax
 *  \param request RPC response message
 *  \param response RPC response message
 */
void readOptoHybridConfig(const RPCMsg *request, RPCMsg *response);

/*!
 *  \brief Local callable version of writeOptoHybridConfig
 *  \param la Local arguments structure
 *  \param ohN Optohybrid optical link number
 *  \param config configuration to write to the OptoHybrid specified
 */
void writeOptoHybridConfigLocal(localArgs *la, uint32_t const& ohN, uint32_t* config);

/*!
 *  \brief Disables calibration pulse in channels between chMin and chMax
 *  \param request RPC response message
 *  \param response RPC response message
 */
void writeOptoHybridConfig(const RPCMsg *request, RPCMsg *response);

#endif
