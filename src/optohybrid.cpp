#include "optohybrid.h"

#include "amc.h"
#include "hw_constants.h"

void configureScanModuleLocal(localArgs * la, uint32_t ohN, uint32_t vfatN, uint32_t scanmode,
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

void printScanConfigurationLocal(localArgs * la, uint32_t ohN, bool useUltra)
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

void startScanModuleLocal(localArgs * la, uint32_t ohN, bool useUltra)
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

void getUltraScanResultsLocal(localArgs * la, uint32_t *outData, uint32_t ohN, uint32_t nevts, uint32_t dacMin, uint32_t dacMax, uint32_t dacStep)
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

void statusOHLocal(localArgs * la, uint32_t ohEnMask)
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
    LOGGER->log_message(LogManager::INFO, "Reeading OH status");

    statusOHLocal(&la, ohEnMask);
    rtxn.abort();
}

size_t readOptoHybridConfigLocal(localArgs * la, uint8_t const& ohN, uint32_t* config)
{
    // FIXME put these into HW constants?
    // "TRIG.TIMING.TAP_DELAY_VFATXX_BIY"; // XX 0 -- 23, Y 0 -- 7, each is 5 bits
    // "FPGA.CONTROL.HDMI.SBIT_SELXX";    // XX 0 -- 8, each is 5 bits
    // "FPGA.CONTROL.HDMI.SBIT_MODEXX";     // XX 0 -- 8, each is 2 bits
    // "TRIG.TIMING.SOT_TAP_DELAY_VFATXX"; // XX 0 -- 23, each is 5 bits
    // Slightly complicated, as not all of these are full registers
    const std::string cfgregs [] = {
        "FPGA.CONTROL.TTC.BXN_OFFSET", // highest 16 bits of some register
        "TRIG.CTRL.VFAT_MASK",         // 24 bits, others unused
        "FPGA.CONTROL.HDMI.SBIT_SEL0",  // 0x0:0x0000001f as above
        "FPGA.CONTROL.HDMI.SBIT_SEL1",  // 0x0:0x000003e0 as above
        "FPGA.CONTROL.HDMI.SBIT_SEL2",  // 0x0:0x00007c00 as above
        "FPGA.CONTROL.HDMI.SBIT_SEL3",  // 0x0:0x000f8000 as above
        "FPGA.CONTROL.HDMI.SBIT_SEL4",  // 0x0:0x01f00000 as above
        "FPGA.CONTROL.HDMI.SBIT_SEL5",  // 0x0:0x3e000000 as above
        "FPGA.CONTROL.HDMI.SBIT_SEL6",  // 0x1:0x0000001f as above
        "FPGA.CONTROL.HDMI.SBIT_SEL7",  // 0x1:0x000003e0 as above
        "FPGA.CONTROL.HDMI.SBIT_MODE0", // 0x1:0x00000c00 as above
        "FPGA.CONTROL.HDMI.SBIT_MODE1", // 0x1:0x00003000 as above
        "FPGA.CONTROL.HDMI.SBIT_MODE2", // 0x1:0x0000c000 as above
        "FPGA.CONTROL.HDMI.SBIT_MODE3", // 0x1:0x00030000 as above
        "FPGA.CONTROL.HDMI.SBIT_MODE4", // 0x1:0x000c0000 as above
        "FPGA.CONTROL.HDMI.SBIT_MODE5", // 0x1:0x00300000 as above
        "FPGA.CONTROL.HDMI.SBIT_MODE6", // 0x1:0x00c00000 as above
        "FPGA.CONTROL.HDMI.SBIT_MODE7", // 0x1:0x03000000 as above

        "TRIG.TIMING.TAP_DELAY_VFAT0_BIT0",   // 0x00:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT0_BIT1",   // 0x00:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT0_BIT2",   // 0x00:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT0_BIT3",   // 0x00:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT0_BIT4",   // 0x00:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT0_BIT5",   // 0x00:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT0_BIT6",   // 0x01:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT0_BIT7",   // 0x01:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT1_BIT0",   // 0x01:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT1_BIT1",   // 0x01:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT1_BIT2",   // 0x01:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT1_BIT3",   // 0x01:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT1_BIT4",   // 0x02:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT1_BIT5",   // 0x02:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT1_BIT6",   // 0x02:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT1_BIT7",   // 0x02:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT2_BIT0",   // 0x02:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT2_BIT1",   // 0x02:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT2_BIT2",   // 0x03:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT2_BIT3",   // 0x03:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT2_BIT4",   // 0x03:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT2_BIT5",   // 0x03:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT2_BIT6",   // 0x03:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT2_BIT7",   // 0x03:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT3_BIT0",   // 0x04:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT3_BIT1",   // 0x04:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT3_BIT2",   // 0x04:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT3_BIT3",   // 0x04:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT3_BIT4",   // 0x04:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT3_BIT5",   // 0x04:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT3_BIT6",   // 0x05:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT3_BIT7",   // 0x05:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT4_BIT0",   // 0x05:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT4_BIT1",   // 0x05:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT4_BIT2",   // 0x05:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT4_BIT3",   // 0x05:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT4_BIT4",   // 0x06:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT4_BIT5",   // 0x06:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT4_BIT6",   // 0x06:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT4_BIT7",   // 0x06:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT5_BIT0",   // 0x06:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT5_BIT1",   // 0x06:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT5_BIT2",   // 0x07:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT5_BIT3",   // 0x07:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT5_BIT4",   // 0x07:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT5_BIT5",   // 0x07:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT5_BIT6",   // 0x07:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT5_BIT7",   // 0x07:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT6_BIT0",   // 0x08:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT6_BIT1",   // 0x08:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT6_BIT2",   // 0x08:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT6_BIT3",   // 0x08:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT6_BIT4",   // 0x08:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT6_BIT5",   // 0x08:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT6_BIT6",   // 0x09:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT6_BIT7",   // 0x09:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT7_BIT0",   // 0x09:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT7_BIT1",   // 0x09:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT7_BIT2",   // 0x09:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT7_BIT3",   // 0x09:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT7_BIT4",   // 0x0a:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT7_BIT5",   // 0x0a:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT7_BIT6",   // 0x0a:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT7_BIT7",   // 0x0a:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT8_BIT0",   // 0x0a:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT8_BIT1",   // 0x0a:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT8_BIT2",   // 0x0b:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT8_BIT3",   // 0x0b:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT8_BIT4",   // 0x0b:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT8_BIT5",   // 0x0b:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT8_BIT6",   // 0x0b:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT8_BIT7",   // 0x0b:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT9_BIT0",   // 0x0c:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT9_BIT1",   // 0x0c:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT9_BIT2",   // 0x0c:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT9_BIT3",   // 0x0c:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT9_BIT4",   // 0x0c:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT9_BIT5",   // 0x0c:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT9_BIT6",   // 0x0d:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT9_BIT7",   // 0x0d:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT10_BIT0",  // 0x0d:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT10_BIT1",  // 0x0d:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT10_BIT2",  // 0x0d:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT10_BIT3",  // 0x0d:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT10_BIT4",  // 0x0e:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT10_BIT5",  // 0x0e:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT10_BIT6",  // 0x0e:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT10_BIT7",  // 0x0e:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT11_BIT0",  // 0x0e:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT11_BIT1",  // 0x0e:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT11_BIT2",  // 0x0f:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT11_BIT3",  // 0x0f:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT11_BIT4",  // 0x0f:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT11_BIT5",  // 0x0f:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT11_BIT6",  // 0x0f:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT11_BIT7",  // 0x0f:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT12_BIT0",  // 0x10:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT12_BIT1",  // 0x10:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT12_BIT2",  // 0x10:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT12_BIT3",  // 0x10:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT12_BIT4",  // 0x10:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT12_BIT5",  // 0x10:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT12_BIT6",  // 0x11:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT12_BIT7",  // 0x11:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT13_BIT0",  // 0x11:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT13_BIT1",  // 0x11:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT13_BIT2",  // 0x11:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT13_BIT3",  // 0x11:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT13_BIT4",  // 0x12:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT13_BIT5",  // 0x12:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT13_BIT6",  // 0x12:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT13_BIT7",  // 0x12:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT14_BIT0",  // 0x12:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT14_BIT1",  // 0x12:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT14_BIT2",  // 0x13:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT14_BIT3",  // 0x13:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT14_BIT4",  // 0x13:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT14_BIT5",  // 0x13:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT14_BIT6",  // 0x13:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT14_BIT7",  // 0x13:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT15_BIT0",  // 0x14:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT15_BIT1",  // 0x14:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT15_BIT2",  // 0x14:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT15_BIT3",  // 0x14:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT15_BIT4",  // 0x14:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT15_BIT5",  // 0x14:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT15_BIT6",  // 0x15:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT15_BIT7",  // 0x15:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT16_BIT0",  // 0x15:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT16_BIT1",  // 0x15:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT16_BIT2",  // 0x15:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT16_BIT3",  // 0x15:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT16_BIT4",  // 0x16:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT16_BIT5",  // 0x16:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT16_BIT6",  // 0x16:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT16_BIT7",  // 0x16:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT17_BIT0",  // 0x16:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT17_BIT1",  // 0x16:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT17_BIT2",  // 0x17:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT17_BIT3",  // 0x17:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT17_BIT4",  // 0x17:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT17_BIT5",  // 0x17:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT17_BIT6",  // 0x17:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT17_BIT7",  // 0x17:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT18_BIT0",  // 0x18:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT18_BIT1",  // 0x18:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT18_BIT2",  // 0x18:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT18_BIT3",  // 0x18:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT18_BIT4",  // 0x18:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT18_BIT5",  // 0x18:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT18_BIT6",  // 0x19:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT18_BIT7",  // 0x19:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT19_BIT0",  // 0x19:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT19_BIT1",  // 0x19:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT19_BIT2",  // 0x19:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT19_BIT3",  // 0x19:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT19_BIT4",  // 0x1a:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT19_BIT5",  // 0x1a:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT19_BIT6",  // 0x1a:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT19_BIT7",  // 0x1a:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT20_BIT0",  // 0x1a:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT20_BIT1",  // 0x1a:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT20_BIT2",  // 0x1b:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT20_BIT3",  // 0x1b:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT20_BIT4",  // 0x1b:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT20_BIT5",  // 0x1b:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT20_BIT6",  // 0x1b:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT20_BIT7",  // 0x1b:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT21_BIT0",  // 0x1c:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT21_BIT1",  // 0x1c:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT21_BIT2",  // 0x1c:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT21_BIT3",  // 0x1c:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT21_BIT4",  // 0x1c:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT21_BIT5",  // 0x1c:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT21_BIT6",  // 0x1d:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT21_BIT7",  // 0x1d:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT22_BIT0",  // 0x1d:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT22_BIT1",  // 0x1d:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT22_BIT2",  // 0x1d:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT22_BIT3",  // 0x1d:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT22_BIT4",  // 0x1e:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT22_BIT5",  // 0x1e:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT22_BIT6",  // 0x1e:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT22_BIT7",  // 0x1e:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT23_BIT0",  // 0x1e:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT23_BIT1",  // 0x1e:0x3e000000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT23_BIT2",  // 0x1f:0x0000001f, as above
        "TRIG.TIMING.TAP_DELAY_VFAT23_BIT3",  // 0x1f:0x000003e0, as above
        "TRIG.TIMING.TAP_DELAY_VFAT23_BIT4",  // 0x1f:0x00007c00, as above
        "TRIG.TIMING.TAP_DELAY_VFAT23_BIT5",  // 0x1f:0x000f8000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT23_BIT6",  // 0x1f:0x01f00000, as above
        "TRIG.TIMING.TAP_DELAY_VFAT23_BIT7",  // 0x1f:0x3e000000, as above

        "TRIG.TIMING.SOT_TAP_DELAY_VFAT0",    // 0x00:0x0000001f, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT1",    // 0x00:0x000003e0, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT2",    // 0x00:0x00007c00, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT3",    // 0x00:0x000f8000, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT4",    // 0x00:0x01f00000, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT5",    // 0x00:0x3e000000, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT6",    // 0x01:0x0000001f, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT7",    // 0x01:0x000003e0, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT8",    // 0x01:0x00007c00, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT9",    // 0x01:0x000f8000, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT10",   // 0x01:0x01f00000, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT11",   // 0x01:0x3e000000, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT12",   // 0x02:0x0000001f, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT13",   // 0x02:0x000003e0, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT14",   // 0x02:0x00007c00, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT15",   // 0x02:0x000f8000, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT16",   // 0x02:0x01f00000, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT17",   // 0x02:0x3e000000, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT18",   // 0x03:0x0000001f, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT19",   // 0x03:0x000003e0, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT20",   // 0x03:0x00007c00, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT21",   // 0x03:0x000f8000, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT22",   // 0x03:0x01f00000, as above
        "TRIG.TIMING.SOT_TAP_DELAY_VFAT23",   // 0x03:0x3e000000, as above
    };

    // FIXME registers and addresses, but what are stored in the DB are address table registers, not 32-bit registers
    std::string regName;

    std::stringstream regBase;
    regBase << "GEM_AMC.OH.OH." << static_cast<int>(ohN);
    size_t i = 0;
    // FIXME read based on raw address, works for reading, what about writing?
    uint32_t oldaddr = 0x0;
    for (auto const& reg : cfgregs) {
        regName = regBase.str()+reg;
        uint32_t regaddr = getAddress(la, regBase.str());
        if (regaddr != oldaddr) {
          config[i] = readReg(la, regName);
          ++i;
        }
        oldaddr = regaddr;
    }

    // FIXME read based on individual masked registers, perform necessary bit shifts
    size_t idx = 0;
    for (size_t sb = 0; sb < 8; ++sb) {
        std::stringstream regname;
        regname << "FPGA.CONTROL.HDMI.SBIT_SEL" << sb;
        uint32_t val = readReg(la,regname.str());
        config[idx] |= val<<(5*i);
        if (i%6 == 5) {
            ++idx;
        }
        ++i;
    }

    idx = i;
    i = 0;
    for (size_t sb = 0; sb < 8; ++sb) {
        std::stringstream regname;
        regname << "FPGA.CONTROL.HDMI.SBIT_MODE" << sb;
        uint32_t val = readReg(la,regname.str());
        config[idx] |= val<<(2*i);
        if (i%6 == 5) {
            ++idx;
        }
        ++i;
    }

    idx = i;
    i = 0;
    for (size_t vf = 0; vf < oh::VFATS_PER_OH; ++vf) {
        for (size_t sb = 0; sb < 8; ++sb) {
            std::stringstream regname;
            regname << "TRIG.TIMING.TAP_DELAY_VFAT" << vf
                    << "_BIT" << sb;
            uint32_t val = readReg(la,regname.str());
            config[idx] |= val<<(5*i);
            if (i%6 == 5) {
                ++idx;
            }
        }
        ++i;
    }

    i = 0;
    for (size_t vf = 0; vf < oh::VFATS_PER_OH; ++vf) {
        std::stringstream regname;
        regname << "TRIG.TIMING.SOT_TAP_DELAY_VFAT" << vf;
        uint32_t val = readReg(la,regname.str());
        config[idx] |= val<<(5*i);
        if (i%6 == 5) {
            ++idx;
        }
    }

    // std::for_each(cfgregs.begin(); cfgregs .end(); [idx=0] (int i) mutable {
    //     regName = regBase.str()+reg;
    //     config[i] = readReg(la, regName);
    //     ++idx;
    //   });

    return 0x0;
}

void readOptoHybridConfig(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");

    std::vector<uint32_t> config;
    config.resize(oh::OH_SINGLE_RAM_SIZE);
    try {
      readOptoHybridConfigLocal(&la, ohN, config.data());
      response->set_binarydata("config",config.data(),config.size());
    } catch (const std::runtime_error& e) {
      std::stringstream errmsg;
      errmsg << "Error reading OptoHybrid config: " << e.what();
      LOGGER->log_message(LogManager::ERROR,errmsg.str());
      response->set_string("error", errmsg.str());
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
    }
}
