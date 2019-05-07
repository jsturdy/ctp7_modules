/*! \file src/amc/blaster_ram.cpp
 *  \author Jared Sturdy <sturdy@cern.ch>
 */

#include "amc/blaster_ram.h"

#include <chrono>
#include <iomanip>
#include <string>
#include <time.h>
#include <thread>
#include <vector>

#include "hw_constants.h"

uint32_t getRAMMaxSize(localArgs *la, BLASTERTypeT const& type)
{
  uint32_t ram_size = 0x0;
  switch (type) {
  case (BLASTERType::GBT) :
    return readReg(la, "GEM_AMC.CONFIG_BLASTER.STATUS.GBT_RAM_SIZE");
  case (BLASTERType::OptoHybrid) :
    return readReg(la, "GEM_AMC.CONFIG_BLASTER.STATUS.OH_RAM_SIZE");
  case (BLASTERType::VFAT) :
    return readReg(la, "GEM_AMC.CONFIG_BLASTER.STATUS.VFAT_RAM_SIZE");
  case (BLASTERType::ALL) :
    ram_size  = getRAMMaxSize(la, BLASTERType::GBT);
    ram_size += getRAMMaxSize(la, BLASTERType::OptoHybrid);
    ram_size += getRAMMaxSize(la, BLASTERType::VFAT);
    return ram_size;
  default:
    break;
  }

  std::stringstream errmsg;
  errmsg << "Invalid BLASTER type " << type << " specified";
  LOGGER->log_message(LogManager::ERROR, errmsg.str());

  // FIXME throw? or return 0?
  throw std::range_error(errmsg.str());
  // return ram_size;
}

bool checkBLOBSize(localArgs *la, BLASTERTypeT const& type, size_t const& sz)
{
  uint32_t ram_sz = getRAMMaxSize(la, type);
  bool valid = (sz == ram_sz) ? true : false;
  return valid;
}

uint32_t getRAMBaseAddr(localArgs *la, BLASTERTypeT const& type, uint8_t const& ohN, uint8_t const& partN)
{
  uint32_t base = 0x0;
  std::string regName;
  switch (type) {
  case (BLASTERType::GBT) :
    if (partN > (oh::GBTS_PER_OH-1)) {
      std::stringstream errmsg;
      errmsg << "Invalid GBT specified: GBT" << partN << " > " << (oh::GBTS_PER_OH-1);
      LOGGER->log_message(LogManager::ERROR, errmsg.str());
      throw std::range_error(errmsg.str());
    }
    regName = stdsprintf("GEM_AMC.CONFIG_BLASTER.RAM.GBT_OH%d",int(ohN));
    base = getAddress(la, regName);
    base += gbt::GBT_SINGLE_RAM_SIZE*partN;
    return base;
  case (BLASTERType::OptoHybrid) :
    regName = stdsprintf("GEM_AMC.CONFIG_BLASTER.RAM.OH_FPGA_OH%d",int(ohN));
    base = getAddress(la, regName);
    return base;
  case (BLASTERType::VFAT) :
    if (partN > (oh::VFATS_PER_OH-1)) {
      std::stringstream errmsg;
      errmsg << "Invalid GBT specified: VFAT" << partN << " > " << (oh::VFATS_PER_OH-1);
      LOGGER->log_message(LogManager::ERROR, errmsg.str());
      throw std::range_error(errmsg.str());
    }
    regName = stdsprintf("GEM_AMC.CONFIG_BLASTER.RAM.VFAT_OH%d",int(ohN));
    base = getAddress(la, regName);
    base += vfat::VFAT_SINGLE_RAM_SIZE*partN;
    return base;
  default:
    break;
  }

  std::stringstream errmsg;
  errmsg << "Invalid BLASTER type " << type << " specified";
  LOGGER->log_message(LogManager::ERROR, errmsg.str());
  throw std::runtime_error(errmsg.str());
}

uint32_t readConfRAMLocal(localArgs *la, BLASTERTypeT const& type, uint32_t* blob, size_t const& blob_sz)
{
  uint32_t nwords = 0x0;
  // uint32_t offset = 0x0;

  if (!checkBLOBSize(la, type, blob_sz)) {
    std::stringstream errmsg;
    errmsg << "Invalid size " << blob_sz << " for BLASTER RAM BLOB";
    LOGGER->log_message(LogManager::ERROR, errmsg.str());
    // FIXME throw? or return 0?
    // throw std::runtime_error(errmsg.str());
    return nwords;
  }

  // do basic memory validation on blob
  if (!blob) {
    std::stringstream errmsg;
    errmsg << "Invalid BLOB " << std::hex << std::setw(8) << std::setfill('0') << blob
           << std::dec << " provided to write BLASTER RAM";
    LOGGER->log_message(LogManager::ERROR, errmsg.str());
    // FIXME throw? or return 0?
    // throw std::runtime_error(errmsg.str());
    return nwords;
  }

  std::stringstream regName;
  regName << "GEM_AMC.CONFIG_BLASTER.RAM";

  LOGGER->log_message(LogManager::DEBUG, stdsprintf("readConfRAM with type: 0x%x, size: 0x%x", type, blob_sz));
  switch (type) {
  case (BLASTERType::GBT):
    // return readGBTConfRAMLocal(la, blob, getRAMMaxSize(la, BLASTERType::GBT)); // FIXME function does not exist
    regName << ".GBT";
    break;
  case (BLASTERType::OptoHybrid):
    // return readOptoHybridConfRAMLocal(la, blob, getRAMMaxSize(la, BLASTERType::OptoHybrid)); // FIXME function does not exist
    regName << ".OH_FPGA";
    break;
  case (BLASTERType::VFAT):
    // return readVFATConfRAMLocal(la, blob, getRAMMaxSize(la, BLASTERType::VFAT)); // FIXME function does not exist
    regName << ".VFAT";
    break;
  case (BLASTERType::ALL):
    nwords  = readConfRAMLocal(la, BLASTERType::GBT, blob+nwords, getRAMMaxSize(la, BLASTERType::GBT));
    // offset += getRAMMaxSize(la, BLASTERType::GBT);
    nwords += readConfRAMLocal(la, BLASTERType::OptoHybrid, blob+nwords, getRAMMaxSize(la,BLASTERType::OptoHybrid));
    // offset += getRAMMaxSize(la,BLASTERType::OptoHybrid);
    nwords += readConfRAMLocal(la, BLASTERType::VFAT, blob+nwords, getRAMMaxSize(la,BLASTERType::VFAT));
    return nwords;
  default:
    // FIXME error? or assume ALL?
    // nwords  = readConfRAMLocal(la,BLASTERType::ALL, blob, blob_sz);
    std::stringstream errmsg;
    errmsg << "Invalid BLASTER RAM type "
           << std::hex << std::setw(8) << std::setfill('0') << type << std::dec
           << " selected for read.";
    LOGGER->log_message(LogManager::ERROR, errmsg.str());

    // FIXME throw? or return 0?
    // throw std::range_error(errmsg.str());
    return nwords;
  }

  nwords = readBlock(la, regName.str(), blob, blob_sz);
  LOGGER->log_message(LogManager::DEBUG, stdsprintf("read: %d words from %s", nwords, regName.str().c_str()));

  return nwords;
}

uint32_t readGBTConfRAMLocal(localArgs *la, void* gbtblob, size_t const& blob_sz, uint16_t const& ohMask)
{
  LOGGER->log_message(LogManager::DEBUG, "readGBTConfRAMLocal called");

  if (blob_sz > getRAMMaxSize(la, BLASTERType::GBT)) {
    std::stringstream errmsg;
    errmsg << "Invalid size " << blob_sz << " for GBT BLASTER RAM BLOB";
    LOGGER->log_message(LogManager::ERROR, errmsg.str());
    throw std::range_error(errmsg.str());
  }

  if (ohMask == 0x0 || ohMask == 0xfff) {
    // read full blob to VFAT RAM
    return readBlock(la, "GEM_AMC.CONFIG_BLASTER.RAM.GBT", static_cast<uint32_t*>(gbtblob), blob_sz);
  } else {
    // read blob to specific GBT RAM, as specified by ohMask, support non consecutive OptoHybrids?
    const uint32_t perblk = gbt::GBT_SINGLE_RAM_SIZE*oh::GBTS_PER_OH;
    uint32_t nwords = 0x0;
    uint32_t* blob = static_cast<uint32_t*>(gbtblob);
    for (size_t oh = 0; oh < amc::OH_PER_AMC; ++oh) {
      if ((0x1<<oh)&ohMask) {
        std::stringstream reg;
        reg << "GEM_AMC.CONFIG_BLASTER.RAM.GBT_OH" << oh;
        nwords += readBlock(la, reg.str(), blob, perblk);
        blob += perblk;
      }
    }
    return nwords;
  }
}

uint32_t readOptoHybridConfRAMLocal(localArgs *la, uint32_t* ohblob, size_t const& blob_sz, uint16_t const& ohMask)
{
  LOGGER->log_message(LogManager::DEBUG, "readOptoHybridConfRAMLocal called");

  if (blob_sz > getRAMMaxSize(la, BLASTERType::OptoHybrid)) {
    std::stringstream errmsg;
    errmsg << "Invalid size " << blob_sz << " for OptoHybrid BLASTER RAM BLOB";
    LOGGER->log_message(LogManager::ERROR, errmsg.str());
    throw std::range_error(errmsg.str());
  }

  if (ohMask == 0x0 || ohMask == 0xfff) {
    // read to all OptoHybrids
    return readBlock(la, "GEM_AMC.CONFIG_BLASTER.RAM.OH", ohblob, blob_sz);
  } else {
    // read blob to specific OptoHybrid RAM, as specified by ohMask
    uint32_t nwords = 0x0;
    uint32_t* blob = ohblob;
    const uint32_t perblk = oh::OH_SINGLE_RAM_SIZE;
    for (size_t oh = 0; oh < amc::OH_PER_AMC; ++oh) {
      if ((0x1<<oh)&ohMask) {
        std::stringstream reg;
        reg << "GEM_AMC.CONFIG_BLASTER.RAM.OH_FPGA_OH" << oh;
        nwords += readBlock(la, reg.str(), blob, perblk);
        blob += perblk;
      }
    }
    return nwords;
  }
}

uint32_t readVFATConfRAMLocal(localArgs *la, uint32_t* vfatblob, size_t const& blob_sz, uint16_t const& ohMask)
{
  LOGGER->log_message(LogManager::DEBUG, "readVFATConfRAMLocal called");

  if (blob_sz > getRAMMaxSize(la, BLASTERType::VFAT)) {
    std::stringstream errmsg;
    errmsg << "Invalid size " << blob_sz << " for VFAT BLASTER RAM BLOB";
    LOGGER->log_message(LogManager::ERROR, errmsg.str());
    throw std::range_error(errmsg.str());
  }

  if (ohMask == 0x0 || ohMask == 0xfff) {
    // read full blob to VFAT RAM
    return readBlock(la, "GEM_AMC.CONFIG_BLASTER.RAM.VFAT", vfatblob, blob_sz);
  } else {
    // read `vfatblob` to OH specific VFAT RAM, as specified by ohMask
    uint32_t nwords = 0x0;
    uint32_t* blob = vfatblob;
    const uint32_t perblk = vfat::VFAT_SINGLE_RAM_SIZE*oh::VFATS_PER_OH;
    for (size_t oh = 0; oh < amc::OH_PER_AMC; ++oh) {
      if ((0x1<<oh)&ohMask) {
        std::stringstream reg;
        reg << "GEM_AMC.CONFIG_BLASTER.RAM.VFAT_OH" << oh;
        nwords += readBlock(la, reg.str(), blob, perblk);
        blob += perblk;
      }
    }
    return nwords;
  }
}

void writeConfRAMLocal(localArgs *la, BLASTERTypeT const& type, uint32_t* blob, size_t const& blob_sz)
{
  if (!checkBLOBSize(la, type, blob_sz)) {
    std::stringstream errmsg;
    errmsg << "Invalid size " << blob_sz << " for BLASTER RAM BLOB";
    LOGGER->log_message(LogManager::ERROR, errmsg.str());
    // FIXME throw? or return 0?
    throw std::runtime_error(errmsg.str());
    // return nwords;
  }

  // do memory validation on blob
  if (!blob) {
    std::stringstream errmsg;
    errmsg << "Invalid BLOB " << std::hex << std::setw(8) << std::setfill('0') << blob
           << std::dec << " provided to write BLASTER RAM";
    LOGGER->log_message(LogManager::ERROR, errmsg.str());
    // FIXME throw? or return 0?
    throw std::runtime_error(errmsg.str());
    // return nwords;
  }

  LOGGER->log_message(LogManager::WARNING, stdsprintf("writeConfRAM with type: 0x%x, size: 0x%x", type, blob_sz));
  switch (type) {
  case (BLASTERType::GBT):
    writeGBTConfRAMLocal(la, blob, getRAMMaxSize(la, BLASTERType::GBT));
    return;
  case (BLASTERType::OptoHybrid):
    writeOptoHybridConfRAMLocal(la, blob, getRAMMaxSize(la, BLASTERType::OptoHybrid));
    return;
  case (BLASTERType::VFAT):
    writeVFATConfRAMLocal(la, blob, getRAMMaxSize(la, BLASTERType::VFAT));
    return;
  case (BLASTERType::ALL):
    LOGGER->log_message(LogManager::WARNING, "Reading the full RAM");
    writeConfRAMLocal(la, BLASTERType::GBT, blob,
                      getRAMMaxSize(la, BLASTERType::GBT));
    writeConfRAMLocal(la, BLASTERType::OptoHybrid, blob + getRAMMaxSize(la, BLASTERType::GBT),
                      getRAMMaxSize(la,BLASTERType::OptoHybrid));
    writeConfRAMLocal(la, BLASTERType::VFAT, blob + getRAMMaxSize(la, BLASTERType::GBT) + getRAMMaxSize(la,BLASTERType::OptoHybrid),
                      getRAMMaxSize(la,BLASTERType::VFAT));
    return;
  default:
    // FIXME error? or assume ALL
    // writeConfRAMLocal(la,BLASTERType::ALL, blob, blob_sz);
    std::stringstream errmsg;
    errmsg << "Invalid BLASTER RAM type "
           << std::hex << std::setw(8) << std::setfill('0') << type << std::dec
           << " selected for write.";
    LOGGER->log_message(LogManager::ERROR, errmsg.str());
    // FIXME throw? or return 0?
    throw std::runtime_error(errmsg.str());
    // return nwords;
  }
}

void writeGBTConfRAMLocal(localArgs *la, uint32_t* gbtblob, size_t const& blob_sz, uint16_t const& ohMask)
{
  LOGGER->log_message(LogManager::DEBUG, "writeGBTConfRAMLocal called");

  if (blob_sz > getRAMMaxSize(la, BLASTERType::GBT)) {
    std::stringstream errmsg;
    errmsg << "Invalid size " << blob_sz << " for GBT BLASTER RAM BLOB";
    LOGGER->log_message(LogManager::ERROR, errmsg.str());
    throw std::range_error(errmsg.str());
  }

  if (ohMask == 0x0 || ohMask == 0xfff) {
    // write full blob to VFAT RAM
    writeBlock(la, "GEM_AMC.CONFIG_BLASTER.RAM.GBT", gbtblob, blob_sz);
  } else {
    // write blob to specific GBT RAM, as specified by ohMask, support non consecutive OptoHybrids?
    const uint32_t perblk = gbt::GBT_SINGLE_RAM_SIZE*oh::GBTS_PER_OH;
    uint32_t* blob = gbtblob;
    for (size_t oh = 0; oh < amc::OH_PER_AMC; ++oh) {
      if ((0x1<<oh)&ohMask) {
        std::stringstream reg;
        reg << "GEM_AMC.CONFIG_BLASTER.RAM.GBT_OH" << oh;
        writeBlock(la, reg.str(), blob, perblk);
        blob += perblk;
      }
    }
  }

  return;
}

void writeOptoHybridConfRAMLocal(localArgs *la, uint32_t* ohblob, size_t const& blob_sz, uint16_t const& ohMask)
{
  LOGGER->log_message(LogManager::DEBUG, "writeOptoHybridConfRAMLocal called");

  if (blob_sz > getRAMMaxSize(la, BLASTERType::OptoHybrid)) {
    std::stringstream errmsg;
    errmsg << "Invalid size " << blob_sz << " for OptoHybrid BLASTER RAM BLOB";
    LOGGER->log_message(LogManager::ERROR, errmsg.str());
    throw std::range_error(errmsg.str());
  }

  if (ohMask == 0x0 || ohMask == 0xfff) {
    // write to all OptoHybrids
    writeBlock(la, "GEM_AMC.CONFIG_BLASTER.RAM.OH", ohblob, blob_sz);
  } else {
    // write blob to specific OptoHybrid RAM, as specified by ohMask
    uint32_t* blob = ohblob;
    const uint32_t perblk = oh::OH_SINGLE_RAM_SIZE;
    for (size_t oh = 0; oh < amc::OH_PER_AMC; ++oh) {
      if ((0x1<<oh)&ohMask) {
        std::stringstream reg;
        reg << "GEM_AMC.CONFIG_BLASTER.RAM.OH_FPGA_OH" << oh;
        writeBlock(la, reg.str(), blob, perblk);
        blob += perblk;
      }
    }
  }

  return;
}

void writeVFATConfRAMLocal(localArgs *la, uint32_t* vfatblob, size_t const& blob_sz, uint16_t const& ohMask)
{
  LOGGER->log_message(LogManager::DEBUG, "writeVFATConfRAMLocal called");

  if (blob_sz > getRAMMaxSize(la, BLASTERType::VFAT)) {
    std::stringstream errmsg;
    errmsg << "Invalid size " << blob_sz << " for VFAT BLASTER RAM BLOB";
    LOGGER->log_message(LogManager::ERROR, errmsg.str());
    throw std::range_error(errmsg.str());
  }

  if (ohMask == 0x0 || ohMask == 0xfff) {
    // write full blob to VFAT RAM
    writeBlock(la, "GEM_AMC.CONFIG_BLASTER.RAM.VFAT", vfatblob, blob_sz);
  } else {
    // write `vfatblob` to OH specific VFAT RAM, as specified by ohMask
    uint32_t* blob = vfatblob;
    const uint32_t perblk = vfat::VFAT_SINGLE_RAM_SIZE*oh::VFATS_PER_OH;
    for (size_t oh = 0; oh < amc::OH_PER_AMC; ++oh) {
      if ((0x1<<oh)&ohMask) {
        std::stringstream reg;
        reg << "GEM_AMC.CONFIG_BLASTER.RAM.VFAT_OH" << oh;
        writeBlock(la, reg.str(), blob, perblk);
        blob += perblk;
      }
    }
  }

  return;
}


////////////////// RPC callback methods //////////////////
void readConfRAM(const RPCMsg *request, RPCMsg *response)
{
  // struct localArgs la = getLocalArgs(response);
  GETLOCALARGS(response);

  BLASTERTypeT type = static_cast<BLASTERTypeT>(request->get_word("type"));
  LOGGER->log_message(LogManager::DEBUG, stdsprintf("BLASTERTypeT is 0x%x", type));

  uint32_t blob_sz = getRAMMaxSize(&la, type);
  LOGGER->log_message(LogManager::DEBUG, stdsprintf("blob_sz is 0x%x", blob_sz));
  std::vector<uint32_t> confblob;
  confblob.reserve(blob_sz);
  try {
    uint32_t nwords = readConfRAMLocal(&la, type, confblob.data(), blob_sz);
    response->set_binarydata("confblob", confblob.data(), nwords);
  } catch (std::runtime_error& e) {
    std::stringstream errmsg;
    errmsg << "Error reading configuration RAM: " << e.what();
    LOGGER->log_message(LogManager::ERROR, errmsg.str());
    response->set_string("error",errmsg.str());
  }
}

void writeConfRAM(const RPCMsg *request, RPCMsg *response)
{
  // struct localArgs la = getLocalArgs(response);
  GETLOCALARGS(response);

  BLASTERTypeT type = static_cast<BLASTERTypeT>(request->get_word("type"));
  LOGGER->log_message(LogManager::DEBUG, stdsprintf("BLASTERTypeT is 0x%x", type));

  uint32_t blob_sz = request->get_binarydata_size("confblob");
  uint32_t confblob[blob_sz];
  LOGGER->log_message(LogManager::DEBUG, stdsprintf("blob_sz is 0x%x", blob_sz));
  request->get_binarydata("confblob", confblob, blob_sz);
  try {
    writeConfRAMLocal(&la, type, confblob, blob_sz);
  } catch (std::runtime_error& e) {
    std::stringstream errmsg;
    errmsg << "Error writing configuration RAM: " << e.what();
    response->set_string("error",errmsg.str());
  }
}

void writeGBTConfRAM(const RPCMsg *request, RPCMsg *response)
{
  // struct localArgs la = getLocalArgs(response);
  GETLOCALARGS(response);

  uint32_t* gbtblob = nullptr;
  uint32_t blob_sz = request->get_binarydata_size("gbtblob");
  request->get_binarydata("gbtblob", gbtblob, blob_sz);
  try {
    writeGBTConfRAMLocal(&la, gbtblob, blob_sz);
  } catch (std::runtime_error& e) {
    std::stringstream errmsg;
    errmsg << "Error writing GBT configuration RAM: " << e.what();
    response->set_string("error",errmsg.str());
  }
}

void writeOptoHybridConfRAM(const RPCMsg *request, RPCMsg *response)
{
  // struct localArgs la = getLocalArgs(response);
  GETLOCALARGS(response);

  uint32_t* ohblob = nullptr;
  uint32_t blob_sz = request->get_binarydata_size("ohblob");
  request->get_binarydata("ohblob", ohblob, blob_sz);
  try {
    writeOptoHybridConfRAMLocal(&la, ohblob, blob_sz);
  } catch (std::runtime_error& e) {
    std::stringstream errmsg;
    errmsg << "Error writing OptoHybrid configuration RAM: " << e.what();
    response->set_string("error",errmsg.str());
  }
}

void writeVFATConfRAM(const RPCMsg *request, RPCMsg *response)
{
  // struct localArgs la = getLocalArgs(response);
  GETLOCALARGS(response);

  uint32_t* vfatblob = nullptr;
  uint32_t blob_sz = request->get_binarydata_size("vfatblob");
  request->get_binarydata("vfatblob", vfatblob, blob_sz);
  try {
    writeVFATConfRAMLocal(&la, vfatblob, blob_sz);
  } catch (std::runtime_error& e) {
    std::stringstream errmsg;
    errmsg << "Error writing VFAT configuration RAM: " << e.what();
    response->set_string("error",errmsg.str());
  }

  return;
}
