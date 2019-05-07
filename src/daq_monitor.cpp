/*! \file daq_monitor.cpp
 *  \brief Contains functions for hardware monitoring
 *  \author Mykhailo Dalchenko <mykhailo.dalchenko@cern.ch>
 *  \author Brian Dorney <brian.l.dorney@cern.ch>
 */

#include "amc.h"
#include <chrono>
#include <thread>
#include "daq_monitor.h"
#include "hw_constants.h"
#include <string>
#include "utils.h"

void getmonTTCmainLocal(localArgs * la)
{
  LOGGER->log_message(LogManager::INFO, "Called getmonTTCmainLocal");
  la->response->set_word("MMCM_LOCKED",readReg(la,"GEM_AMC.TTC.STATUS.CLK.MMCM_LOCKED"));
  la->response->set_word("TTC_SINGLE_ERROR_CNT",readReg(la,"GEM_AMC.TTC.STATUS.TTC_SINGLE_ERROR_CNT"));
  la->response->set_word("BC0_LOCKED",readReg(la,"GEM_AMC.TTC.STATUS.BC0.LOCKED"));
  la->response->set_word("L1A_ID",readReg(la,"GEM_AMC.TTC.L1A_ID"));
  la->response->set_word("L1A_RATE",readReg(la,"GEM_AMC.TTC.L1A_RATE"));
}

void getmonTTCmain(const RPCMsg *request, RPCMsg *response)
{
  GETLOCALARGS(response);
  getmonTTCmainLocal(&la);
  rtxn.abort();
}

void getmonTRIGGERmainLocal(localArgs * la, int NOH, int ohMask)
{
  std::string t1,t2;
  la->response->set_word("OR_TRIGGER_RATE",readReg(la,"GEM_AMC.TRIGGER.STATUS.OR_TRIGGER_RATE"));
  int NOH_local = readReg(la,"GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
  if (NOH_local < NOH) NOH = NOH_local;
  for (int ohN = 0; ohN < NOH; ohN++){
    // If this Optohybrid is masked skip it
    if(!((ohMask >> ohN) & 0x1)){
      continue;
    }
    t1 = stdsprintf("OH%s.TRIGGER_RATE",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.TRIGGER.OH%s.TRIGGER_RATE",std::to_string(ohN).c_str());
    la->response->set_word(t1,readReg(la,t2));
  }
}

void getmonTRIGGERmain(const RPCMsg *request, RPCMsg *response)
{
  GETLOCALARGS(response);

  unsigned int NOH = readReg(&la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
  int ohMask = 0xfff;
  if (request->get_key_exists("ohMask")) {
    ohMask = request->get_word("ohMask");
  }

  if (request->get_key_exists("NOH")) {
    unsigned int NOH_requested = request->get_word("NOH");
    if (NOH_requested > NOH) {
      LOGGER->log_message(LogManager::WARNING, stdsprintf("NOH requested (%i) > NUM_OF_OH AMC register (%i)",NOH_requested,NOH));
      ohMask = ohMask & (0xfff >> (NOH_MAX-NOH));
    }
    NOH = NOH_requested;
  }

  getmonTRIGGERmainLocal(&la, NOH, ohMask);
  rtxn.abort();
}

void getmonTRIGGEROHmainLocal(localArgs * la, int NOH, int ohMask)
{
  std::string t1,t2;
  int NOH_local = readReg(la,"GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
  if (NOH_local < NOH) NOH = NOH_local;
  for (int ohN = 0; ohN < NOH; ohN++){
    // If this Optohybrid is masked skip it
    if (!((ohMask >> ohN) & 0x1)) {
      t1 = stdsprintf("OH%s.LINK0_MISSED_COMMA_CNT",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      t1 = stdsprintf("OH%s.LINK1_MISSED_COMMA_CNT",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      t1 = stdsprintf("OH%s.LINK0_OVERFLOW_CNT",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      t1 = stdsprintf("OH%s.LINK1_OVERFLOW_CNT",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      t1 = stdsprintf("OH%s.LINK0_UNDERFLOW_CNT",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      t1 = stdsprintf("OH%s.LINK1_UNDERFLOW_CNT",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      t1 = stdsprintf("OH%s.LINK0_SBIT_OVERFLOW_CNT",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      t1 = stdsprintf("OH%s.LINK1_SBIT_OVERFLOW_CNT",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      continue;
    }
    t1 = stdsprintf("OH%s.LINK0_MISSED_COMMA_CNT",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.TRIGGER.OH%s.LINK0_MISSED_COMMA_CNT",std::to_string(ohN).c_str());
    la->response->set_word(t1,readReg(la,t2));
    t1 = stdsprintf("OH%s.LINK1_MISSED_COMMA_CNT",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.TRIGGER.OH%s.LINK1_MISSED_COMMA_CNT",std::to_string(ohN).c_str());
    la->response->set_word(t1,readReg(la,t2));
    t1 = stdsprintf("OH%s.LINK0_OVERFLOW_CNT",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.TRIGGER.OH%s.LINK0_OVERFLOW_CNT",std::to_string(ohN).c_str());
    la->response->set_word(t1,readReg(la,t2));
    t1 = stdsprintf("OH%s.LINK1_OVERFLOW_CNT",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.TRIGGER.OH%s.LINK1_OVERFLOW_CNT",std::to_string(ohN).c_str());
    la->response->set_word(t1,readReg(la,t2));
    t1 = stdsprintf("OH%s.LINK0_UNDERFLOW_CNT",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.TRIGGER.OH%s.LINK0_UNDERFLOW_CNT",std::to_string(ohN).c_str());
    la->response->set_word(t1,readReg(la,t2));
    t1 = stdsprintf("OH%s.LINK1_UNDERFLOW_CNT",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.TRIGGER.OH%s.LINK1_UNDERFLOW_CNT",std::to_string(ohN).c_str());
    la->response->set_word(t1,readReg(la,t2));
    t1 = stdsprintf("OH%s.LINK0_SBIT_OVERFLOW_CNT",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.TRIGGER.OH%s.LINK0_SBIT_OVERFLOW_CNT",std::to_string(ohN).c_str());
    la->response->set_word(t1,readReg(la,t2));
    t1 = stdsprintf("OH%s.LINK1_SBIT_OVERFLOW_CNT",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.TRIGGER.OH%s.LINK1_SBIT_OVERFLOW_CNT",std::to_string(ohN).c_str());
    la->response->set_word(t1,readReg(la,t2));
  }
}

void getmonTRIGGEROHmain(const RPCMsg *request, RPCMsg *response)
{
  GETLOCALARGS(response);

  unsigned int NOH = readReg(&la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
  int ohMask = 0xfff;
  if (request->get_key_exists("ohMask")) {
    ohMask = request->get_word("ohMask");
  }

  if (request->get_key_exists("NOH")) {
    unsigned int NOH_requested = request->get_word("NOH");
    if (NOH_requested > NOH) {
      LOGGER->log_message(LogManager::WARNING, stdsprintf("NOH requested (%i) > NUM_OF_OH AMC register (%i)",NOH_requested,NOH));
      ohMask = ohMask & (0xfff >> (NOH_MAX-NOH));
    }
    NOH = NOH_requested;
  }

  getmonTRIGGEROHmainLocal(&la, NOH, ohMask);
  rtxn.abort();
}

void getmonDAQmainLocal(localArgs * la)
{
  la->response->set_word("DAQ_ENABLE",readReg(la,"GEM_AMC.DAQ.CONTROL.DAQ_ENABLE"));
  la->response->set_word("DAQ_LINK_READY",readReg(la,"GEM_AMC.DAQ.STATUS.DAQ_LINK_RDY"));
  la->response->set_word("DAQ_LINK_AFULL",readReg(la,"GEM_AMC.DAQ.STATUS.DAQ_LINK_AFULL"));
  la->response->set_word("DAQ_OFIFO_HAD_OFLOW",readReg(la,"GEM_AMC.DAQ.STATUS.DAQ_OUTPUT_FIFO_HAD_OVERFLOW"));
  la->response->set_word("L1A_FIFO_HAD_OFLOW",readReg(la,"GEM_AMC.DAQ.STATUS.L1A_FIFO_HAD_OVERFLOW"));
  la->response->set_word("L1A_FIFO_DATA_COUNT",readReg(la,"GEM_AMC.DAQ.EXT_STATUS.L1A_FIFO_DATA_CNT"));
  la->response->set_word("DAQ_FIFO_DATA_COUNT",readReg(la,"GEM_AMC.DAQ.EXT_STATUS.DAQ_FIFO_DATA_CNT"));
  la->response->set_word("EVENT_SENT",readReg(la,"GEM_AMC.DAQ.EXT_STATUS.EVT_SENT"));
  la->response->set_word("TTS_STATE",readReg(la,"GEM_AMC.DAQ.STATUS.TTS_STATE"));
  la->response->set_word("INPUT_ENABLE_MASK",readReg(la,"GEM_AMC.DAQ.CONTROL.INPUT_ENABLE_MASK"));
  la->response->set_word("INPUT_AUTOKILL_MASK",readReg(la,"GEM_AMC.DAQ.STATUS.INPUT_AUTOKILL_MASK"));
}

void getmonDAQmain(const RPCMsg *request, RPCMsg *response)
{
  GETLOCALARGS(response);
  getmonDAQmainLocal(&la);
  rtxn.abort();
}

void getmonDAQOHmainLocal(localArgs * la, int NOH, int ohMask)
{
  std::string t1,t2;
  int NOH_local = readReg(la,"GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
  if (NOH_local < NOH) NOH = NOH_local;
  for (int ohN = 0; ohN < NOH; ohN++){
    // If this Optohybrid is masked skip it
    if (!((ohMask >> ohN) & 0x1)) {
      t1 = stdsprintf("OH%s.STATUS.EVT_SIZE_ERR",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      t1 = stdsprintf("OH%s.STATUS.EVENT_FIFO_HAD_OFLOW",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      t1 = stdsprintf("OH%s.STATUS.INPUT_FIFO_HAD_OFLOW",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      t1 = stdsprintf("OH%s.STATUS.INPUT_FIFO_HAD_UFLOW",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      t1 = stdsprintf("OH%s.STATUS.VFAT_TOO_MANY",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      t1 = stdsprintf("OH%s.STATUS.VFAT_NO_MARKER",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      continue;
    }
    t1 = stdsprintf("OH%s.STATUS.EVT_SIZE_ERR",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.DAQ.%s",t1.c_str());
    la->response->set_word(t1,readReg(la,t2));
    t1 = stdsprintf("OH%s.STATUS.EVENT_FIFO_HAD_OFLOW",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.DAQ.%s",t1.c_str());
    la->response->set_word(t1,readReg(la,t2));
    t1 = stdsprintf("OH%s.STATUS.INPUT_FIFO_HAD_OFLOW",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.DAQ.%s",t1.c_str());
    la->response->set_word(t1,readReg(la,t2));
    t1 = stdsprintf("OH%s.STATUS.INPUT_FIFO_HAD_UFLOW",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.DAQ.%s",t1.c_str());
    la->response->set_word(t1,readReg(la,t2));
    t1 = stdsprintf("OH%s.STATUS.VFAT_TOO_MANY",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.DAQ.%s",t1.c_str());
    la->response->set_word(t1,readReg(la,t2));
    t1 = stdsprintf("OH%s.STATUS.VFAT_NO_MARKER",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.DAQ.%s",t1.c_str());
    la->response->set_word(t1,readReg(la,t2));
  }
}

void getmonDAQOHmain(const RPCMsg *request, RPCMsg *response)
{
  GETLOCALARGS(response);

  unsigned int NOH = readReg(&la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
  int ohMask = 0xfff;
  if (request->get_key_exists("ohMask")) {
    ohMask = request->get_word("ohMask");
  }

  if (request->get_key_exists("NOH")) {
    unsigned int NOH_requested = request->get_word("NOH");
    if (NOH_requested > NOH) {
      LOGGER->log_message(LogManager::WARNING, stdsprintf("NOH requested (%i) > NUM_OF_OH AMC register (%i)",NOH_requested,NOH));
      ohMask = ohMask & (0xfff >> (NOH_MAX-NOH));
    }
    NOH = NOH_requested;
  }

  getmonDAQOHmainLocal(&la, NOH, ohMask);
  rtxn.abort();
}

void getmonGBTLinkLocal(localArgs * la, int NOH, bool doReset)
{
    //Reset Requested?
    if (doReset) {
         writeReg(la, "GEM_AMC.GEM_SYSTEM.CTRL.LINK_RESET", 0x1);
    }

    std::string regName, respName; //regName used for read/write, respName sets word in RPC response
    for (int ohN=0; ohN < NOH; ++ohN) {
        for (unsigned int gbtN=0; gbtN < oh::GBTS_PER_OH; ++gbtN) {
            //Ready
            respName = stdsprintf("OH%i.GBT%i.READY",ohN,gbtN);
            regName = stdsprintf("GEM_AMC.OH_LINKS.OH%i.GBT%i_READY",ohN,gbtN);
            la->response->set_word(respName,readReg(la, regName));

            //Was not ready
            respName = stdsprintf("OH%i.GBT%i.WAS_NOT_READY",ohN,gbtN);
            regName = stdsprintf("GEM_AMC.OH_LINKS.OH%i.GBT%i_WAS_NOT_READY",ohN,gbtN);
            la->response->set_word(respName,readReg(la, regName));

            //Rx had overflow
            respName = stdsprintf("OH%i.GBT%i.RX_HAD_OVERFLOW",ohN,gbtN);
            regName = stdsprintf("GEM_AMC.OH_LINKS.OH%i.GBT%i_RX_HAD_OVERFLOW",ohN,gbtN);
            la->response->set_word(respName,readReg(la, regName));

            //Rx had underflow
            respName = stdsprintf("OH%i.GBT%i.RX_HAD_UNDERFLOW",ohN,gbtN);
            regName = stdsprintf("GEM_AMC.OH_LINKS.OH%i.GBT%i_RX_HAD_UNDERFLOW",ohN,gbtN);
            la->response->set_word(respName,readReg(la, regName));
        } //End Loop Over GBT's
    } //End Loop Over All OH's

    return;
} //End getmonGBTLinkLocal()

void getmonGBTLink(const RPCMsg *request, RPCMsg *response)
{
  GETLOCALARGS(response);

  unsigned int NOH = readReg(&la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");

  if (request->get_key_exists("NOH")) {
    unsigned int NOH_requested = request->get_word("NOH");
    if (NOH_requested > NOH) {
      LOGGER->log_message(LogManager::WARNING, stdsprintf("NOH requested (%i) > NUM_OF_OH AMC register (%i)",NOH_requested,NOH));
    }
    NOH = NOH_requested;
  }

  bool doReset = false;
  if (request->get_key_exists("doReset") ) {
    doReset = request->get_word("doReset");
  }

  getmonGBTLinkLocal(&la, NOH, doReset);
  rtxn.abort();
} //End getmonGBTLink()

void getmonOHLink(const RPCMsg *request, RPCMsg *response)
{
  GETLOCALARGS(response);

  unsigned int NOH = readReg(&la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");

  if (request->get_key_exists("NOH")) {
    unsigned int NOH_requested = request->get_word("NOH");
    if (NOH_requested > NOH) {
      LOGGER->log_message(LogManager::WARNING, stdsprintf("NOH requested (%i) > NUM_OF_OH AMC register (%i)",NOH_requested,NOH));
    }
    NOH = NOH_requested;
  }

  bool doReset = false;
  if (request->get_key_exists("doReset") ) {
    doReset = request->get_word("doReset");
  }

  getmonGBTLinkLocal(&la, NOH, doReset);
  getmonVFATLinkLocal(&la, NOH, doReset);

  rtxn.abort();
} //End getmonOHLink()

void getmonOHmainLocal(localArgs * la, int NOH, int ohMask)
{
  int NOH_local = readReg(la,"GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
  if (NOH_local < NOH) NOH = NOH_local;
  std::string t1,t2;
  for (int ohN = 0; ohN < NOH; ohN++) {
    // If this Optohybrid is masked skip it
    if (!((ohMask >> ohN) & 0x1)) {
      t1 = stdsprintf("OH%s.FW_VERSION",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      t1 = stdsprintf("OH%s.EVENT_COUNTER",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      t1 = stdsprintf("OH%s.EVENT_RATE",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      t1 = stdsprintf("OH%s.GTX.TRK_ERR",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      t1 = stdsprintf("OH%s.GTX.TRG_ERR",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      t1 = stdsprintf("OH%s.GBT.TRK_ERR",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      t1 = stdsprintf("OH%s.CORR_VFAT_BLK_CNT",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      t1 = stdsprintf("OH%s.COUNTERS.SEU",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      t1 = stdsprintf("OH%s.STATUS.SEU",std::to_string(ohN).c_str());
      la->response->set_word(t1,0xdeaddead);
      continue;
    }
    t1 = stdsprintf("OH%s.FW_VERSION",std::to_string(ohN).c_str());
    if (fw_version_check("getmonOHmain",la) == 3) {
      uint32_t t_fwver=0xffffffff;
      t2 = stdsprintf("GEM_AMC.OH.OH%s.FPGA.CONTROL.RELEASE.VERSION.MAJOR",std::to_string(ohN).c_str());
      t_fwver = t_fwver & (0x00ffffff|(readReg(la,t2) << 24));
      LOGGER->log_message(LogManager::INFO, stdsprintf("FW version MAJOR for OH%i is %08x, t_fwver is %08x ",ohN, readReg(la,t2), t_fwver));
      t2 = stdsprintf("GEM_AMC.OH.OH%s.FPGA.CONTROL.RELEASE.VERSION.MINOR",std::to_string(ohN).c_str());
      t_fwver = t_fwver & (0xff00ffff|(readReg(la,t2) << 16));
      LOGGER->log_message(LogManager::INFO, stdsprintf("FW version MINOR for OH%i is %08x, t_fwver is %08x ",ohN, readReg(la,t2), t_fwver));
      t2 = stdsprintf("GEM_AMC.OH.OH%s.FPGA.CONTROL.RELEASE.VERSION.BUILD",std::to_string(ohN).c_str());
      t_fwver = t_fwver & (0xffff00ff|(readReg(la,t2) << 8));
      LOGGER->log_message(LogManager::INFO, stdsprintf("FW version BUILD for OH%i is %08x, t_fwver is %08x ",ohN, readReg(la,t2), t_fwver));
      t2 = stdsprintf("GEM_AMC.OH.OH%s.FPGA.CONTROL.RELEASE.VERSION.GENERATION",std::to_string(ohN).c_str());
      t_fwver = t_fwver & (0xffffff00|(readReg(la,t2)));
      LOGGER->log_message(LogManager::INFO, stdsprintf("FW version GENERATION for OH%i is %08x, t_fwver is %08x ",ohN, readReg(la,t2), t_fwver));
      la->response->set_word(t1,t_fwver);
    } else {
      t2 = stdsprintf("GEM_AMC.OH.OH%s.STATUS.FW.VERSION",std::to_string(ohN).c_str());
      la->response->set_word(t1,readReg(la,t2));
    }
    t1 = stdsprintf("OH%s.EVENT_COUNTER",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.DAQ.OH%s.COUNTERS.EVN",std::to_string(ohN).c_str());
    la->response->set_word(t1,readReg(la,t2));
    t1 = stdsprintf("OH%s.EVENT_RATE",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.DAQ.OH%s.COUNTERS.EVT_RATE",std::to_string(ohN).c_str());
    la->response->set_word(t1,readReg(la,t2));
    t1 = stdsprintf("OH%s.GTX.TRK_ERR",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.OH.OH%s.COUNTERS.GTX_LINK.TRK_ERR",std::to_string(ohN).c_str());
    la->response->set_word(t1,readReg(la,t2));
    t1 = stdsprintf("OH%s.GTX.TRG_ERR",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.OH.OH%s.COUNTERS.GTX_LINK.TRG_ERR",std::to_string(ohN).c_str());
    la->response->set_word(t1,readReg(la,t2));
    t1 = stdsprintf("OH%s.GBT.TRK_ERR",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.OH.OH%s.COUNTERS.GBT_LINK.TRK_ERR",std::to_string(ohN).c_str());
    la->response->set_word(t1,readReg(la,t2));
    t1 = stdsprintf("OH%s.CORR_VFAT_BLK_CNT",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.DAQ.OH%s.COUNTERS.CORRUPT_VFAT_BLK_CNT",std::to_string(ohN).c_str());
    la->response->set_word(t1,readReg(la,t2));
    t1 = stdsprintf("OH%s.COUNTERS.SEU",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.OH.OH%s.COUNTERS.SEU",std::to_string(ohN).c_str());
    la->response->set_word(t1,readReg(la,t2));
    t1 = stdsprintf("OH%s.STATUS.SEU",std::to_string(ohN).c_str());
    t2 = stdsprintf("GEM_AMC.OH.OH%s.STATUS.SEU",std::to_string(ohN).c_str());
    la->response->set_word(t1,readReg(la,t2));
  }
}

void getmonOHmain(const RPCMsg *request, RPCMsg *response)
{
  GETLOCALARGS(response);

  unsigned int NOH = readReg(&la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
  int ohMask = 0xfff;
  if (request->get_key_exists("ohMask")) {
    ohMask = request->get_word("ohMask");
  }

  if (request->get_key_exists("NOH")) {
    unsigned int NOH_requested = request->get_word("NOH");
    if (NOH_requested > NOH) {
      LOGGER->log_message(LogManager::WARNING, stdsprintf("NOH requested (%i) > NUM_OF_OH AMC register (%i)",NOH_requested,NOH));
      ohMask = ohMask & (0xfff >> (NOH_MAX-NOH));
    }
    NOH = NOH_requested;
  }

  getmonOHmainLocal(&la, NOH, ohMask);
  rtxn.abort();
}

void getmonOHSCAmainLocal(localArgs *la, int NOH, int ohMask)
{
    std::string strRegName, strKeyName;

    //Get original monitoring mask
    uint32_t initSCAMonOffMask = readReg(la, "GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING.MONITORING_OFF");

    //Turn on monitoring for requested links
    writeReg(la, "GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING.MONITORING_OFF", (~ohMask) & 0x3fc);
    int NOH_local = readReg(la,"GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
    if (NOH_local < NOH) NOH = NOH_local;

    for (int ohN = 0; ohN < NOH; ++ohN) { //Loop over all optohybrids
        // If this Optohybrid is masked skip it
        if (!((ohMask >> ohN) & 0x1)) {
          //SCA Temperature
          strKeyName = stdsprintf("OH%i.SCA_TEMP",ohN);
          la->response->set_word(strKeyName,0xdeaddead);
          //OH Temperature Sensors
          for (int tempVal=1; tempVal <= 9; ++tempVal) { //Loop over optohybrid temperatures sensosrs
              strKeyName = stdsprintf("OH%i.BOARD_TEMP%i",ohN,tempVal);
              la->response->set_word(strKeyName, 0xdeaddead);
          } //End Loop over optohybrid temeprature sensors
          //Voltage Monitor - AVCCN
          strKeyName = stdsprintf("OH%i.AVCCN",ohN);
          la->response->set_word(strKeyName, 0xdeaddead);
          //Voltage Monitor - AVTTN
          strKeyName = stdsprintf("OH%i.AVTTN",ohN);
          la->response->set_word(strKeyName, 0xdeaddead);
          //Voltage Monitor - 1V0_INT
          strKeyName = stdsprintf("OH%i.1V0_INT",ohN);
          la->response->set_word(strKeyName, 0xdeaddead);
          //Voltage Monitor - 1V8F
          strKeyName = stdsprintf("OH%i.1V8F",ohN);
          la->response->set_word(strKeyName, 0xdeaddead);
          //Voltage Monitor - 1V5
          strKeyName = stdsprintf("OH%i.1V5",ohN);
          la->response->set_word(strKeyName, 0xdeaddead);
          //Voltage Monitor - 2V5_IO
          strKeyName = stdsprintf("OH%i.2V5_IO",ohN);
          la->response->set_word(strKeyName, 0xdeaddead);
          //Voltage Monitor - 3V0
          strKeyName = stdsprintf("OH%i.3V0",ohN);
          la->response->set_word(strKeyName, 0xdeaddead);
          //Voltage Monitor - 1V8
          strKeyName = stdsprintf("OH%i.1V8",ohN);
          la->response->set_word(strKeyName, 0xdeaddead);
          //Voltage Monitor - VTRX_RSSI2
          strKeyName = stdsprintf("OH%i.VTRX_RSSI2",ohN);
          la->response->set_word(strKeyName, 0xdeaddead);
          //Voltage Monitor - VTRX_RSSI1
          strKeyName = stdsprintf("OH%i.VTRX_RSSI1",ohN);
          la->response->set_word(strKeyName, 0xdeaddead);
          continue;
        }

        //Log Message
        LOGGER->log_message(LogManager::INFO, stdsprintf("Reading SCA Monitoring Values for OH%i",ohN));

        //SCA Temperature
        strRegName = stdsprintf("GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING.OH%i.SCA_TEMP",ohN);
        strKeyName = stdsprintf("OH%i.SCA_TEMP",ohN);
        la->response->set_word(strKeyName,readReg(la, strRegName));

        //OH Temperature Sensors
        for (int tempVal=1; tempVal <= 9; ++tempVal) { //Loop over optohybrid temperatures sensosrs
            strRegName = stdsprintf("GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING.OH%i.BOARD_TEMP%i",ohN,tempVal);
            strKeyName = stdsprintf("OH%i.BOARD_TEMP%i",ohN,tempVal);
            la->response->set_word(strKeyName, readReg(la, strRegName));
        } //End Loop over optohybrid temeprature sensors

        //Voltage Monitor - AVCCN
        strRegName = stdsprintf("GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING.OH%i.AVCCN",ohN);
        strKeyName = stdsprintf("OH%i.AVCCN",ohN);
        la->response->set_word(strKeyName, readReg(la, strRegName));

        //Voltage Monitor - AVTTN
        strRegName = stdsprintf("GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING.OH%i.AVTTN",ohN);
        strKeyName = stdsprintf("OH%i.AVTTN",ohN);
        la->response->set_word(strKeyName, readReg(la, strRegName));

        //Voltage Monitor - 1V0_INT
        strRegName = stdsprintf("GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING.OH%i.1V0_INT",ohN);
        strKeyName = stdsprintf("OH%i.1V0_INT",ohN);
        la->response->set_word(strKeyName, readReg(la, strRegName));

        //Voltage Monitor - 1V8F
        strRegName = stdsprintf("GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING.OH%i.1V8F",ohN);
        strKeyName = stdsprintf("OH%i.1V8F",ohN);
        la->response->set_word(strKeyName, readReg(la, strRegName));

        //Voltage Monitor - 1V5
        strRegName = stdsprintf("GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING.OH%i.1V5",ohN);
        strKeyName = stdsprintf("OH%i.1V5",ohN);
        la->response->set_word(strKeyName, readReg(la, strRegName));

        //Voltage Monitor - 2V5_IO
        strRegName = stdsprintf("GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING.OH%i.2V5_IO",ohN);
        strKeyName = stdsprintf("OH%i.2V5_IO",ohN);
        la->response->set_word(strKeyName, readReg(la, strRegName));

        //Voltage Monitor - 3V0
        strRegName = stdsprintf("GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING.OH%i.3V0",ohN);
        strKeyName = stdsprintf("OH%i.3V0",ohN);
        la->response->set_word(strKeyName, readReg(la, strRegName));

        //Voltage Monitor - 1V8
        strRegName = stdsprintf("GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING.OH%i.1V8",ohN);
        strKeyName = stdsprintf("OH%i.1V8",ohN);
        la->response->set_word(strKeyName, readReg(la, strRegName));

        //Voltage Monitor - VTRX_RSSI2
        strRegName = stdsprintf("GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING.OH%i.VTRX_RSSI2",ohN);
        strKeyName = stdsprintf("OH%i.VTRX_RSSI2",ohN);
        la->response->set_word(strKeyName, readReg(la, strRegName));

        //Voltage Monitor - VTRX_RSSI1
        strRegName = stdsprintf("GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING.OH%i.VTRX_RSSI1",ohN);
        strKeyName = stdsprintf("OH%i.VTRX_RSSI1",ohN);
        la->response->set_word(strKeyName, readReg(la, strRegName));
    } //End Loop over all optohybrids

    //Return monitoring to original value
    writeReg(la, "GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING.MONITORING_OFF", initSCAMonOffMask);

    return;
} //End getmonOHSCAmainLocal(...)

void getmonOHSCAmain(const RPCMsg *request, RPCMsg *response)
{
  GETLOCALARGS(response);

  unsigned int NOH = readReg(&la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
  int ohMask = 0xfff;
  if (request->get_key_exists("ohMask")) {
    ohMask = request->get_word("ohMask");
  }

  if (request->get_key_exists("NOH")) {
    unsigned int NOH_requested = request->get_word("NOH");
    if (NOH_requested > NOH) {
      LOGGER->log_message(LogManager::WARNING, stdsprintf("NOH requested (%i) > NUM_OF_OH AMC register (%i)",NOH_requested,NOH));
      ohMask = ohMask & (0xfff >> (NOH_MAX-NOH));
    }
    NOH = NOH_requested;
  }

  getmonOHSCAmainLocal(&la, NOH, ohMask);
  rtxn.abort();
}

void getmonOHSysmonLocal(localArgs *la, int NOH, int ohMask, bool doReset)
{
    std::string strKeyName;
    std::string strRegBase;
    int NOH_local = readReg(la,"GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
    if (NOH_local < NOH) NOH = NOH_local;

    if (fw_version_check("getmonOHSysmon", la) == 3) {
        for (int ohN = 0; ohN < NOH; ++ohN) { //Loop over all optohybrids
            // If this Optohybrid is masked skip it
            if (!((ohMask >> ohN) & 0x1)) {
              //Read Alarm conditions & counters - OVERTEMP
              strKeyName = stdsprintf("OH%i.OVERTEMP",ohN);
              la->response->set_word(strKeyName,0xdeaddead);
              strKeyName = stdsprintf("OH%i.CNT_OVERTEMP",ohN);
              la->response->set_word(strKeyName,0xdeaddead);
              //Read Alarm conditions & counters - VCCAUX_ALARM
              strKeyName = stdsprintf("OH%i.VCCAUX_ALARM",ohN);
              la->response->set_word(strKeyName,0xdeaddead);
              strKeyName = stdsprintf("OH%i.CNT_VCCAUX_ALARM",ohN);
              la->response->set_word(strKeyName,0xdeaddead);
              //Read Alarm conditions & counters - VCCINT_ALARM
              strKeyName = stdsprintf("OH%i.VCCINT_ALARM",ohN);
              la->response->set_word(strKeyName,0xdeaddead);
              strKeyName = stdsprintf("OH%i.CNT_VCCINT_ALARM",ohN);
              la->response->set_word(strKeyName,0xdeaddead);
              //Read Sysmon Values - Core Temperature
              strKeyName = stdsprintf("OH%i.FPGA_CORE_TEMP",ohN);
              la->response->set_word(strKeyName, 0xdeaddead);
              //Read Sysmon Values - Core Voltage
              strKeyName = stdsprintf("OH%i.FPGA_CORE_1V0",ohN);
              la->response->set_word(strKeyName, 0xdeaddead);
              //Read Sysmon Values - I/O Voltage
              strKeyName = stdsprintf("OH%i.FPGA_CORE_2V5_IO",ohN);
              la->response->set_word(strKeyName, 0xdeaddead);
              continue;
            }

            //Set regBase
            strRegBase = stdsprintf("GEM_AMC.OH.OH%i.FPGA.ADC.CTRL.",ohN);

            //Log Message
            LOGGER->log_message(LogManager::INFO, stdsprintf("Reading Sysmon Values for OH%i",ohN));

            //Issue reset??
            if (doReset) {
                LOGGER->log_message(LogManager::INFO, stdsprintf("Reseting CNT_OVERTEMP, CNT_VCCAUX_ALARM and CNT_VCCINT_ALARM for OH%i",ohN));
                writeReg(la, strRegBase+"RESET", 0x1);
            }

            //Read Alarm conditions & counters - OVERTEMP
            strKeyName = stdsprintf("OH%i.OVERTEMP",ohN);
            la->response->set_word(strKeyName,readReg(la, strRegBase + "OVERTEMP"));

            strKeyName = stdsprintf("OH%i.CNT_OVERTEMP",ohN);
            la->response->set_word(strKeyName,readReg(la, strRegBase + "CNT_OVERTEMP"));

            //Read Alarm conditions & counters - VCCAUX_ALARM
            strKeyName = stdsprintf("OH%i.VCCAUX_ALARM",ohN);
            la->response->set_word(strKeyName,readReg(la, strRegBase + "VCCAUX_ALARM"));

            strKeyName = stdsprintf("OH%i.CNT_VCCAUX_ALARM",ohN);
            la->response->set_word(strKeyName,readReg(la, strRegBase + "CNT_VCCAUX_ALARM"));

            //Read Alarm conditions & counters - VCCINT_ALARM
            strKeyName = stdsprintf("OH%i.VCCINT_ALARM",ohN);
            la->response->set_word(strKeyName,readReg(la, strRegBase + "VCCINT_ALARM"));

            strKeyName = stdsprintf("OH%i.CNT_VCCINT_ALARM",ohN);
            la->response->set_word(strKeyName,readReg(la, strRegBase + "CNT_VCCINT_ALARM"));

            //Enable Sysmon ADC Read
            writeReg(la, strRegBase + "ENABLE", 0x1);

            //Read Sysmon Values - Core Temperature
            writeReg(la, strRegBase + "ADR_IN", 0x0);
            strKeyName = stdsprintf("OH%i.FPGA_CORE_TEMP",ohN);
            la->response->set_word(strKeyName, ((readReg(la, strRegBase + "DATA_OUT") >> 6) & 0x3ff));

            //Read Sysmon Values - Core Voltage
            writeReg(la, strRegBase + "ADR_IN", 0x1);
            strKeyName = stdsprintf("OH%i.FPGA_CORE_1V0",ohN);
            la->response->set_word(strKeyName, ((readReg(la, strRegBase + "DATA_OUT") >> 6) & 0x3ff));

            //Read Sysmon Values - I/O Voltage
            writeReg(la, strRegBase + "ADR_IN", 0x2);
            strKeyName = stdsprintf("OH%i.FPGA_CORE_2V5_IO",ohN);
            la->response->set_word(strKeyName, ((readReg(la, strRegBase + "DATA_OUT") >> 6) & 0x3ff));

            //Disable Sysmon ADC Read
            writeReg(la, strRegBase + "ENABLE", 0x0);
        } //End Loop over all optohybrids
    } //End Case: v3 Electronics
    else{ //Case: v2b Electronics
        for (int ohN = 0; ohN < NOH; ++ohN) { //Loop over all optohybrids
            // If this Optohybrid is masked skip it
            if (!((ohMask >> ohN) & 0x1)) {
              //Read Sysmon Values - Core Temperature
              strKeyName = stdsprintf("OH%i.FPGA_CORE_TEMP",ohN);
              la->response->set_word(strKeyName, 0xdeaddead);
              //Read Sysmon Values - Core Voltage
              strKeyName = stdsprintf("OH%i.FPGA_CORE_1V0",ohN);
              la->response->set_word(strKeyName, 0xdeaddead);
              //Read Sysmon Values - I/O Voltage
              strKeyName = stdsprintf("OH%i.FPGA_CORE_2V5_IO",ohN);
              la->response->set_word(strKeyName, 0xdeaddead);
              continue;
            }

            //Set regBase
            strRegBase = stdsprintf("GEM_AMC.OH.OH%i.ADC.",ohN);

            //Log Message
            LOGGER->log_message(LogManager::INFO, stdsprintf("Reading Sysmon Values for OH%i",ohN));

            //Read Sysmon Values - Core Temperature
            strKeyName = stdsprintf("OH%i.FPGA_CORE_TEMP",ohN);
            la->response->set_word(strKeyName, ((readReg(la, strRegBase + "TEMP") >> 6) & 0x3ff));

            //Read Sysmon Values - Core Voltage
            strKeyName = stdsprintf("OH%i.FPGA_CORE_1V0",ohN);
            la->response->set_word(strKeyName, ((readReg(la, strRegBase + "VCCINT") >> 6) & 0x3ff));

            //Read Sysmon Values - I/O Voltage
            strKeyName = stdsprintf("OH%i.FPGA_CORE_2V5_IO",ohN);
            la->response->set_word(strKeyName, ((readReg(la, strRegBase + "VCCAUX") >> 6) & 0x3ff));
        } //End Loop all optohybrids
    } //End Case: v2b Electronics

    return;
} //End getmonOHSysmonLocal()

void getmonOHSysmon(const RPCMsg *request, RPCMsg *response)
{
  GETLOCALARGS(response);

  unsigned int NOH = readReg(&la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
  int ohMask = 0xfff;
  if (request->get_key_exists("ohMask")) {
    ohMask = request->get_word("ohMask");
  }

  if (request->get_key_exists("NOH")) {
    unsigned int NOH_requested = request->get_word("NOH");
    if (NOH_requested > NOH) {
      LOGGER->log_message(LogManager::WARNING, stdsprintf("NOH requested (%i) > NUM_OF_OH AMC register (%i)",NOH_requested,NOH));
      ohMask = ohMask & (0xfff >> (NOH_MAX-NOH));
    }
    NOH = NOH_requested;
  }

  bool doReset = request->get_word("doReset");

  getmonOHSysmonLocal(&la, NOH, ohMask, doReset);
  rtxn.abort();
} //End getmonOHSysmon()

void getmonSCALocal(localArgs * la, int NOH)
{
  int NOH_local = readReg(la,"GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
  if (NOH_local < NOH) NOH = NOH_local;
  std::string t1,t2;
  la->response->set_word("SCA.STATUS.READY", readReg(la, "GEM_AMC.SLOW_CONTROL.SCA.STATUS.READY"));
  la->response->set_word("SCA.STATUS.CRITICAL_ERROR", readReg(la, "GEM_AMC.SLOW_CONTROL.SCA.STATUS.CRITICAL_ERROR"));
  for (int i = 0; i < NOH; ++i) {
    t1 = stdsprintf("SCA.STATUS.NOT_READY_CNT_OH%s",std::to_string(i).c_str());
    t2 = stdsprintf("GEM_AMC.SLOW_CONTROL.SCA.STATUS.NOT_READY_CNT_OH%s",std::to_string(i).c_str());
    la->response->set_word(t1,readReg(la,t2));
  }
}

void getmonSCA(const RPCMsg *request, RPCMsg *response)
{
  GETLOCALARGS(response);

  int NOH = request->get_word("NOH");

  getmonSCALocal(&la, NOH);
  rtxn.abort();
} //End getmonSCA()

void getmonVFATLinkLocal(localArgs * la, int NOH, bool doReset)
{
    //Reset Requested?
    if (doReset) {
         writeReg(la, "GEM_AMC.GEM_SYSTEM.CTRL.LINK_RESET", 0x1);
         std::this_thread::sleep_for(std::chrono::microseconds(92)); // FIXME sleep for N orbits
    }

    std::string regName, respName; //regName used for read/write, respName sets word in RPC response
    bool vfatOutOfSync = false;
    for (int ohN=0; ohN < NOH; ++ohN) {
        for (unsigned int vfatN=0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
            //Sync Error Counters
            respName = stdsprintf("OH%i.VFAT%i.SYNC_ERR_CNT",ohN,vfatN);
            regName = stdsprintf("GEM_AMC.OH_LINKS.OH%i.VFAT%i.SYNC_ERR_CNT",ohN,vfatN);
            int nSyncErrs = readReg(la,regName);
            la->response->set_word(respName,nSyncErrs);
            if ( nSyncErrs > 0 ) {
                vfatOutOfSync = true;
            }

            //DAQ Event Counters
            respName = stdsprintf("OH%i.VFAT%i.DAQ_EVENT_CNT",ohN,vfatN);
            regName = stdsprintf("GEM_AMC.OH_LINKS.OH%i.VFAT%i.DAQ_EVENT_CNT",ohN,vfatN);
            la->response->set_word(respName,readReg(la,regName));

            //DAQ CRC Error Counters
            respName = stdsprintf("OH%i.VFAT%i.DAQ_CRC_ERROR_CNT",ohN,vfatN);
            regName = stdsprintf("GEM_AMC.OH_LINKS.OH%i.VFAT%i.DAQ_CRC_ERROR_CNT",ohN,vfatN);
            la->response->set_word(respName,readReg(la,regName));
        } //End Loop Over VFAT's
    } //End Loop Over All OH's

    //Set OOS flag (out of sync)
    if (vfatOutOfSync) {
        la->response->set_string("warning","One or more VFATs found to be out of sync\n");
    }

    return;
} //End getmonVFATLinkLocal()

void getmonVFATLink(const RPCMsg *request, RPCMsg *response)
{
  GETLOCALARGS(response);

  unsigned int NOH = readReg(&la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");

  if (request->get_key_exists("NOH")) {
    unsigned int NOH_requested = request->get_word("NOH");
    if (NOH_requested > NOH) {
      LOGGER->log_message(LogManager::WARNING, stdsprintf("NOH requested (%i) > NUM_OF_OH AMC register (%i)",NOH_requested,NOH));
    }
    NOH = NOH_requested;
  }

  bool doReset = false;
  if (request->get_key_exists("doReset") ) {
    doReset = request->get_word("doReset");
  }

  getmonVFATLinkLocal(&la, NOH, doReset);
  rtxn.abort();
} //End getmonVFATLink()

extern "C" {
    const char *module_version_key = "daq_monitor v1.0.1";
    int module_activity_color = 4;
    void module_init(ModuleManager *modmgr) {
        if (memhub_open(&memsvc) != 0) {
            LOGGER->log_message(LogManager::ERROR, stdsprintf("Unable to connect to memory service: %s", memsvc_get_last_error(memsvc)));
            LOGGER->log_message(LogManager::ERROR, "Unable to load module");
            return; // Do not register our functions, we depend on memsvc.
        }
        modmgr->register_method("daq_monitor", "getmonTTCmain", getmonTTCmain);
        modmgr->register_method("daq_monitor", "getmonTRIGGERmain", getmonTRIGGERmain);
        modmgr->register_method("daq_monitor", "getmonTRIGGEROHmain", getmonTRIGGEROHmain);
        modmgr->register_method("daq_monitor", "getmonDAQmain", getmonDAQmain);
        modmgr->register_method("daq_monitor", "getmonDAQOHmain", getmonDAQOHmain);
        modmgr->register_method("daq_monitor", "getmonGBTLink", getmonGBTLink);
        modmgr->register_method("daq_monitor", "getmonOHLink", getmonOHLink);
        modmgr->register_method("daq_monitor", "getmonOHmain", getmonOHmain);
        modmgr->register_method("daq_monitor", "getmonOHSCAmain", getmonOHSCAmain);
        modmgr->register_method("daq_monitor", "getmonOHSysmon", getmonOHSysmon);
        modmgr->register_method("daq_monitor", "getmonSCA", getmonSCA);
        modmgr->register_method("daq_monitor", "getmonVFATLink", getmonVFATLink);
    }
}
