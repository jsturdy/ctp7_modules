/*! \file vfat3.cpp
 *  \brief RPC module for VFAT3 methods
 *  \author Mykhailo Dalchenko <mykhailo.dalchenko@cern.ch>
 *  \author Cameron Bravo <cbravo135@gmail.com>
 *  \author Brian Dorney <brian.l.dorney@cern.ch>
 */

#include "vfat3.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <iomanip>
#include <memory>

#include "amc.h"
#include "amc/blaster_ram.h"
#include "optohybrid.h"

#include "hw_constants_checks.h"
// #include "hw_constants.h"

#include "reedmuller.h"

uint32_t vfatSyncCheckLocal(localArgs *la, uint32_t ohN)
{
    uint32_t goodVFATs = 0;
    for (uint8_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
        std::stringstream regBase;
        regBase << "GEM_AMC.OH_LINKS.OH" << ohN << ".VFAT" << static_cast<uint32_t>(vfatN);
        bool linkGood       = readReg(la, regBase.str()+".LINK_GOOD");
        uint32_t linkErrors = readReg(la, regBase.str()+".SYNC_ERR_CNT");
        goodVFATs = goodVFATs | ((linkGood && (linkErrors == 0)) << vfatN);
    }

    return goodVFATs;
}

void vfatSyncCheck(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");

    // uint32_t goodVFATs = vfatSyncCheckLocal(&la, ohN);

    response->set_word("goodVFATs", vfatSyncCheckLocal(&la, ohN));

    rtxn.abort();
}

void configureVFAT3DacMonitorLocal(localArgs *la, uint32_t ohN, uint32_t mask, uint32_t dacSelect)
{
    uint32_t goodVFATs = vfatSyncCheckLocal(la, ohN);
    uint32_t notmask = ~mask & 0xFFFFFF;
    if ((notmask & goodVFATs) != notmask) { // FIXME, standard check+message?
        std::stringstream errmsg;
        errmsg << "One of the unmasked VFATs is not Sync'd."
               << "\tgoodVFATs: 0x" << std::hex << std::setw(8) << std::setfill('0') << goodVFATs
               << "\tnotmask: 0x"   << std::hex << std::setw(8) << std::setfill('0') << notmask << std::dec;
        throw std::runtime_error(errmsg.str());
        la->response->set_string("error", errmsg.str());
        return;
    }

    // Get ref voltage and monitor gain
    // These should have been set at time of configure
    uint32_t adcVRefValues[oh::VFATS_PER_OH];
    uint32_t monitorGainValues[oh::VFATS_PER_OH];
    broadcastReadLocal(la, adcVRefValues, ohN, "CFG_VREF_ADC", mask);
    broadcastReadLocal(la, monitorGainValues, ohN, "CFG_MON_GAIN", mask);

    for (uint8_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
        if (!((notmask >> vfatN) & 0x1)) // FIXME, standard check?
            continue;

        //Build global control 4 register
        uint32_t glbCtr4 = (adcVRefValues[vfatN] << 8) + (monitorGainValues[vfatN] << 7) + dacSelect;
        writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.CFG_4",ohN,vfatN), glbCtr4);
    }

    return;
}

void configureVFAT3DacMonitor(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    const uint32_t ohN      = request->get_word("ohN");
    const uint32_t vfatMask = request->get_key_exists("vfatMask") ? request->get_word("vfatMask") : 0x0;
    uint32_t dacSelect = request->get_word("dacSelect");

    LOGGER->log_message(LogManager::INFO, stdsprintf("Programming VFAT3 ADC Monitoring for Selection %i",dacSelect));
    configureVFAT3DacMonitorLocal(&la, ohN, vfatMask, dacSelect);

    rtxn.abort();
}

void configureVFAT3DacMonitorMultiLink(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohMask    = request->get_word("ohMask");
    uint32_t dacSelect = request->get_word("dacSelect");

    uint32_t NOH = readReg(&la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
    if (request->get_key_exists("NOH")) { // FIXME standard check+message?
        uint32_t NOH_requested = request->get_word("NOH");
        if (NOH_requested <= NOH) {
            NOH = NOH_requested;
        } else {
            std::stringstream errmsg;
            errmsg << "NOH requested (" << NOH_requested << ") > NUM_OF_OH AMC register value ("
                   << NOH << "), NOH request will be disregarded";
            LOGGER->log_message(LogManager::WARNING, errmsg.str());
        }
    }
    for (uint8_t ohN = 0; ohN < NOH; ++ohN) {
        if (!((ohMask >> ohN) & 0x1)) {
            continue;
        }

        //Get VFAT Mask
        uint32_t vfatMask = getOHVFATMaskLocal(&la, ohN);

        LOGGER->log_message(LogManager::INFO, stdsprintf("Programming VFAT3 ADC Monitoring on OH%i for Selection %i",ohN,dacSelect));
        configureVFAT3DacMonitorLocal(&la, ohN, vfatMask, dacSelect);
    }

    rtxn.abort();
}

void configureVFAT3sLocal(localArgs *la, uint32_t ohN, uint32_t vfatMask, uint32_t const *config)
{
    uint32_t goodVFATs = vfatSyncCheckLocal(la, ohN);
    uint32_t notmask = ~vfatMask & 0xFFFFFF;

    if ((notmask & goodVFATs) != notmask) { // FIXME, standard check+message?
        std::stringstream errmsg;
        errmsg << "One of the unmasked VFATs is not Sync'd."
               << "\tgoodVFATs: 0x" << std::hex << std::setw(8) << std::setfill('0') << goodVFATs
               << "\tnotmask: 0x"   << std::hex << std::setw(8) << std::setfill('0') << notmask << std::dec;
        throw std::runtime_error(errmsg.str());
        la->response->set_string("error", errmsg.str());
        return;
    }

    if (config == nullptr) {
        LOGGER->log_message(LogManager::INFO, "Load configuration settings from text file");
        for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
            if ((notmask >> vfatN) & 0x1) {
              std::stringstream configFileBase;
              configFileBase << "/mnt/persistent/gemdaq/vfat3/config_OH" << ohN
                             << "_VFAT" << vfatN << ".txt";
                std::ifstream infile(configFileBase.str());
                if (!infile.is_open()) {
                    std::stringstream errmsg;
                    errmsg << "Could not open config file " << configFileBase.str();
                    LOGGER->log_message(LogManager::ERROR, errmsg.str());
                    la->response->set_string("error", errmsg.str());
                    return;
                }
                std::string line;
                std::getline(infile,line); // skip first line
                std::stringstream regBase;
                regBase << "GEM_AMC.OH.OH" << ohN << ".GEB.VFAT" << vfatN << ".CFG_";
                while (std::getline(infile,line)) {
                    std::stringstream iss(line);
                    std::string dacName;
                    uint32_t dacVal;
                    if (!(iss >> dacName >> dacVal)) {
                        std::stringstream errmsg;
                        errmsg << "Unable to read settings from line: " << line;
                        LOGGER->log_message(LogManager::ERROR, errmsg.str());
                        throw std::runtime_error(errmsg.str());
                        la->response->set_string("error", errmsg.str());
                        break;
                    } else {
                        std::string regName = regBase.str() + dacName;
                        writeReg(la, regName, dacVal);
                    }
                }
            }
        }
    } else {
        LOGGER->log_message(LogManager::INFO, "Loading configuration settings from BLOB not yet implemented");
        size_t offset = 0x0;
        // FIXME presumes certain structure of config:
        //   * 32-bit words containing full VFAT configuration
        //   * 74 words per VFAT (includes necessary padding, as per BLASTER interface)
        //   * BLOB does not contain words for masked VFATs, i.e., incompatible with BLASTER interface
        for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
            if ((notmask >> vfatN) & 0x1) {
                uint32_t const* vfatcfg = config+offset;
                writeVFAT3ConfigLocal(la, ohN, vfatN, vfatcfg);
                offset += vfat::VFAT_SINGLE_RAM_SIZE;  // FIXME here assumes input data *only* provided for unmasked VFATs
            }
            // offset += vfat::VFAT_SINGLE_RAM_SIZE;  // FIXME here assumes input data provided for *all* VFATs
        }
    }
}

void configureVFAT3s(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    const uint32_t ohN      = request->get_word("ohN");
    const uint32_t vfatMask = request->get_key_exists("vfatMask") ? request->get_word("vfatMask") : 0x0;
    const bool useRAM       = request->get_key_exists("useRAM") ? request->get_word("useRAM") : false;

    vfat::config_t config{};
    uint32_t cfg_sz = 0; // FIXME unnecessary?
    uint32_t* cfg_p = nullptr;
    if (!useRAM) {
        if (request->get_key_exists("vfatcfg")) {
            cfg_sz = request->get_binarydata_size("vfatcfg"); // FIXME, does this even matter?
            request->get_binarydata("vfatcfg", config.data(), config.size());
            cfg_p = reinterpret_cast<uint32_t*>(config.data());
        }
        configureVFAT3sLocal(&la, ohN, vfatMask, cfg_p);
        cfg_p = nullptr; // FIXME just for safety
    } else {
        std::vector<uint32_t> vfatcfg;
        vfatcfg.resize(vfat::VFAT_SINGLE_RAM_SIZE*oh::VFATS_PER_OH);
        cfg_sz = readVFATConfRAMLocal(&la, vfatcfg.data(), vfatcfg.size(), (0x1<<ohN));
        for (uint8_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
            std::copy_n(reinterpret_cast<uint16_t*>(vfatcfg.data()+(vfatN*vfat::VFAT_SINGLE_RAM_SIZE)),
                        vfat::CFG_SIZE, config.begin());
            cfg_sz = config.size();
            try {
                cfg_p = reinterpret_cast<uint32_t*>(config.data());
                writeVFAT3ConfigLocal(&la, ohN, vfatN, cfg_p);
                cfg_p = nullptr; // FIXME just for safety
            } catch (const std::runtime_error& e) { // FIXME continue on error?
                std::stringstream errmsg;
                errmsg << "Error writing VFAT3 config: " << e.what();
                rtxn.abort();  // FIXME necessary?
                EMIT_RPC_ERROR(la.response, errmsg.str(), (void)"");
            }
        }
    }
    rtxn.abort();
}

void getChannelRegistersVFAT3(const RPCMsg *request, RPCMsg *response)
{
    LOGGER->log_message(LogManager::INFO, "Getting VFAT3 Channel Registers");

    GETLOCALARGS(response);

    const uint32_t ohN      = request->get_word("ohN");
    const uint32_t vfatMask = request->get_key_exists("vfatMask") ? request->get_word("vfatMask") : 0x0;

    uint32_t chanRegData[oh::VFATS_PER_OH*vfat::CHANNELS_PER_VFAT];

    getChannelRegistersVFAT3Local(&la, ohN, vfatMask, chanRegData);

    response->set_word_array("chanRegData",chanRegData,oh::VFATS_PER_OH*vfat::CHANNELS_PER_VFAT);

    rtxn.abort();
}

void getChannelRegistersVFAT3Local(localArgs *la, uint32_t ohN, uint32_t vfatMask, uint32_t *chanRegData)
{
    //Determine the inverse of the vfatmask
    uint32_t notmask = ~vfatMask & 0xFFFFFF;

    LOGGER->log_message(LogManager::INFO, "Read channel register settings");
    for (uint8_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
        if (!((notmask >> vfatN) & 0x1)) // FIXME, standard check?
            continue;

        uint32_t goodVFATs = vfatSyncCheckLocal(la, ohN); // FIXME every VFAT?
        if (!((goodVFATs >> vfatN) & 0x1)) { // FIXME, standard check+message?
            std::stringstream errmsg;
            errmsg << "The requested VFAT is not Sync'd."
                   << "\t goodVFATs: 0x"      << std::hex << std::setw(8) << std::setfill('0') << goodVFATs << std::dec
                   << "\t requested VFAT: 0x" << static_cast<uint32_t>(vfatN)
                   << "\t maskOH: 0x"         << std::hex << std::setw(8) << std::setfill('0') << vfatMask << std::dec;
            throw std::runtime_error(errmsg.str());
            la->response->set_string("error", errmsg.str());
            return;
        }

        for (uint8_t chan = 0; chan < vfat::CHANNELS_PER_VFAT; ++chan) {
            int idx = vfatN*vfat::CHANNELS_PER_VFAT + chan;

            std::stringstream regName;
            regName << "GEM_AMC.OH.OH" << ohN << ".GEB.VFAT" << static_cast<uint32_t>(vfatN)
                    << ".VFAT_CHANNELS.CHANNEL"              << static_cast<uint32_t>(chan);

            std::stringstream msg;
            msg << "Reading channel register for VFAT" << static_cast<uint32_t>(vfatN)
                << " chan "                            << static_cast<uint32_t>(chan);
            LOGGER->log_message(LogManager::INFO, msg.str());
            chanRegData[idx] = readRawAddress(getAddress(la, regName.str()), la->response);
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    }

    return;
}

void readVFAT3ADCLocal(localArgs *la, uint32_t * outData, uint32_t ohN, bool useExtRefADC, uint32_t mask)
{
    if (useExtRefADC) { //Case: Use ADC with external reference
        broadcastReadLocal(la, outData, ohN, "ADC1_UPDATE", mask);
        std::this_thread::sleep_for(std::chrono::microseconds(20));
        broadcastReadLocal(la, outData, ohN, "ADC1_CACHED", mask);
    } else{ //Case: Use ADC with internal reference
        broadcastReadLocal(la, outData, ohN, "ADC0_UPDATE", mask);
        std::this_thread::sleep_for(std::chrono::microseconds(20));
        broadcastReadLocal(la, outData, ohN, "ADC0_CACHED", mask);
    }

    return;
}

void readVFAT3ADC(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    const uint32_t ohN      = request->get_word("ohN");
    const uint32_t vfatMask = request->get_key_exists("vfatMask") ? request->get_word("vfatMask") : 0x0;
    bool useExtRefADC = request->get_word("useExtRefADC");

    uint32_t adcData[oh::VFATS_PER_OH];

    LOGGER->log_message(LogManager::INFO, stdsprintf("Reading VFAT3 ADC's for OH%i with mask %x",ohN, vfatMask));
    readVFAT3ADCLocal(&la, adcData, ohN, useExtRefADC, vfatMask);

    response->set_word_array("adcData",adcData,oh::VFATS_PER_OH);

    rtxn.abort();
}

void readVFAT3ADCMultiLink(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohMask = request->get_word("ohMask");
    bool useExtRefADC = request->get_word("useExtRefADC");

    uint32_t NOH = readReg(&la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
    if (request->get_key_exists("NOH")) { // FIXME standard check+message?
        uint32_t NOH_requested = request->get_word("NOH");
        if (NOH_requested <= NOH) {
            NOH = NOH_requested;
        } else {
            std::stringstream errmsg;
            errmsg << "NOH requested (" << NOH_requested << ") > NUM_OF_OH AMC register value ("
                   << NOH << "), NOH request will be disregarded";
            LOGGER->log_message(LogManager::WARNING, errmsg.str());
        }
    }
    uint32_t adcData[oh::VFATS_PER_OH] = {0};
    uint32_t adcDataAll[amc::OH_PER_AMC*oh::VFATS_PER_OH] = {0};
    for (uint8_t ohN = 0; ohN < NOH; ++ohN) {
        if (!((ohMask >> ohN) & 0x1))
            continue;

        std::stringstream msg;
        msg << "Reading VFAT3 ADC Values for all chips on OH" << static_cast<uint32_t>(ohN);
        LOGGER->log_message(LogManager::INFO, msg.str());

        //Get VFAT Mask
        uint32_t vfatMask = getOHVFATMaskLocal(&la, ohN);

        //Get all ADC values
        readVFAT3ADCLocal(&la, adcData, ohN, useExtRefADC, vfatMask);

        //Copy all ADC values
        std::copy(adcData, adcData+oh::VFATS_PER_OH, adcDataAll+(oh::VFATS_PER_OH*ohN));
    } //End Loop over all Optohybrids

    response->set_word_array("adcDataAll",adcDataAll,amc::OH_PER_AMC*oh::VFATS_PER_OH);

    rtxn.abort();
}

void setChannelRegistersVFAT3SimpleLocal(localArgs *la, uint32_t ohN, uint32_t vfatMask, uint32_t *chanRegData)
{
    //Determine the inverse of the vfatmask
    uint32_t notmask = ~vfatMask & 0xFFFFFF;

    LOGGER->log_message(LogManager::INFO, "Write channel register settings");
    for (uint8_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
        if (!((notmask >> vfatN) & 0x1)) // FIXME, standard check?
            continue;

        uint32_t goodVFATs = vfatSyncCheckLocal(la, ohN); // FIXME every VFAT?
        if (!((goodVFATs >> vfatN) & 0x1)) { // FIXME, standard check+message?
            std::stringstream errmsg;
            errmsg << "The requested VFAT is not Sync'd."
                   << "\t goodVFATs: 0x"      << std::hex << std::setw(8) << std::setfill('0') << goodVFATs << std::dec
                   << "\t requested VFAT: 0x" << static_cast<uint32_t>(vfatN)
                   << "\t vfatMask: 0x"       << std::hex << std::setw(8) << std::setfill('0') << vfatMask << std::dec;
            throw std::runtime_error(errmsg.str());
            la->response->set_string("error", errmsg.str());
            return;
        }

        for (uint8_t chan = 0; chan < vfat::CHANNELS_PER_VFAT; ++chan) {
            int idx = vfatN*vfat::CHANNELS_PER_VFAT + chan;
            std::stringstream regName;
            regName << "GEM_AMC.OH.OH" << ohN << ".GEB.VFAT" << static_cast<uint32_t>(vfatN)
                    << ".VFAT_CHANNELS.CHANNEL"              << static_cast<uint32_t>(chan);
            writeRawAddress(getAddress(la, regName.str()), chanRegData[idx], la->response);
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    }

    return;
}

void setChannelRegistersVFAT3Local(localArgs *la, uint32_t ohN, uint32_t vfatMask, uint32_t *calEnable, uint32_t *masks, uint32_t *trimARM, uint32_t *trimARMPol, uint32_t *trimZCC, uint32_t *trimZCCPol)
{
    //Determine the inverse of the vfatmask
    uint32_t notmask = ~vfatMask & 0xFFFFFF;

    LOGGER->log_message(LogManager::INFO, "Write channel register settings");
    for (uint8_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
        if (!((notmask >> vfatN) & 0x1)) // FIXME, standard check?
            continue;

        uint32_t goodVFATs = vfatSyncCheckLocal(la, ohN); // FIXME every VFAT?
        if (!((goodVFATs >> vfatN) & 0x1)) { // FIXME, standard check+message?
            std::stringstream errmsg;
            errmsg << "The requested VFAT is not Sync'd."
                   << "\t goodVFATs: 0x"      << std::hex << std::setw(8) << std::setfill('0') << goodVFATs << std::dec
                   << "\t requested VFAT: 0x" << static_cast<uint32_t>(vfatN)
                   << "\t vfatMask: 0x"       << std::hex << std::setw(8) << std::setfill('0') << vfatMask << std::dec;
            throw std::runtime_error(errmsg.str());
            la->response->set_string("error", errmsg.str());
            return;
        }

        for (uint8_t chan = 0; chan < vfat::CHANNELS_PER_VFAT; ++chan) {
            int idx = vfatN*vfat::CHANNELS_PER_VFAT + chan;

            std::stringstream regName;
            regName << "GEM_AMC.OH.OH" << ohN << ".GEB.VFAT" << static_cast<uint32_t>(vfatN)
                    << ".VFAT_CHANNELS.CHANNEL"              << static_cast<uint32_t>(chan);

            if (trimARM[idx] > 0x3F || trimARM[idx] < 0x0) { // FIXME, standard check+message?
                std::stringstream errmsg;
                errmsg << "arming comparator trim value must be positive in the range [0x0,0x3F]."
                       << " Value given for VFAT" << static_cast<uint32_t>(vfatN)
                       << " chan "                << static_cast<uint32_t>(chan)
                       << ": "                    << std::hex << trimARM[idx] << std::dec;
                throw std::range_error(errmsg.str());
                la->response->set_string("error", errmsg.str());
                return;
            } else if (trimZCC[idx] > 0x3F || trimZCC[idx] < 0x0) { // FIXME, standard check+message?
                std::stringstream errmsg;
                errmsg << "zero crossing comparator trim value must be positive in the range [0x0,0x3F]."
                       << " Value given for VFAT" << static_cast<uint32_t>(vfatN)
                       << " chan "                << static_cast<uint32_t>(chan)
                       << ": "                    << std::hex << trimZCC[idx] << std::dec;
                throw std::range_error(errmsg.str());
                la->response->set_string("error", errmsg.str());
                return;
            }

            std::stringstream msg;
            msg << "Setting channel register for VFAT" << static_cast<uint32_t>(vfatN)
                << " chan "                            << static_cast<uint32_t>(chan);
            LOGGER->log_message(LogManager::INFO, msg.str());
            uint32_t chanRegVal = (calEnable[idx]  << 15) + (masks[idx]   << 14)
                                + (trimZCCPol[idx] << 13) + (trimZCC[idx] << 7)
                                + (trimARMPol[idx] <<  6) + (trimARM[idx]);
            writeRawAddress(getAddress(la, regName.str()), chanRegVal, la->response);
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    }

    return;
}

void setChannelRegistersVFAT3(const RPCMsg *request, RPCMsg *response)
{
    LOGGER->log_message(LogManager::INFO, "Setting VFAT3 Channel Registers");

    GETLOCALARGS(response);

    const uint32_t ohN      = request->get_word("ohN");
    const uint32_t vfatMask = request->get_key_exists("vfatMask") ? request->get_word("vfatMask") : 0x0;

    if (request->get_key_exists("simple")) {
        uint32_t chanRegData[3072];

        request->get_word_array("chanRegData",chanRegData);

        setChannelRegistersVFAT3SimpleLocal(&la, ohN, vfatMask, chanRegData);
    } //End Case: user provided a single array
    else{ //Case: user provided multiple arrays
        uint32_t calEnable[3072];
        uint32_t masks[3072];
        uint32_t trimARM[3072];
        uint32_t trimARMPol[3072];
        uint32_t trimZCC[3072];
        uint32_t trimZCCPol[3072];

        request->get_word_array("calEnable",calEnable);
        request->get_word_array("masks",masks);
        request->get_word_array("trimARM",trimARM);
        request->get_word_array("trimARMPol",trimARMPol);
        request->get_word_array("trimZCC",trimZCC);
        request->get_word_array("trimZCCPol",trimZCCPol);

        setChannelRegistersVFAT3Local(&la, ohN, vfatMask, calEnable, masks, trimARM, trimARMPol, trimZCC, trimZCCPol);
    } //End Case: user provided multiple arrays

    rtxn.abort();
} //End setChannelRegistersVFAT3()

void statusVFAT3sLocal(localArgs *la, uint32_t ohN)
{
    std::string regs [] = {"CFG_PULSE_STRETCH ",
                           "CFG_SYNC_LEVEL_MODE",
                           "CFG_FP_FE",
                           "CFG_RES_PRE",
                           "CFG_CAP_PRE",
                           "CFG_PT",
                           "CFG_SEL_POL",
                           "CFG_FORCE_EN_ZCC",
                           "CFG_SEL_COMP_MODE",
                           "CFG_VREF_ADC",
                           "CFG_IREF",
                           "CFG_THR_ARM_DAC",
                           "CFG_LATENCY",
                           "CFG_CAL_SEL_POL",
                           "CFG_CAL_DAC",
                           "CFG_CAL_MODE",
                           "CFG_BIAS_CFD_DAC_2",
                           "CFG_BIAS_CFD_DAC_1",
                           "CFG_BIAS_PRE_I_BSF",
                           "CFG_BIAS_PRE_I_BIT",
                           "CFG_BIAS_PRE_I_BLCC",
                           "CFG_BIAS_PRE_VREF",
                           "CFG_BIAS_SH_I_BFCAS",
                           "CFG_BIAS_SH_I_BDIFF",
                           "CFG_BIAS_SH_I_BFAMP",
                           "CFG_BIAS_SD_I_BDIFF",
                           "CFG_BIAS_SD_I_BSF",
                           "CFG_BIAS_SD_I_BFCAS",
                           "CFG_RUN"};
    std::string regName;

    for (uint8_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
        char regBase [100];
        sprintf(regBase, "GEM_AMC.OH_LINKS.OH%i.VFAT%i.",ohN, vfatN);
        for (auto &reg : regs) {
            regName = std::string(regBase)+reg;
            la->response->set_word(regName,readReg(la,regName));
        }
    }
}

void statusVFAT3s(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");
    LOGGER->log_message(LogManager::INFO, "Reading VFAT3 status");

    statusVFAT3sLocal(&la, ohN);
    rtxn.abort();
}

uint16_t decodeChipID(uint32_t encChipID)
{
  // can the generator be static to limit creation/destruction of resources?
  static reedmuller rm = 0;
  static std::unique_ptr<int[]> encoded = nullptr;
  static std::unique_ptr<int[]> decoded = nullptr;

  if ((!(rm = reedmuller_init(2, 5)))
      || (!(encoded = std::make_unique<int[]>(rm->n)))
      || (!(decoded = std::make_unique<int[]>(rm->k)))
      ) {
    std::stringstream errmsg;
    errmsg << "Out of memory";

    reedmuller_free(rm);

    throw std::runtime_error(errmsg.str());
  }

  uint32_t maxcode = reedmuller_maxdecode(rm);
  if (encChipID > maxcode) {
    std::stringstream errmsg;
    errmsg << std::hex << std::setw(8) << std::setfill('0') << encChipID
           << " is larger than the maximum decodeable by RM(2,5)"
           << std::hex << std::setw(8) << std::setfill('0') << maxcode
           << std::dec;
    throw std::range_error(errmsg.str());
  }

  for (int j = 0; j < rm->n; ++j)
    encoded.get()[(rm->n-j-1)] = (encChipID>>j) &0x1;

  int result = reedmuller_decode(rm, encoded.get(), decoded.get());

  if (result) {
    uint16_t decChipID = 0x0;

    char tmp_decoded[1024];
    char* dp = tmp_decoded;

    for (int j = 0; j < rm->k; ++j)
      dp += sprintf(dp,"%d", decoded.get()[j]);

    char *p;
    errno = 0;

    uint32_t conv = strtoul(tmp_decoded, &p, 2);
    if (errno != 0 || *p != '\0') {
      std::stringstream errmsg;
      errmsg << "Unable to convert " << std::string(tmp_decoded) << " to int type";

      reedmuller_free(rm);

      throw std::runtime_error(errmsg.str());
    } else {
      decChipID = conv;
      reedmuller_free(rm);
      return decChipID;
    }
  } else {
    std::stringstream errmsg;
    errmsg << "Unable to decode message 0x"
           << std::hex << std::setw(8) << std::setfill('0') << encChipID
           << ", probably more than " << reedmuller_strength(rm) << " errors";

    reedmuller_free(rm);
    throw std::runtime_error(errmsg.str());
  }
}

void getVFAT3ChipIDsLocal(localArgs *la, uint32_t ohN, uint32_t *chipIDs, uint32_t vfatMask, bool rawID)
{
  uint32_t goodVFATs = vfatSyncCheckLocal(la, ohN);
  uint32_t notmask = ~vfatMask & 0xFFFFFF;
  // if ((notmask & goodVFATs) != notmask) { // FIXME, standard check+message?
  //     std::stringstream errmsg;
  //     errmsg << "One of the unmasked VFATs is not Sync'd."
  //            << "\tgoodVFATs: 0x" << std::hex << std::setw(8) << std::setfill('0') << goodVFATs
  //            << "\tnotmask: 0x"   << std::hex << std::setw(8) << std::setfill('0') << notmask << std::dec;
  //     throw std::runtime_error(errmsg.str());
  //     la->response->set_string("error", errmsg.str());
  //     return;
  // }

  for (uint8_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
    std::stringstream regBase;
    regBase << "GEM_AMC.OH.OH" << static_cast<uint32_t>(ohN)
            << ".GEB.VFAT"     << static_cast<uint32_t>(vfatN)
            << ".HW_CHIP_ID";

    if (!(((goodVFATs&notmask) >> vfatN) & 0x1)) {
        chipIDs[vfatN] = 0xdeaddead;
        la->response->set_word(regBase.str(), 0xdeaddead);
        continue;
    }

    uint32_t id = readReg(la,regBase.str());

    uint16_t decChipID = 0x0;
    try {
      decChipID = decodeChipID(id);
      std::stringstream msg;
      msg << "OH" << ohN << "::VFAT" << static_cast<int>(vfatN) << ": chipID is: 0x"
          << std::hex << std::setw(8) << std::setfill('0') << id << std::dec
          <<"(raw) or 0x"
          << std::hex << std::setw(4) << std::setfill('0') << decChipID << std::dec
          << "(decoded)";
      LOGGER->log_message(LogManager::INFO, msg.str());

      // FIXME return data, rather than set response
      if (rawID) {
        chipIDs[vfatN] = id;
        la->response->set_word(regBase.str(), id); // FIXME, remove
      } else {
        chipIDs[vfatN] = decChipID;
        la->response->set_word(regBase.str(), decChipID); // FIXME, remove
      }
    } catch (std::runtime_error& e) {
      std::stringstream errmsg;
      errmsg << "Error decoding chipID: " << e.what()
             << ", returning raw chipID";
      LOGGER->log_message(LogManager::ERROR, errmsg.str());
      chipIDs[vfatN] = id;
      la->response->set_word(regBase.str(),id); // FIXME, remove
    }
  }
}

void getVFAT3ChipIDs(const RPCMsg *request, RPCMsg *response)
{
    // struct localArgs la = getLocalArgs(response);
    GETLOCALARGS(response);

    const uint32_t ohN      = request->get_word("ohN");
    const uint32_t vfatMask = request->get_key_exists("vfatMask") ? request->get_word("vfatMask") : 0x0;

    bool rawID = false;
    if (request->get_key_exists("rawID"))
        rawID = request->get_word("rawID");

    LOGGER->log_message(LogManager::DEBUG, "Reading VFAT3 chipIDs");

    std::vector<uint32_t> chipIDs;
    chipIDs.resize(oh::VFATS_PER_OH);
    try {
        getVFAT3ChipIDsLocal(&la, ohN, chipIDs.data(), vfatMask, rawID);
    } catch (const std::runtime_error& e) {
        std::stringstream errmsg;
        errmsg << "Error reading VFAT3 config: " << e.what();
        rtxn.abort();  // FIXME necessary?
        EMIT_RPC_ERROR(la.response, errmsg.str(), (void)"");
    }
    response->set_word_array("chipIDs", chipIDs.data(), chipIDs.size());
    rtxn.abort();
}

uint32_t readVFAT3ConfigLocal(localArgs *la, uint8_t const& ohN, uint8_t const& vfatN, uint32_t *config)
{
    std::stringstream base;
    base << "GEM_AMC.OH.OH" << static_cast<uint32_t>(ohN)
         << ".GEB.VFAT"     << static_cast<uint32_t>(vfatN)
         << ".VFAT_CHANNELS";
    uint32_t baseaddr = getAddress(la, base.str());

    uint16_t* vfatconfig = reinterpret_cast<uint16_t*>(config);

    for (size_t reg = 0; reg < vfat::CFG_SIZE; ++reg) {
        vfatconfig[reg] = 0xffff&readRawAddress(baseaddr+reg, la->response);
    }

    return 0x0;
}

void readVFAT3Config(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    const uint32_t ohN   = request->get_word("ohN");
    const uint32_t vfatN = request->get_word("vfatN");

    std::vector<uint32_t> config;
    config.resize(vfat::VFAT_SINGLE_RAM_SIZE);
    try {
      readVFAT3ConfigLocal(&la, ohN, vfatN, config.data());
      response->set_binarydata("config", config.data(), config.size());
    } catch (const std::runtime_error& e) {
      std::stringstream errmsg;
      errmsg << "Error reading VFAT3 config: " << e.what();
      rtxn.abort();  // FIXME necessary?
      EMIT_RPC_ERROR(la.response, errmsg.str(), (void)"");
    }

    rtxn.abort();
}

// void writeVFAT3ConfigLocal(localArgs *la, uint8_t const& ohN, uint8_t const& vfatN, vfat::config_t const& config)
void writeVFAT3ConfigLocal(localArgs *la, uint8_t const ohN, uint8_t const vfatN, uint32_t const *config)
{
    if (config == nullptr) {
        std::stringstream errmsg;
        errmsg << "The config data supplied is invalid!";
        throw std::runtime_error(errmsg.str());
    }

    amc::isValidOptoHybrid(ohN, readReg(la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH"));
    oh::isValidVFAT(vfatN);

    std::stringstream base;
    base << "GEM_AMC.OH.OH" << static_cast<uint32_t>(ohN)
         << ".GEB.VFAT"     << static_cast<uint32_t>(vfatN)
         << ".VFAT_CHANNELS";
    uint32_t baseAddr = getAddress(la, base.str());

    uint16_t const *vfatconfig = reinterpret_cast<uint16_t const*>(config);

    for (size_t reg = 0; reg < vfat::CFG_SIZE; ++reg) {
        writeRawAddress(baseAddr+reg, 0xffff&vfatconfig[reg], la->response);
    }

    return;
}

void writeVFAT3Config(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    const uint32_t ohN   = request->get_word("ohN");
    const uint32_t vfatN = request->get_word("vfatN");
    const bool useRAM    = request->get_word("useRAM");

    std::vector<uint32_t> config;
    try {
      if (!useRAM) {
        uint32_t cfg_sz = request->get_binarydata_size("config");
        config.resize(cfg_sz);
        request->get_binarydata("config", config.data(), cfg_sz);
      } else {
        config.resize(vfat::VFAT_SINGLE_RAM_SIZE);

        // FIXME this does not read from the RAM
        readVFAT3ConfigLocal(&la, ohN, vfatN, config.data());

        std::vector<uint32_t> tmpconfig;
        tmpconfig.resize(24*vfat::VFAT_SINGLE_RAM_SIZE);

        // FIXME this does not work for a single VFAT,
        // FIXME rather, it reads the entire VFAT RAM for the specified OH...
        readVFATConfRAMLocal(&la, tmpconfig.data(), 24*vfat::VFAT_SINGLE_RAM_SIZE, (0x1<<ohN));
        // FIXME put tmpconfig.data()+(vfatN*vfat::VFAT_SINGLE_RAM_SIZE) into config.data()
      }
    } catch (const std::runtime_error& e) {
      std::stringstream errmsg;
      errmsg << "Error reading VFAT3 config: " << e.what();
      rtxn.abort();  // FIXME necessary?
      EMIT_RPC_ERROR(la.response, errmsg.str(), (void)"");
    }

    try {
      writeVFAT3ConfigLocal(&la, ohN, vfatN, config.data());
    } catch (const std::runtime_error& e) {
      std::stringstream errmsg;
      errmsg << "Error writing VFAT3 config: " << e.what();
      rtxn.abort();  // FIXME necessary?
      EMIT_RPC_ERROR(la.response, errmsg.str(), (void)"");
    }

    rtxn.abort();
}

/** MIGRATED FROM optohybrid.so **/
void broadcastWriteLocal(localArgs *la, uint32_t ohN, std::string regName, uint32_t value, uint32_t mask)
{
  uint32_t fw_maj = readReg(la, "GEM_AMC.GEM_SYSTEM.RELEASE.MAJOR");
  if (fw_maj == 1) {
    char regBase [100];
    sprintf(regBase, "GEM_AMC.OH.OH%i.GEB.Broadcast",ohN);

    std::string t_regName;

    // Reset broadcast module
    t_regName = std::string(regBase) + ".Reset";
    writeRawReg(la, t_regName, 0);
    // Set broadcast mask
    t_regName = std::string(regBase) + ".Mask";
    writeRawReg(la, t_regName, mask);
    // Issue broadcast write request
    t_regName = std::string(regBase) + ".Request." + regName;
    writeRawReg(la, t_regName, value);
    // Wait until broadcast write finishes
    t_regName = std::string(regBase) + ".Running";
    while (uint32_t t_res = readRawReg(la, t_regName)) {
      if (t_res == 0xdeaddead) break;
      usleep(1000);
    }
  } else if (fw_maj == 3) {
    std::string t_regName;
    for (uint8_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
      if (!((mask >> vfatN)&0x1)) {
        char regBase [100];
        sprintf(regBase, "GEM_AMC.OH.OH%i.GEB.VFAT%i.",ohN, vfatN);
        t_regName = std::string(regBase)+regName;
        writeReg(la, t_regName, value);
      }
    }
  } else {
    LOGGER->log_message(LogManager::ERROR, stdsprintf("Unexpected value for system release major: %i",fw_maj));
  }
}

void broadcastWrite(const RPCMsg *request, RPCMsg *response)
{
  GETLOCALARGS(response);

  std::string regName = request->get_string("reg_name");
  uint32_t value = request->get_word("value");
  uint32_t mask  = request->get_key_exists("mask")?request->get_word("mask"):0xFF000000;
  uint32_t ohN   = request->get_word("ohN");

  broadcastWriteLocal(&la, ohN, regName, value, mask);
  rtxn.abort();
}

void broadcastReadLocal(localArgs *la, uint32_t * outData, uint32_t ohN, std::string regName, uint32_t mask)
{
  uint32_t fw_maj = readReg(la, "GEM_AMC.GEM_SYSTEM.RELEASE.MAJOR");
  char regBase [100];
  if (fw_maj == 1) {
    sprintf(regBase,"GEM_AMC.OH.OH%i.GEB.VFATS.VFAT",ohN);
  } else if (fw_maj == 3) {
    sprintf(regBase,"GEM_AMC.OH.OH%i.GEB.VFAT",ohN);
   } else {
    LOGGER->log_message(LogManager::ERROR, "Unexpected value for system release major!");
    la->response->set_string("error", "Unexpected value for system release major!");
  }
  std::string t_regName;
  for (uint8_t i = 0; i < oh::VFATS_PER_OH; ++i) {
    if ((mask >> i)&0x1) outData[i] = 0;
    else {
      t_regName = std::string(regBase) + std::to_string(i)+"."+regName;
      outData[i] = readReg(la, t_regName);
      if (outData[i] == 0xdeaddead) la->response->set_string("error",stdsprintf("Error reading register %s",t_regName.c_str()));
    }
  }
  return;
}

void broadcastRead(const RPCMsg *request, RPCMsg *response)
{
  GETLOCALARGS(response);

  std::string regName = request->get_string("reg_name");
  uint32_t mask = request->get_key_exists("mask")?request->get_word("mask"):0xFF000000;
  uint32_t ohN  = request->get_word("ohN");

  uint32_t outData[oh::VFATS_PER_OH];
  broadcastReadLocal(&la, outData, ohN, regName, mask);
  response->set_word_array("data", outData, oh::VFATS_PER_OH);

  rtxn.abort();
}

// Set default values to VFAT parameters. VFATs will remain in sleep mode
// FIXME VFAT2 OBSOLETE
void biasAllVFATsLocal(localArgs *la, uint32_t ohN, uint32_t mask)
{
  for (auto const& it : vfat_parameters) {
    broadcastWriteLocal(la, ohN, it.first, it.second, mask);
  }
}

void setAllVFATsToRunModeLocal(localArgs *la, uint32_t ohN, uint32_t mask)
{
    switch(fw_version_check("setAllVFATsToRunMode", la)) {
        case 3:
            broadcastWriteLocal(la, ohN, "CFG_RUN", 0x1, mask);
            break;
        case 1:
            broadcastWriteLocal(la, ohN, "ContReg0", 0x37, mask);
            break;
        default:
            LOGGER->log_message(LogManager::ERROR, "Unexpected value for system release major, do nothing");
            break;
    }

    return;
}

void setAllVFATsToSleepModeLocal(localArgs *la, uint32_t ohN, uint32_t mask)
{
    switch(fw_version_check("setAllVFATsToRunMode", la)) {
        case 3:
            broadcastWriteLocal(la, ohN, "CFG_RUN", 0x0, mask);
            break;
        case 1:
            broadcastWriteLocal(la, ohN, "ContReg0", 0x36, mask);
            break;
        default:
            LOGGER->log_message(LogManager::ERROR, "Unexpected value for system release major, do nothing");
            break;
    }

    return;
}

void loadVT1Local(localArgs *la, uint32_t ohN, std::string config_file, uint32_t vt1)
{
  char regBase [100];
  sprintf(regBase,"GEM_AMC.OH.OH%i",ohN);
  uint32_t vfatN, trimRange;
  std::string line, regName;
  // Check if there's a config file. If yes, set the thresholds and trim range according to it, otherwise st only thresholds (equal on all chips) to provided vt1 value
  if (config_file!="") {
    LOGGER->log_message(LogManager::INFO, stdsprintf("CONFIG FILE FOUND: %s", config_file.c_str()));
    std::ifstream infile(config_file);
    std::getline(infile,line);// skip first line
    while (std::getline(infile,line)) {
      std::stringstream iss(line);
      if (!(iss >> vfatN >> vt1 >> trimRange)) {
        LOGGER->log_message(LogManager::ERROR, "ERROR READING SETTINGS");
        la->response->set_string("error", "Error reading settings");
        break;
      } else {
        char regBase [100];
        sprintf(regBase,"GEM_AMC.OH.OH%i.GEB.VFATS.VFAT%i.VThreshold1",ohN, vfatN);
        //LOGGER->log_message(LogManager::INFO, stdsprintf("WRITING 0x%8x to REG: %s", vt1, regName.c_str()));
        writeRawReg(la, std::string(regName), vt1);
        sprintf(regBase,"GEM_AMC.OH.OH%i.GEB.VFATS.VFAT%i.ContReg3",ohN, vfatN);
        //LOGGER->log_message(LogManager::INFO, stdsprintf("WRITING 0x%8x to REG: %s", trimRange, regName.c_str()));
        writeRawReg(la, regName, trimRange);
      }
    }
  } else {
    LOGGER->log_message(LogManager::INFO, "CONFIG FILE NOT FOUND");
    broadcastWriteLocal(la, ohN, "VThreshold1", vt1);
  }
}

void loadVT1(const RPCMsg *request, RPCMsg *response)
{
  GETLOCALARGS(response);

  uint32_t ohN = request->get_word("ohN");
  std::string config_file = request->get_key_exists("thresh_config_filename")?request->get_string("thresh_config_filename"):"";
  uint32_t vt1 = request->get_key_exists("vt1")?request->get_word("vt1"):0x64;

  loadVT1Local(&la, ohN, config_file, vt1);

  rtxn.abort();
}

void loadTRIMDACLocal(localArgs *la, uint32_t ohN, std::string config_file)
{
  std::ifstream infile(config_file);
  std::string line, regName;
  uint32_t vfatN, vfatCH, trim, mask;
  std::getline(infile,line);// skip first line
  while (std::getline(infile,line)) {
    std::stringstream iss(line);
    if (!(iss >> vfatN >> vfatCH >> trim >> mask)) {
		  LOGGER->log_message(LogManager::ERROR, "ERROR READING SETTINGS");
      la->response->set_string("error", "Error reading settings");
      break;
    } else {
      char regBase [100];
      sprintf(regBase,"GEM_AMC.OH.OH%i.GEB.VFATS.VFAT%i.VFATChannels.ChanReg%i",ohN, vfatN, vfatCH);
      regName = std::string(regBase);
      writeRawReg(la, regName, trim + 32*mask);
    }
  }
}

void loadTRIMDAC(const RPCMsg *request, RPCMsg *response)
{
  GETLOCALARGS(response);

  uint32_t ohN = request->get_word("ohN");
  std::string config_file = request->get_string("trim_config_filename");//"/mnt/persistent/texas/test/chConfig_GEMINIm01L1.txt";

  loadTRIMDACLocal(&la, ohN, config_file);
  rtxn.abort();
}

// FIXME VFAT2 OBSOLETE
void configureVFATs(const RPCMsg *request, RPCMsg *response)
{
  GETLOCALARGS(response);

  uint32_t ohN    = request->get_word("ohN");
  std::string trim_config_file   = request->get_string("trim_config_filename");//"/mnt/persistent/texas/test/chConfig_GEMINIm01L1.txt";
  std::string thresh_config_file = request->get_key_exists("thresh_config_filename")?request->get_string("thresh_config_filename"):"";
  uint32_t vt1 = request->get_key_exists("vt1")?request->get_word("vt1"):0x64;

  LOGGER->log_message(LogManager::INFO, "BIAS VFATS");
  biasAllVFATsLocal(&la, ohN);
  LOGGER->log_message(LogManager::INFO, "LOAD VT1 VFATS");
  loadVT1Local(&la, ohN, thresh_config_file, vt1);
  LOGGER->log_message(LogManager::INFO, "LOAD TRIM VFATS");
  loadTRIMDACLocal(&la, ohN, trim_config_file);
  if (request->get_key_exists("set_run"))
    setAllVFATsToRunModeLocal(&la, ohN);

  rtxn.abort();
}

void stopCalPulse2AllChannelsLocal(localArgs *la, uint32_t ohN, uint32_t mask, uint32_t ch_min, uint32_t ch_max)
{
    //Get FW release
    uint32_t fw_maj = readReg(la, "GEM_AMC.GEM_SYSTEM.RELEASE.MAJOR");

    if (fw_maj == 1) {
        uint32_t trimVal=0;
        for (uint8_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
            if ((mask >> vfatN) & 0x1) continue; //skip masked VFATs
            for (uint32_t chan = ch_min; chan <= ch_max; ++chan) {
                if (chan>127) {
                    LOGGER->log_message(LogManager::ERROR, stdsprintf("OH %d: Chan %d greater than possible chan_max %d",ohN,chan,127));
                } else {
                    trimVal = (0x3f & readReg(la, stdsprintf("GEM_AMC.OH.OH%d.GEB.VFATS.VFAT%d.VFATChannels.ChanReg%d",ohN,vfatN,chan)));
                    writeReg(la, stdsprintf("GEM_AMC.OH.OH%d.GEB.VFATS.VFAT%d.VFATChannels.ChanReg%d",ohN,vfatN,chan),trimVal);
                }
            }
        }
    } else if (fw_maj == 3) {
        for (uint8_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
            if ((mask >> vfatN) & 0x1) continue; //skip masked VFATs
            for (uint32_t chan = ch_min; chan <= ch_max; ++chan) {
                writeReg(la, stdsprintf("GEM_AMC.OH.OH%d.GEB.VFAT%d.VFAT_CHANNELS.CHANNEL%d.CALPULSE_ENABLE", ohN, vfatN, chan), 0x0);
            }
        }
    } else {
        LOGGER->log_message(LogManager::ERROR, stdsprintf("Unexpected value for system release major: %i",fw_maj));
    }

    return;
}

void stopCalPulse2AllChannels(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");
    uint32_t mask = request->get_word("mask");
    uint32_t ch_min = request->get_word("ch_min");
    uint32_t ch_max = request->get_word("ch_max");

    stopCalPulse2AllChannelsLocal(&la, ohN, mask, ch_min, ch_max);

    rtxn.abort();
}
/** **/

extern "C" {
    const char *module_version_key = "vfat3 v1.0.1";
    int module_activity_color = 4;
    void module_init(ModuleManager *modmgr) {
        if (memhub_open(&memsvc) != 0) {
            std::stringstream errmsg;
            errmsg << "Unable to connect to memory service: "
                   << memsvc_get_last_error(memsvc);
            LOGGER->log_message(LogManager::ERROR, errmsg.str());
            LOGGER->log_message(LogManager::ERROR, "Unable to load module");
            return; // Do not register our functions, we depend on memsvc.
        }
        modmgr->register_method("vfat3", "configureVFAT3s",                   configureVFAT3s);
        modmgr->register_method("vfat3", "configureVFAT3DacMonitor",          configureVFAT3DacMonitor);
        modmgr->register_method("vfat3", "configureVFAT3DacMonitorMultiLink", configureVFAT3DacMonitorMultiLink); // FIXME
        modmgr->register_method("vfat3", "getChannelRegistersVFAT3",          getChannelRegistersVFAT3);
        modmgr->register_method("vfat3", "readVFAT3ADC",                      readVFAT3ADC);
        modmgr->register_method("vfat3", "readVFAT3ADCMultiLink",             readVFAT3ADCMultiLink); // FIXME
        modmgr->register_method("vfat3", "setChannelRegistersVFAT3",          setChannelRegistersVFAT3);
        modmgr->register_method("vfat3", "statusVFAT3s",                      statusVFAT3s);
        modmgr->register_method("vfat3", "vfatSyncCheck",                     vfatSyncCheck);
        modmgr->register_method("vfat3", "getVFAT3ChipIDs",                   getVFAT3ChipIDs);
        modmgr->register_method("vfat3", "readVFAT3Config",                   readVFAT3Config);
        modmgr->register_method("vfat3", "writeVFAT3Config",                  writeVFAT3Config);

        modmgr->register_method("vfat3", "broadcastRead",                     broadcastRead);
        modmgr->register_method("vfat3", "broadcastWrite",                    broadcastWrite);
        modmgr->register_method("vfat3", "configureVFATs",                    configureVFATs);
        modmgr->register_method("vfat3", "loadTRIMDAC",                       loadTRIMDAC);
        modmgr->register_method("vfat3", "loadVT1",                           loadVT1);
        modmgr->register_method("vfat3", "stopCalPulse2AllChannels",          stopCalPulse2AllChannels);
    }
}
