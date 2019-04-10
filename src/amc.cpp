/*! \file amc.cpp
 *  \brief AMC methods for RPC modules
 *  \author Cameron Bravo <cbravo135@gmail.com>
 *  \author Mykhailo Dalchenko <mykhailo.dalchenko@cern.ch>
 *  \author Brian Dorney <brian.l.dorney@cern.ch>
 */

#include "amc.h"
#include "amc/ttc.h"
#include "amc/daq.h"
#include "amc/sca.h"
#include "amc/blaster_ram.h"

#include <chrono>
#include <string>
#include <time.h>
#include <thread>
#include <vector>

unsigned int fw_version_check(const char* caller_name, localArgs *la)
{
    int iFWVersion = readReg(la, "GEM_AMC.GEM_SYSTEM.RELEASE.MAJOR");
    char regBuf[200];
    switch (iFWVersion) {
        case 1:
        {
            LOGGER->log_message(LogManager::INFO, "System release major is 1, v2B electronics behavior");
            break;
        }
        case 3:
        {
            LOGGER->log_message(LogManager::INFO, "System release major is 3, v3 electronics behavior");
            break;
        }
        default:
        {
            LOGGER->log_message(LogManager::ERROR, "Unexpected value for system release major!");
            sprintf(regBuf,"Unexpected value for system release major!");
            la->response->set_string("error",regBuf);
            break;
        }
    }
    return iFWVersion;
}

uint32_t getOHVFATMaskLocal(localArgs * la, uint32_t ohN)
{
    uint32_t mask = 0x0;
    for (int vfatN=0; vfatN<24; ++vfatN) { //Loop over all vfats
        uint32_t syncErrCnt = readReg(la, stdsprintf("GEM_AMC.OH_LINKS.OH%i.VFAT%i.SYNC_ERR_CNT",ohN,vfatN));

        if (syncErrCnt > 0x0) { //Case: nonzero sync errors, mask this vfat
            mask = mask + (0x1 << vfatN);
        } //End Case: nonzero sync errors, mask this vfat
    } //End loop over all vfats

    return mask;
} //End getOHVFATMaskLocal()

void getOHVFATMask(const RPCMsg *request, RPCMsg *response) {
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");

    uint32_t vfatMask = getOHVFATMaskLocal(&la, ohN);
    LOGGER->log_message(LogManager::INFO, stdsprintf("Determined VFAT Mask for OH%i to be 0x%x",ohN,vfatMask));

    response->set_word("vfatMask",vfatMask);

    rtxn.abort();
} //End getOHVFATMask(...)

void getOHVFATMaskMultiLink(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    int ohMask = 0xfff;
    if (request->get_key_exists("ohMask")) {
        ohMask = request->get_word("ohMask");
    }

    unsigned int NOH = readReg(&la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
    if (request->get_key_exists("NOH")) {
        unsigned int NOH_requested = request->get_word("NOH");
        if (NOH_requested <= NOH)
            NOH = NOH_requested;
        else
            LOGGER->log_message(LogManager::WARNING, stdsprintf("NOH requested (%i) > NUM_OF_OH AMC register value (%i), NOH request will be disregarded",NOH_requested,NOH));
    }

    uint32_t ohVfatMaskArray[12];
    for (unsigned int ohN=0; ohN<NOH; ++ohN) {
        // If this Optohybrid is masked skip it
        if (!((ohMask >> ohN) & 0x1)) {
            ohVfatMaskArray[ohN] = 0xffffff;
            continue;
        }
        else{
            ohVfatMaskArray[ohN] = getOHVFATMaskLocal(&la, ohN);
            LOGGER->log_message(LogManager::INFO, stdsprintf("Determined VFAT Mask for OH%i to be 0x%x",ohN,ohVfatMaskArray[ohN]));
        }
    } //End Loop over all Optohybrids

    //Debugging
    LOGGER->log_message(LogManager::DEBUG, "All VFAT Masks found, listing:");
    for (int ohN=0; ohN<12; ++ohN) {
        LOGGER->log_message(LogManager::DEBUG, stdsprintf("VFAT Mask for OH%i to be 0x%x",ohN,ohVfatMaskArray[ohN]));
    }

    response->set_word_array("ohVfatMaskArray",ohVfatMaskArray,12);

    rtxn.abort();
} //End getOHVFATMaskMultiLink(...)

std::vector<uint32_t> sbitReadOutLocal(localArgs *la, uint32_t ohN, uint32_t acquireTime, bool *maxNetworkSizeReached)
{
    //Setup the sbit monitor
    const int nclusters = 8;
    writeReg(la, "GEM_AMC.TRIGGER.SBIT_MONITOR.OH_SELECT", ohN);
    uint32_t addrSbitMonReset=getAddress(la, "GEM_AMC.TRIGGER.SBIT_MONITOR.RESET");
    uint32_t addrSbitL1ADelay=getAddress(la, "GEM_AMC.TRIGGER.SBIT_MONITOR.L1A_DELAY");
    uint32_t addrSbitCluster[nclusters];
    for (int iCluster=0; iCluster < nclusters; ++iCluster) {
        addrSbitCluster[iCluster] = getAddress(la, stdsprintf("GEM_AMC.TRIGGER.SBIT_MONITOR.CLUSTER%i",iCluster) );
    }

    //Take the VFATs out of slow control only mode
    writeReg(la, "GEM_AMC.GEM_SYSTEM.VFAT3.SC_ONLY_MODE", 0x0);

    //[0:10] address of sbit cluster
    //[11:13] cluster size
    //[14:26] L1A Delay (consider anything over 4095 as overflow)
    std::vector<uint32_t> storedSbits;

    //readout sbits
    time_t acquisitionTime,startTime;
    bool acquire=true;
    startTime=time(NULL);
    (*maxNetworkSizeReached) = false;
    uint32_t l1ADelay;
    while(acquire) {
        if ( sizeof(uint32_t) * storedSbits.size() > 65000 ) { //Max TCP/IP message is 65535
            (*maxNetworkSizeReached) = true;
            break;
        }

        //Reset monitors
        writeRawAddress(addrSbitMonReset, 0x1, la->response);

        //wait for 4095 clock cycles then read L1A delay
        std::this_thread::sleep_for (std::chrono::nanoseconds(4095*25));
        l1ADelay = readRawAddress(addrSbitL1ADelay, la->response);
        if (l1ADelay > 4095) { //Anything larger than this consider as overflow
            l1ADelay = 4095; //(0xFFF in hex)
        }

        //get sbits
        bool anyValid=false;
        std::vector<uint32_t> tempSBits; //will only be stored into storedSbits if anyValid is true
        for (int cluster=0; cluster<nclusters; ++cluster) {
            //bits [10:0] is the address of the cluster
            //bits [14:12] is the cluster size
            //bits 15 and 11 are not used
            uint32_t thisCluster = readRawAddress(addrSbitCluster[cluster], la->response);
            uint32_t sbitAddress = (thisCluster & 0x7ff);
            int clusterSize = (thisCluster >> 12) & 0x7;
            bool isValid = (sbitAddress < 1536); //Possible values are [0,(24*64)-1]

            if (isValid) {
                LOGGER->log_message(LogManager::INFO,stdsprintf("valid sbit data: thisClstr %x; sbitAddr %x;",thisCluster,sbitAddress));
                anyValid=true;
            }

            //Store the sbit
            tempSBits.push_back( ((l1ADelay & 0x1fff) << 14) + ((clusterSize & 0x7) << 11) + (sbitAddress & 0x7ff) );
        } //End Loop over clusters

        if (anyValid) {
            storedSbits.insert(storedSbits.end(),tempSBits.begin(),tempSBits.end());
        }

        acquisitionTime=difftime(time(NULL),startTime);
        if (uint32_t(acquisitionTime) > acquireTime) {
            acquire=false;
        }
    } //End readout sbits

    return storedSbits;
} //End sbitReadOutLocal(...)

void sbitReadOut(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");
    uint32_t acquireTime = request->get_word("acquireTime");

    bool maxNetworkSizeReached = false;

    time_t startTime=time(NULL);
    std::vector<uint32_t> storedSbits = sbitReadOutLocal(&la, ohN, acquireTime, &maxNetworkSizeReached);
    time_t approxLivetime=difftime(time(NULL),startTime);

    if (maxNetworkSizeReached) {
        response->set_word("maxNetworkSizeReached", maxNetworkSizeReached);
        response->set_word("approxLiveTime",approxLivetime);
    }
    response->set_word_array("storedSbits",storedSbits);

    rtxn.abort();
} //End sbitReadOut()

extern "C" {
    const char *module_version_key = "amc v1.0.1";
    int module_activity_color = 4;
    void module_init(ModuleManager *modmgr) {
        if (memhub_open(&memsvc) != 0) {
            LOGGER->log_message(LogManager::ERROR, stdsprintf("Unable to connect to memory service: %s", memsvc_get_last_error(memsvc)));
            LOGGER->log_message(LogManager::ERROR, "Unable to load module");
            return; // Do not register our functions, we depend on memsvc.
        }

        modmgr->register_method("amc", "getOHVFATMask",          getOHVFATMask); // FIXME belongs in optohybrid
        modmgr->register_method("amc", "getOHVFATMaskMultiLink", getOHVFATMaskMultiLink);
        modmgr->register_method("amc", "sbitReadOut",            sbitReadOut);

        // DAQ module methods (from amc/daq)
        modmgr->register_method("amc", "enableDAQLink",           enableDAQLink);
        modmgr->register_method("amc", "disableDAQLink",          disableDAQLink);
        modmgr->register_method("amc", "setZS",                   setZS);
        modmgr->register_method("amc", "resetDAQLink",            resetDAQLink);
        modmgr->register_method("amc", "setDAQLinkInputTimeout",  setDAQLinkInputTimeout);
        modmgr->register_method("amc", "setDAQLinkRunType",       setDAQLinkRunType);
        modmgr->register_method("amc", "setDAQLinkRunParameter",  setDAQLinkRunParameter);
        modmgr->register_method("amc", "setDAQLinkRunParameters", setDAQLinkRunParameters);

        modmgr->register_method("amc", "configureDAQModule",   configureDAQModule);
        modmgr->register_method("amc", "enableDAQModule",      enableDAQModule);

        // TTC module methods (from amc/ttc)
        modmgr->register_method("amc", "ttcModuleReset",     ttcModuleReset);
        modmgr->register_method("amc", "ttcMMCMReset",       ttcMMCMReset);
        modmgr->register_method("amc", "ttcMMCMPhaseShift",  ttcMMCMPhaseShift);
        modmgr->register_method("amc", "checkPLLLock",       checkPLLLock);
        modmgr->register_method("amc", "getMMCMPhaseMean",   getMMCMPhaseMean);
        modmgr->register_method("amc", "getMMCMPhaseMedian", getMMCMPhaseMedian);
        modmgr->register_method("amc", "getGTHPhaseMean",    getGTHPhaseMean);
        modmgr->register_method("amc", "getGTHPhaseMedian",  getGTHPhaseMedian);
        modmgr->register_method("amc", "ttcCounterReset",    ttcCounterReset);
        modmgr->register_method("amc", "getL1AEnable",       getL1AEnable);
        modmgr->register_method("amc", "setL1AEnable",       setL1AEnable);
        modmgr->register_method("amc", "getTTCConfig",       getTTCConfig);
        modmgr->register_method("amc", "setTTCConfig",       setTTCConfig);
        modmgr->register_method("amc", "getTTCStatus",       getTTCStatus);
        modmgr->register_method("amc", "getTTCErrorCount",   getTTCErrorCount);
        modmgr->register_method("amc", "getTTCCounter",      getTTCCounter);
        modmgr->register_method("amc", "getL1AID",           getL1AID);
        modmgr->register_method("amc", "getL1ARate",         getL1ARate);
        modmgr->register_method("amc", "getTTCSpyBuffer",    getTTCSpyBuffer);

        // SCA module methods (from amc/sca)
        // modmgr->register_method("amc", "scaHardResetEnable", scaHardResetEnable);
        modmgr->register_method("amc", "readSCAChipID",     readSCAChipID);
        modmgr->register_method("amc", "readSCASEUCounter", readSCASEUCounter);

        // BLASTER RAM module methods (from amc/blaster_ram)
        modmgr->register_method("amc", "writeConfRAM", writeConfRAM);
        modmgr->register_method("amc", "readConfRAM",  readConfRAM);
    }
}
