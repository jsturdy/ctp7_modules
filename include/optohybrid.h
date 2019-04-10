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


/*!
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

/*!
 *  \brief Configures V2b FW scan module
 *  \param request RPC response message
 *  \param response RPC response message
 */
void configureScanModule(const RPCMsg *request, RPCMsg *response);

/*!
 *  \brief Local callable version of getUltraScanResults
 *  \param la Local arguments structure
 *  \param outData Pointer to output data array
 *  \param nevts Number of events per scan point
 *  \param dacMin Minimal value of scan variable
 *  \param dacMax Maximal value of scan variable
 *  \param dacStep Scan variable change step
 */
void getUltraScanResultsLocal(localArgs *la, uint32_t *outData, uint32_t ohN, uint32_t nevts, uint32_t dacMin, uint32_t dacMax, uint32_t dacStep);

/*!
 *  \brief Returns results of an ultra scan routine
 *  \param request RPC response message
 *  \param response RPC response message
 */
void getUltraScanResults(const RPCMsg *request, RPCMsg *response);

/*!
 *  \brief Local callable version of printScanConfiguration
 *  \param la Local arguments structure
 *  \param ohN Optohybrid optical link number
 *  \param useUltra Set to 1 in order to use the ultra scan
 */
void printScanConfigurationLocal(localArgs * la, uint32_t ohN, bool useUltra);

/*!
 *  \brief Prints V2b FW scan module configuration
 *  \param request RPC response message
 *  \param response RPC response message
 */
void printScanConfiguration(const RPCMsg *request, RPCMsg *response);

/*!
 *  \brief Local callable version of startScanModule
 *  \param la Local arguments structure
 *  \param ohN Optohybrid optical link number
 *  \param useUltra Set to 1 in order to use the ultra scan
 */
void startScanModuleLocal(localArgs * la, uint32_t ohN, bool useUltra);

/*!
 *  \brief Starts V2b FW scan module
 *  \param request RPC response message
 *  \param response RPC response message
 */
void startScanModule(const RPCMsg *request, RPCMsg *response);

/*!
 *  \brief Local callable version of statusOH
 *  \param la Local arguments structure
 *  \param ohEnMask Bit mask of enabled optical links
 */
void statusOHLocal(localArgs * la, uint32_t ohEnMask);

/*!
 *  \brief Returns a list of the most important monitoring registers of optohybrids
 *  \param request RPC response message
 *  \param response RPC response message
 */
void statusOH(const RPCMsg *request, RPCMsg *response);

/*! FIXME INCOMPLETE
 *  \brief Local callable version of readOptoHybridConfig
 *  \param la Local arguments structure
 *  \param ohN Optohybrid optical link number
 *  \param config configuration to read from the OptoHybrid specified
 */
size_t readOptoHybridConfigLocal(localArgs *la, uint32_t const& ohN, uint32_t* config);

/*! FIXME INCOMPLETE
 *  \brief RPC callback to read OptoHybrid configuration
 *  \param request RPC response message
 *  \param response RPC response message
 */
void readOptoHybridConfig(const RPCMsg *request, RPCMsg *response);

/*! FIXME INCOMPLETE
 *  \brief Local callable version of writeOptoHybridConfig
 *  \param la Local arguments structure
 *  \param ohN Optohybrid optical link number
 *  \param config configuration to write to the OptoHybrid specified
 */
void writeOptoHybridConfigLocal(localArgs *la, uint32_t const& ohN, uint32_t* config);

/*! FIXME INCOMPLETE
 *  \brief RPC callback to write OptoHybrid configuration
 *  \param request RPC response message
 *  \param response RPC response message
 */
void writeOptoHybridConfig(const RPCMsg *request, RPCMsg *response);

#endif
