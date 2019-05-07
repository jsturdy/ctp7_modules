#include "optohybrid.h"

#include "amc.h"
#include "amc/blaster_ram.h"
#include "hw_constants.h"

void configureScanModuleLocal(localArgs *la, uint32_t ohN, uint32_t vfatN, uint32_t scanmode,
                              bool useUltra, uint32_t mask, uint32_t ch, uint32_t nevts,
                              uint32_t dacMin, uint32_t dacMax, uint32_t dacStep)
{
    std::stringstream scanBase;
    scanBase << "GEM_AMC.OH.OH" << ohN << ".ScanController";

    (useUltra) ? scanBase << ".ULTRA": scanBase << ".THLAT";

    // check if another scan is running
    uint32_t status = readReg(la, scanBase.str() + ".MONITOR.STATUS");
    if (status > 0) {
      std::stringstream errmsg;
      errmsg << scanBase.str() << "Scan is already running, not starting a new scan: 0x"
             << std::hex << status << std::dec;
      LOGGER->log_message(LogManager::ERROR, errmsg.str());
      la->response->set_string("error", errmsg.str());
      return;
    }

    // reset scan module
    writeRawReg(la, scanBase.str() + ".RESET", 0x1);

    // write scan parameters
    writeReg(la, scanBase.str() + ".CONF.MODE", scanmode);
    if (useUltra) {
      writeReg(la, scanBase.str() + ".CONF.MASK", mask);
    } else {
      writeReg(la, scanBase.str() + ".CONF.CHIP", vfatN);
    }

    // FIXME FROM HwOptoHybrid original // if (mode == 0x1 || mode == 0x3) {
    // FIXME FROM HwOptoHybrid original //   // protect for non-existent channels?
    // FIXME FROM HwOptoHybrid original //   // need also to enable this channel and disable all others
    // FIXME FROM HwOptoHybrid original //   writeReg(la, scanBase.str() + ".CONF.CHAN", channel);
    // FIXME FROM HwOptoHybrid original //   if (mode == 0x3) {
    // FIXME FROM HwOptoHybrid original //     // need also to enable cal pulse to this channel and disable all others
    // FIXME FROM HwOptoHybrid original //   }
    // FIXME FROM HwOptoHybrid original // }

    writeReg(la, scanBase.str() + ".CONF.CHAN",   ch);
    writeReg(la, scanBase.str() + ".CONF.NTRIGS", nevts);
    writeReg(la, scanBase.str() + ".CONF.MIN",    dacMin);
    writeReg(la, scanBase.str() + ".CONF.MAX",    dacMax);
    writeReg(la, scanBase.str() + ".CONF.STEP",   dacStep);

    return;
}

void configureScanModule(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");
    uint32_t scanmode = request->get_word("scanmode");

    bool useUltra = false;
    uint32_t mask = 0xFFFFFFFF;
    uint32_t vfatN = 0;
    if (request->get_key_exists("useUltra")) {
        useUltra = true;
        mask = request->get_word("mask");
    } else {
        vfatN = request->get_word("vfatN");
    }

    uint32_t ch      = request->get_word("ch");
    uint32_t nevts   = request->get_word("nevts");
    uint32_t dacMin  = request->get_word("dacMin");
    uint32_t dacMax  = request->get_word("dacMax");
    uint32_t dacStep = request->get_word("dacStep");

    configureScanModuleLocal(&la, ohN, vfatN, scanmode, useUltra, mask, ch, nevts, dacMin, dacMax, dacStep);

    rtxn.abort();
}

void printScanConfigurationLocal(localArgs *la, uint32_t ohN, bool useUltra)
{
    std::stringstream sstream;
    sstream<<ohN;
    std::string strOhN = sstream.str();

    //Set Scan Base
    std::string scanBase = "GEM_AMC.OH.OH" + strOhN + ".ScanController";
    (useUltra)?scanBase += ".ULTRA":scanBase += ".THLAT";

    //char regBuf[200];
    //sprintf(regBuf,scanBase);

    std::map<std::string, uint32_t> map_regValues;

    //Set reg names
    map_regValues[scanBase + ".CONF.MODE"]      = 0;
    map_regValues[scanBase + ".CONF.MIN"]       = 0;
    map_regValues[scanBase + ".CONF.MAX"]       = 0;
    map_regValues[scanBase + ".CONF.STEP"]      = 0;
    map_regValues[scanBase + ".CONF.CHAN"]      = 0;
    map_regValues[scanBase + ".CONF.NTRIGS"]    = 0;
    map_regValues[scanBase + ".MONITOR.STATUS"] = 0;

    if (useUltra) {
        map_regValues[scanBase + ".CONF.MASK"] = 0;
    } else {
        map_regValues[scanBase + ".CONF.CHIP"] = 0;
    }

    stdsprintf(scanBase.c_str());
    for (auto& regIter : map_regValues) {
        regIter.second = readReg(la, regIter.first);
        stdsprintf("FW %s   : %d",regIter.first.c_str(), regIter.second);
        if ( regIter.second == 0xdeaddead) {
            std::stringstream errmsg;
            errmsg << "Error erading register " << regIter.first.c_str();
            la->response->set_string("error",errmsg.str());
        }
    }

    return;
}

void printScanConfiguration(const RPCMsg *request, RPCMsg *response){
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");

    bool useUltra = false;
    if (request->get_key_exists("useUltra")){
        useUltra = true;
    }

    printScanConfigurationLocal(&la, ohN, useUltra);

    rtxn.abort();
}

void startScanModuleLocal(localArgs *la, uint32_t ohN, bool useUltra)
{
    std::stringstream scanBase;
    scanBase << "GEM_AMC.OH.OH" << ohN << ".ScanController";

    (useUltra) ? scanBase << ".ULTRA": scanBase << ".THLAT";

    // FIXME FROM HwOptoHybrid original // writeReg(getDeviceBaseNode(),scanBase.str()+".CONF.NTRIGS", nevts);

    uint32_t status = readReg(la, scanBase.str() + ".MONITOR.STATUS");
    if (status > 0) {
        std::stringstream errmsg;
        errmsg << scanBase.str() << "Scan is already running, not starting a new scan: 0x"
               << std::hex << status << std::dec;
        LOGGER->log_message(LogManager::ERROR, errmsg.str());
        la->response->set_string("error", errmsg.str());
        return;
    }

    uint32_t errcode = readReg(la, scanBase.str() + ".MONITOR.ERROR");
    if (errcode > 0 ){
        std::stringstream errmsg;
        errmsg << scanBase.str() << "Error in scan configuration, not starting a new scans: 0x"
               << std::hex << errcode << std::dec;
        LOGGER->log_message(LogManager::ERROR, errmsg.str());
        la->response->set_string("error", errmsg.str());
        return;
    }

    writeReg(la, scanBase.str() + ".START", 0x1);
    status  = readReg(la, scanBase.str() + ".MONITOR.STATUS");
    errcode = readReg(la, scanBase.str() + ".MONITOR.ERROR");
    if (errcode || !(status)) {
        std::stringstream errmsg;
        errmsg << "OH " << static_cast<uint32_t>(ohN) << ": Scan failed to start"
               << "\tERROR Code:\t " << errcode
               << "\tSTATUS Code:\t " << status;
        LOGGER->log_message(LogManager::WARNING, errmsg.str());
    }

    return;
}

void startScanModule(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");

    bool useUltra = false;
    if (request->get_key_exists("useUltra")) {
        useUltra = true;
    }

    startScanModuleLocal(&la, ohN, useUltra);

    rtxn.abort();
}

void getUltraScanResultsLocal(localArgs *la, uint32_t *outData, uint32_t ohN, uint32_t nevts, uint32_t dacMin, uint32_t dacMax, uint32_t dacStep)
{
    std::stringstream sstream;
    sstream<<ohN;
    std::string strOhN = sstream.str();

    //Set Scan Base
    std::string scanBase = "GEM_AMC.OH.OH" + strOhN + ".ScanController.ULTRA";

    //Get L1A Count & num events
    uint32_t ohnL1A_0 = readReg(la, "GEM_AMC.OH.OH" + strOhN + ".COUNTERS.T1.SENT.L1A");
    uint32_t ohnL1A   = readReg(la, "GEM_AMC.OH.OH" + strOhN + ".COUNTERS.T1.SENT.L1A");
    uint32_t numtrigs = readReg(la, scanBase + ".CONF.NTRIGS");

    //Print latency counts
    bool bIsLatency = false;
    if (readReg(la, scanBase + ".CONF.MODE") == 2) {
        bIsLatency = true;

        stdsprintf(
                "At Link %i: %d/%d L1As processed, %d%% done",
                    ohN,
                    readReg(la, "GEM_AMC.OH.OH" + strOhN + ".COUNTERS.T1.SENT.L1A") - ohnL1A_0,
                    nevts*numtrigs,
                    (readReg(la, "GEM_AMC.OH.OH" + strOhN + ".COUNTERS.T1.SENT.L1A") - ohnL1A_0)*100./(nevts*numtrigs)
                );
    }

    //Check if the scan is still running
    while (readReg(la, scanBase + ".MONITOR.STATUS") > 0) {
        stdsprintf("OH %i: Ultra scan still running (0x%x), not returning results",ohN,
                   readReg(la, scanBase + ".MONITOR.STATUS"));
        if (bIsLatency) {
            if ((readReg(la, "GEM_AMC.OH.OH" + strOhN + ".COUNTERS.T1.SENT.L1A") - ohnL1A ) > numtrigs) {
                stdsprintf("At Link %i: %d/%d L1As processed, %d%% done",
                           ohN,
                           readReg(la, "GEM_AMC.OH.OH" + strOhN + ".COUNTERS.T1.SENT.L1A") - ohnL1A_0,
                           nevts*numtrigs,
                           (readReg(la, "GEM_AMC.OH.OH" + strOhN + ".COUNTERS.T1.SENT.L1A") - ohnL1A_0)*100./(nevts*numtrigs)
                           );
                ohnL1A   =  readReg(la, "GEM_AMC.OH.OH" + strOhN + ".COUNTERS.T1.SENT.L1A");
            }
        }
        sleep(0.1);
    }

    LOGGER->log_message(LogManager::DEBUG, "OH " + strOhN + ": getUltraScanResults(...)");
    LOGGER->log_message(LogManager::DEBUG, stdsprintf("\tUltra scan status (0x%08x)\n",
                                                      readReg(la, scanBase + ".MONITOR.STATUS")));
    LOGGER->log_message(LogManager::DEBUG, stdsprintf("\tUltra scan results available (0x%06x)",
                                                      readReg(la, scanBase + ".MONITOR.READY")));

    for(uint32_t dacVal = dacMin; dacVal <= dacMax; dacVal += dacStep){
        for(int vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN){
            int idx = vfatN*(dacMax-dacMin+1)/dacStep+(dacVal-dacMin)/dacStep;
            outData[idx] = readReg(la, stdsprintf("GEM_AMC.OH.OH%i.ScanController.ULTRA.RESULTS.VFAT%i",ohN,vfatN));
            LOGGER->log_message(LogManager::DEBUG, stdsprintf("\tUltra scan results: outData[%i] = (%i, %i)",idx,(outData[idx]&0xff000000)>>24,(outData[idx]&0xffffff)));
        }
    }

    return;
}

void getUltraScanResults(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohN     = request->get_word("ohN");
    uint32_t nevts   = request->get_word("nevts");
    uint32_t dacMin  = request->get_word("dacMin");
    uint32_t dacMax  = request->get_word("dacMax");
    uint32_t dacStep = request->get_word("dacStep");

    uint32_t outData[oh::VFATS_PER_OH*(dacMax-dacMin+1)/dacStep];
    getUltraScanResultsLocal(&la, outData, ohN, nevts, dacMin, dacMax, dacStep);
    response->set_word_array("data",outData,oh::VFATS_PER_OH*(dacMax-dacMin+1)/dacStep);

    rtxn.abort();
}

void statusOHLocal(localArgs *la, uint32_t ohEnMask)
{
    std::string regs [] = {"CFG_PULSE_STRETCH ",
                           "TRIG.CTRL.SBIT_SOT_READY",
                           "TRIG.CTRL.SBIT_SOT_UNSTABLE",
                           "GBT.TX.TX_READY",
                           "GBT.RX.RX_READY",
                           "GBT.RX.RX_VALID",
                           "GBT.RX.CNT_LINK_ERR",
                           "ADC.CTRL.CNT_OVERTEMP",
                           "ADC.CTRL.CNT_VCCAUX_ALARM",
                           "ADC.CTRL.CNT_VCCINT_ALARM",
                           "CONTROL.RELEASE.DATE",
                           "CONTROL.RELEASE.VERSION.MAJOR",
                           "CONTROL.RELEASE.VERSION.MINOR",
                           "CONTROL.RELEASE.VERSION.BUILD",
                           "CONTROL.RELEASE.VERSION.GENERATION",
                           "CONTROL.SEM.CNT_SEM_CRITICAL",
                           "CONTROL.SEM.CNT_SEM_CORRECTION",
                           "TRIG.CTRL.SOT_INVERT",
                           "GBT.TX.CNT_RESPONSE_SENT",
                           "GBT.RX.CNT_REQUEST_RECEIVED",
                           "CLOCKING.CLOCKING.GBT_MMCM_LOCKED",
                           "CLOCKING.CLOCKING.LOGIC_MMCM_LOCKED",
                           "CLOCKING.CLOCKING.GBT_MMCM_UNLOCKED_CNT",
                           "CLOCKING.CLOCKING.LOGIC_MMCM_UNLOCKED_CNT"};
    std::string regName;

    for (uint8_t ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
        if ((ohEnMask >> ohN) & 0x1) {
            char regBase [100];
            sprintf(regBase, "GEM_AMC.OH.OH%i.",ohN);
            for (auto const& reg : regs) {
                regName = std::string(regBase)+reg;
                la->response->set_word(regName,readReg(la,regName));
            }
        }
    }
}

void statusOH(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohEnMask = request->get_word("ohEnMask");
    LOGGER->log_message(LogManager::INFO, "Reading OH status");

    statusOHLocal(&la, ohEnMask);
    rtxn.abort();
}

uint32_t getOptoHybridBaseAddress(localArgs *la, uint8_t const& ohN)
{
    std::stringstream regName;
    regName << "GEM_AMC.OH.OH" << static_cast<int>(ohN) << ".FPGA";
    return getAddress(la,regName.str());
}

size_t readOptoHybridRegistersLocal(localArgs *la, uint8_t const& ohN,
                                    std::string const& base,
                                    uint32_t const& nRegs,
                                    uint32_t *config)
{
    std::stringstream regName;
    regName << "GEM_AMC.OH.OH" << static_cast<int>(ohN) << ".FPGA." << base;
    const uint32_t ohbaseaddr = getOptoHybridBaseAddress(la, ohN);
    const uint32_t baseaddr   = getAddress(la, regName.str());
    for (uint32_t reg = 0; reg < nRegs; ++reg) {
      config[2*reg]   = readRawAddress(baseaddr+reg, la->response);
      config[2*reg+1] = baseaddr+reg-ohbaseaddr;
    }
    
    return 2*nRegs;
}

size_t writeOptoHybridRegistersLocal(localArgs *la, uint8_t const& ohN,
                                     std::string const& base,
                                     uint32_t const& nRegs,
                                     uint32_t *config)
{
    std::stringstream regName;
    regName << "GEM_AMC.OH.OH" << static_cast<int>(ohN) << ".FPGA." << base;
    const uint32_t ohbaseaddr = getOptoHybridBaseAddress(la, ohN);
    const uint32_t baseaddr   = getAddress(la, regName.str());
    for (uint32_t reg = 0; reg < nRegs; ++reg) {
        writeRawAddress(baseaddr+reg+config[2+reg+1], config[2+reg], la->response);
    }
    
    return 2*nRegs;
}

size_t readOptoHybridHDMIConfigLocal(localArgs *la, uint8_t const& ohN, uint32_t *config)
{
    // CONTROL.HDMI.SBIT_SEL0-7 (5 bits each)
    // CONTROL.HDMI.SBIT_MODE0-7 (2 bits each)
    // fully contained in 2 registers
    const size_t nHDMIRegs = 2;
    // const std::string base = "CONTROL.HDMI"; // FIXME, requries vX.Y.Z of OHv3 FW
    const std::string base = "CONTROL.HDMI.SBIT_SEL0";
    return readOptoHybridRegistersLocal(la, ohN, base, nHDMIRegs, config);
}

size_t readOptoHybridTAPDelayConfigLocal(localArgs *la, uint8_t const& ohN, uint32_t *config)
{
    // VFAT s-bit TAP delays, 6 blocks of 5 bits in each 32-bit register
    // 24*8 = 96 setings in 24*8/6 = 32 registers, starting from a base of
    // "TRIG.TIMING.TAP_DELAYS" suggested change, with VFATXX_BITYY" as sub-nodes
    // "TRIG.TIMING.TAP_DELAY_VFAT0_BIT0"
    const size_t nTAPRegs = 32;
    const std::string base = "TRIG.TIMING.TAP_DELAY_VFAT0_BIT0";
    return readOptoHybridRegistersLocal(la, ohN, base, nTAPRegs, config);
}

size_t readOptoHybridSOTTAPDelayConfigLocal(localArgs *la, uint8_t const& ohN, uint32_t *config)
{
    // VFAT SOT TAP delays, 6 blocks of 5 bits in each 32-bit register
    // 24 settings in 24/6 = 4 registers, starting from a base of
    // "TRIG.TIMING.SOT_TAP_DELAYS" suggested change, with VFATXX as sub-nodes?
    // "TRIG.TIMING.SOT_TAP_DELAY_VFAT0"
    const size_t nSOTTAPRegs = 4;
    const std::string base = "TRIG.TIMING.SOT_TAP_DELAY_VFAT0";
    return readOptoHybridRegistersLocal(la, ohN, base, nSOTTAPRegs, config);
}

size_t readOptoHybridConfigLocal(localArgs *la, uint8_t const& ohN, uint32_t *config)
{
    size_t wrdcnt = 0x0;
    for (auto && cfg : oh::CONFIG_MAP) {
        wrdcnt += readOptoHybridRegistersLocal(la, ohN, cfg.first, cfg.second, config+wrdcnt);
    }

    // // Alternative method, call specific functions, need to get the first two configs
    // size_t wrdcnt = 0x0;
    // wrdcnt += readOptoHybridRegistersLocal(la, ohN, "CONTROL.TTC.BXN_OFFSET", 1, config+wrdcnt);
    // wrdcnt += readOptoHybridRegistersLocal(la, ohN, "TRIG.CTRL.VFAT_MASK",    1, config+wrdcnt);
    // wrdcnt += readOptoHybridHDMIConfigLocal(la, ohN, config+wrdcnt);
    // wrdcnt += readOptoHybridTAPDelayConfigLocal(la, ohN, config+wrdcnt);
    // wrdcnt += readOptoHybridSOTTAPDelayConfigLocal(la, ohN, config+wrdcnt);
    return wrdcnt;
}

size_t writeOptoHybridConfigLocal(localArgs *la, uint8_t const& ohN, uint32_t *config)
{
    size_t wrdcnt = 0x0;
    for (auto && cfg : oh::CONFIG_MAP) {
        wrdcnt += writeOptoHybridRegistersLocal(la, ohN, cfg.first, cfg.second, config+wrdcnt);
    }

    // // Alternative method, call specific functions, need to get the first two configs
    // size_t wrdcnt = 0x0;
    // wrdcnt += writeOptoHybridRegistersLocal(la, ohN, "CONTROL.TTC.BXN_OFFSET", 1, config+wrdcnt);
    // wrdcnt += writeOptoHybridRegistersLocal(la, ohN, "TRIG.CTRL.VFAT_MASK",    1, config+wrdcnt);
    // wrdcnt += writeOptoHybridHDMIConfigLocal(la, ohN, config+wrdcnt);
    // wrdcnt += writeOptoHybridTAPDelayConfigLocal(la, ohN, config+wrdcnt);
    // wrdcnt += writeOptoHybridSOTTAPDelayConfigLocal(la, ohN, config+wrdcnt);
    return wrdcnt;
}

void readOptoHybridConfig(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    const uint32_t ohN = request->get_word("ohN");

    std::vector<uint32_t> config;
    config.resize(oh::OH_SINGLE_RAM_SIZE);
    try {
      const uint32_t cfg_sz = readOptoHybridConfigLocal(&la, ohN, config.data());
      // response->set_binarydata("config",config.data(),config.size());
      response->set_binarydata("config", config.data(), cfg_sz);
    } catch (const std::runtime_error& e) {
      std::stringstream errmsg;
      errmsg << "Error reading OptoHybrid config: " << e.what();
      LOGGER->log_message(LogManager::ERROR,errmsg.str());
      response->set_string("error", errmsg.str());
    }

    rtxn.abort();
}

void writeOptoHybridConfig(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    const uint32_t ohN = request->get_word("ohN");
    bool useRAM  = false;
    if (request->get_key_exists("useRAM"))
        useRAM = request->get_word("useRAM");

    std::vector<uint32_t> config;
    if (!useRAM) {
        if (request->get_key_exists("config")) {
            uint32_t cfg_sz = request->get_binarydata_size("config");
            config.resize(cfg_sz);
            request->get_binarydata("config", config.data(), config.size());
        } else {
            std::stringstream errmsg;
            errmsg << "Unable to configure OptoHybrid, no configuration provided";
            EMIT_RPC_ERROR(la.response, errmsg.str(), (void)"");
        }
    } else {
        config.resize(oh::OH_SINGLE_RAM_SIZE);
        try {
          const uint32_t cfg_sz = readOptoHybridConfRAMLocal(&la, config.data(), config.size(), (0x1<<ohN));
        } catch (const std::runtime_error& e) {
            std::stringstream errmsg;
            errmsg << "Error reading OptoHybrid config from RAM: " << e.what();
            EMIT_RPC_ERROR(la.response, errmsg.str(), (void)"");
        }
    }

    try {
        writeOptoHybridConfigLocal(&la, ohN, config.data());
    } catch (const std::runtime_error& e) {
        std::stringstream errmsg;
        errmsg << "Error writing OptoHybrid config: " << e.what();
        EMIT_RPC_ERROR(la.response, errmsg.str(), (void)"");
    }

    rtxn.abort();
}

extern "C" {
    const char *module_version_key = "optohybrid v1.0.1";
    int module_activity_color = 4;
    void module_init(ModuleManager *modmgr) {
        if (memhub_open(&memsvc) != 0) {
            LOGGER->log_message(LogManager::ERROR, stdsprintf("Unable to connect to memory service: %s",
                                                              memsvc_get_last_error(memsvc)));
            LOGGER->log_message(LogManager::ERROR, "Unable to load module");
            return; // Do not register our functions, we depend on memsvc.
        }
        modmgr->register_method("optohybrid", "configureScanModule",      configureScanModule);
        modmgr->register_method("optohybrid", "getUltraScanResults",      getUltraScanResults);
        modmgr->register_method("optohybrid", "printScanConfiguration",   printScanConfiguration);
        modmgr->register_method("optohybrid", "startScanModule",          startScanModule);
        modmgr->register_method("optohybrid", "statusOH",                 statusOH);
        modmgr->register_method("optohybrid", "readOptoHybridConfig",     readOptoHybridConfig);
    }
}
