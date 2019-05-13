/*! \file
 *  \brief RPC module for GBT methods
 *  \author Laurent Pétré <lpetre@ulb.ac.be>
 */

#include "gbt.h"
#include "amc/blaster_ram.h"
#include "hw_constants.h"
#include "hw_constants_checks.h"

#include "moduleapi.h"
#include "memhub.h"
#include "utils.h"

#include <array>
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

void scanGBTPhases(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    const uint32_t ohN = request->get_word("ohN");
    const uint32_t nScans = request->get_word("nScans");
    const uint8_t phaseMin = request->get_word("phaseMin");
    const uint8_t phaseMax = request->get_word("phaseMax");
    const uint8_t phaseStep = request->get_word("phaseStep");

    LOGGER->log_message(LogManager::INFO, stdsprintf("Calling Local Method for OH #%u.", ohN));
    try {
      scanGBTPhasesLocal(&la, ohN, nScans, phaseMin, phaseMax, phaseStep);
    } catch (const std::runtime_error& e) {
      std::stringstream errmsg;
      errmsg << "GBT Scan for OH #" << ohN << " Failed: " << e.what();
      rtxn.abort();  // FIXME necessary?
      EMIT_RPC_ERROR(la.response, errmsg.str(), (void)"");
    }

    rtxn.abort();
}

bool scanGBTPhasesLocal(localArgs *la, const uint32_t ohN, const uint32_t N, const uint8_t phaseMin, const uint8_t phaseMax, const uint8_t phaseStep)
{
    LOGGER->log_message(LogManager::INFO, stdsprintf("Scanning the phases for OH #%u.", ohN));

    const uint32_t ohMax = readReg(la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
    if (ohN >= ohMax) {
        std::stringstream errmsg;
        errmsg << "The ohN parameter supplied (" << ohN
               << ") exceeds the number of OH's supported by the CTP7 ("
               << ohMax << ").";
        // EMIT_RPC_ERROR(la->response, errmsg.str(), true);
        throw std::runtime_error(errmsg.str());
    }

    if (gbt::checkPhase(la->response, phaseMin))
        throw std::runtime_error("Invalid phase specified");
        // return true;

    if (gbt::checkPhase(la->response, phaseMax))
        throw std::runtime_error("Invalid phase specified");
        // return true;

    std::vector<std::vector<uint32_t>> results(oh::VFATS_PER_OH, std::vector<uint32_t>(16));

    // Perform the scan
    for (uint8_t phase = phaseMin; phase <= phaseMax; phase += phaseStep) {
        // Set the new phases
        for (uint32_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
            if (writeGBTPhaseLocal(la, ohN, vfatN, phase))
                return true;
        }

        // Wait for the phases to be set
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        for (uint32_t repN = 0; repN < N; ++repN) {
            // Try to synchronize the VFAT's
            // FIXME this touches all links, is this acceptable?
            writeReg(la, "GEM_AMC.GEM_SYSTEM.CTRL.LINK_RESET", 1);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            // Check the VFAT status
            for (uint32_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
                const bool linkGood   = (readReg(la, stdsprintf("GEM_AMC.OH_LINKS.OH%hu.VFAT%hu.LINK_GOOD", ohN, vfatN)) == 1);
                const bool syncErrCnt = (readReg(la, stdsprintf("GEM_AMC.OH_LINKS.OH%hu.VFAT%hu.SYNC_ERR_CNT", ohN, vfatN)) == 0);
                const bool cfgRun     = (readReg(la, stdsprintf("GEM_AMC.OH.OH%hu.GEB.VFAT%hu.CFG_RUN", ohN, vfatN)) != 0xdeaddead);

                // If no errors, the phase is good
                if (linkGood && syncErrCnt && cfgRun)
                    ++results[vfatN][phase];
            }
        }
    }

    // Write the results to RPC keys
    for (uint32_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
        la->response->set_word_array(stdsprintf("OH%u.VFAT%u", ohN, vfatN), results[vfatN]);
    }

    return false;
}

void writeGBTConfig(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    const uint32_t ohN  = request->get_word("ohN");
    const uint32_t gbtN = request->get_word("gbtN");
    const bool useRAM   = request->get_key_exists("useRAM") ? request->get_word("useRAM") : false;

    uint32_t configSize = 0x0;
    gbt::config_t config{};

    if (!useRAM) {
      configSize = request->get_binarydata_size("config");
      request->get_binarydata("config", config.data(), config.size());
    } else {
      std::vector<uint32_t> gbtcfg;
      gbtcfg.resize(gbt::GBT_SINGLE_RAM_SIZE*oh::GBTS_PER_OH);
      configSize = readGBTConfRAMLocal(&la, gbtcfg.data(), gbtcfg.size(), (0x1<<ohN));

      // FIXME pick the correct GBT cfg out of the blob
      std::copy_n(reinterpret_cast<uint8_t*>(gbtcfg.data()+(gbtN*gbt::GBT_SINGLE_RAM_SIZE)),
                  gbt::CONFIG_SIZE, config.begin());
      configSize = config.size();
    }

    if (configSize != gbt::CONFIG_SIZE) {
        std::stringstream errmsg;
        errmsg << "The provided configuration does not have the correct size."
               << " Config is " << configSize << " registers long while this methods expects "
               << gbt::CONFIG_SIZE << " 8-bits registers.";
        rtxn.abort();
        EMIT_RPC_ERROR(la.response, errmsg.str(), (void)"");
    }

    try {
      writeGBTConfigLocal(&la, ohN, gbtN, config);
    } catch (const std::runtime_error& e) {
        std::stringstream errmsg;
        errmsg << e.what();
        rtxn.abort();  // FIXME necessary?
        EMIT_RPC_ERROR(la.response, errmsg.str(), (void)"");
    }
    rtxn.abort();
}

bool writeGBTConfigLocal(localArgs *la, const uint32_t ohN, const uint32_t gbtN, const gbt::config_t &config)
{
    LOGGER->log_message(LogManager::INFO, stdsprintf("Writing the configuration of OH #%u - GBTX #%u.", ohN, gbtN));

    const uint32_t ohMax = readReg(la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
    if (ohN >= ohMax) {
        std::stringstream errmsg;
        errmsg << "The ohN parameter supplied (" << ohN
               << ") exceeds the number of OH's supported by the CTP7 ("
               << ohMax << ").";
        throw std::runtime_error(errmsg.str());
    } else if (gbtN >= oh::GBTS_PER_OH) {
        std::stringstream errmsg;
        errmsg << "The gbtN parameter supplied (" << gbtN
               << ") exceeds the number of GBT's per OH ("
               << oh::GBTS_PER_OH << ").";
        throw std::runtime_error(errmsg.str());
    }

    for (size_t address = 0; address < gbt::CONFIG_SIZE; ++address) {
        if (writeGBTRegLocal(la, ohN, gbtN, static_cast<uint16_t>(address), config[address]))
            return true;
    }

    return false;
}

void writeAllGBTConfigs(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    const uint32_t ohN = request->get_word("ohN");
    const bool useRAM  = request->get_key_exists("useRAM") ? request->get_word("useRAM") : false;

    uint32_t configSize = 0x0;
    std::array<gbt::config_t, oh::GBTS_PER_OH> gbtcfg{};
    std::vector<uint32_t> config;

    // Extract the configuration of the GBTx chips, can anything in here throw?
    if (!useRAM) { // use passed config values
      if (request->get_key_exists("config")) { // FIXME only support one complete blob?
        configSize = request->get_binarydata_size("config");
        config.resize(configSize);
        request->get_binarydata("config", config.data(), config.size());

        size_t gbtidx = 0;
        for (auto& cfg : gbtcfg) {
          std::copy_n(reinterpret_cast<uint8_t*>(config.data()+((gbtidx++)*gbt::GBT_SINGLE_RAM_SIZE)),
                      gbt::CONFIG_SIZE, cfg.begin());
          configSize += cfg.size();
        }
      } else { // FIXME are three separate blobs supported?
        size_t gbtidx = 0;
        for (auto& cfg : gbtcfg) {
          std::stringstream cfgName;
          cfgName << "gbt" << (gbtidx++);
          // FIXME throws if key doesn't exist, no protection as above
          configSize += request->get_binarydata_size(cfgName.str());
          request->get_binarydata(cfgName.str(), cfg.data(), cfg.size());
        }
      }
    } else { // use BLASTER values
      config.resize(gbt::GBT_SINGLE_RAM_SIZE*oh::GBTS_PER_OH);
      // FIXME this might throw
      configSize = readGBTConfRAMLocal(&la, config.data(), config.size(), (0x1<<ohN));

      size_t gbtidx = 0;
      for (auto& cfg : gbtcfg) {
        std::copy_n(reinterpret_cast<uint8_t*>(config.data()+((gbtidx++)*gbt::GBT_SINGLE_RAM_SIZE)),
                    gbt::CONFIG_SIZE, cfg.begin());
        configSize += cfg.size(); // FIXME not if we do 3 lines before
      }
    }

    // Write the configuration to the GBTx chips
    size_t gbtidx = 0;
    for (auto const& cfg : gbtcfg) {
      configSize = cfg.size();
      if (configSize != gbt::CONFIG_SIZE) {
        std::stringstream errmsg;
        errmsg << "The provided configuration does not have the correct size."
               << " Config is " << configSize << " registers long while this method expects "
               << gbt::CONFIG_SIZE << " 8-bits registers.";
        rtxn.abort();  // FIXME necessary?
        EMIT_RPC_ERROR(la.response, errmsg.str(), (void)"");
      }

      try {
        writeGBTConfigLocal(&la, ohN, (gbtidx++), cfg);
      } catch (const std::runtime_error& e) {
        std::stringstream errmsg;
        errmsg << e.what();
        rtxn.abort();  // FIXME necessary?
        EMIT_RPC_ERROR(la.response, errmsg.str(), (void)"");
      }
    }
    rtxn.abort();
}

void writeGBTPhase(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    // Get the keys
    const uint32_t ohN = request->get_word("ohN");
    const uint32_t vfatN = request->get_word("vfatN");
    const uint8_t phase = request->get_word("phase");

    // Write the phase
    try {
      writeGBTPhaseLocal(&la, ohN, vfatN, phase);
    } catch (const std::runtime_error& e) {
      std::stringstream errmsg;
      errmsg << e.what();
      rtxn.abort();  // FIXME necessary?
      EMIT_RPC_ERROR(la.response, errmsg.str(), (void)"");
    }

    rtxn.abort();
} //End writeGBTPhase

bool writeGBTPhaseLocal(localArgs *la, const uint32_t ohN, const uint32_t vfatN, const uint8_t phase)
{
    LOGGER->log_message(LogManager::INFO, stdsprintf("Writing the VFAT #%u phase of OH #%u.", vfatN, ohN));

    const uint32_t ohMax = readReg(la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
    if (ohN >= ohMax) {
        std::stringstream errmsg;
        errmsg << "The ohN parameter supplied (" << ohN
               << ") exceeds the number of OH's supported by the CTP7 ("
               << ohMax << ").";
        LOGGER->log_message(LogManager::ERROR, errmsg.str());
        // EMIT_RPC_ERROR(la->response, errmsg.str(), true);
        throw std::runtime_error(errmsg.str());
    }

    if (vfatN >= oh::VFATS_PER_OH) {
        std::stringstream errmsg;
        errmsg << "The vfatN parameter supplied (" << vfatN
               << ") exceeds the number of VFAT's per OH ("
               << oh::VFATS_PER_OH << ").";
        LOGGER->log_message(LogManager::ERROR, errmsg.str());
        // EMIT_RPC_ERROR(la->response, errmsg.str(), true);
        throw std::runtime_error(errmsg.str());
    }

    // phase check
    if (gbt::checkPhase(la->response, phase))
        throw std::runtime_error("Invalid phase specified");
        // return true;

    // Write the triplicated phase registers
    const uint32_t gbtN = gbt::elinkMappings::VFAT_TO_GBT[vfatN];

    for (uint8_t regN = 0; regN < gbt::REGISTERS_PER_ELINK; ++regN) { // FIXME what is this looping?
        const uint16_t regAddress = gbt::elinkMappings::ELINK_TO_REGISTERS[gbt::elinkMappings::VFAT_TO_ELINK[vfatN]][regN];

        if (writeGBTRegLocal(la, ohN, gbtN, regAddress, phase))
            return true;
    }

    return false;
}

bool writeGBTRegLocal(localArgs *la, const uint32_t ohN, const uint32_t gbtN, const uint16_t address, const uint8_t value)
{
    if (gbtN >= oh::GBTS_PER_OH) {
        std::stringstream errmsg;
        errmsg << "The gbtN parameter supplied (" << gbtN
               << ") exceeds the number of GBT's per OH ("
               << oh::GBTS_PER_OH << ").";
        // EMIT_RPC_ERROR(la->response, errmsg.str(), true);
        throw std::runtime_error(errmsg.str());
    } else if (address >= gbt::CONFIG_SIZE) {
        std::stringstream errmsg;
        errmsg << "GBT has " << std::hex << std::setw(8) << std::setfill('0') << gbt::CONFIG_SIZE
               << " writable addresses while the provided address is "
               << std::hex << std::setw(8) << std::setfill('0') << address << std::dec
               << ".";
        // EMIT_RPC_ERROR(la->response, errmsg.str(), true);
        throw std::runtime_error(errmsg.str());
    }

    // GBT registers are 8 bits long
    writeReg(la, "GEM_AMC.SLOW_CONTROL.IC.READ_WRITE_LENGTH", 1);

    const uint32_t linkN = ohN*oh::GBTS_PER_OH + gbtN;
    writeReg(la, "GEM_AMC.SLOW_CONTROL.IC.GBTX_LINK_SELECT", linkN);

    // Write to the register
    writeReg(la, "GEM_AMC.SLOW_CONTROL.IC.ADDRESS", address);
    writeReg(la, "GEM_AMC.SLOW_CONTROL.IC.WRITE_DATA", value);
    writeReg(la, "GEM_AMC.SLOW_CONTROL.IC.EXECUTE_WRITE", 1);

    return false;
}

void writeGBTReg(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    // Get the keys
    const uint32_t ohN   = request->get_word("ohN");
    const uint32_t gbtN  = request->get_word("gbtN");
    const uint16_t addr  = request->get_word("addr");
    const uint16_t value = request->get_word("value");

    // Read the phase
    try {
      writeGBTRegLocal(&la, ohN, gbtN, addr, value);
    } catch (const std::runtime_error& e) {
      std::stringstream errmsg;
      errmsg << e.what();
      rtxn.abort();  // FIXME necessary?
      EMIT_RPC_ERROR(la.response, errmsg.str(), (void)"");
    }

    rtxn.abort();
}

void readGBTConfig(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    const uint32_t ohN  = request->get_word("ohN");
    const uint32_t gbtN = request->get_word("gbtN");

    // FIXME Is this properly initialized here?
    gbt::config_t config{};

    // Read the configuration
    try {
      readGBTConfigLocal(&la, ohN, gbtN, config);
      response->set_binarydata("config", config.data(), config.size());
    } catch (const std::range_error& e) {
      // FIXME could wrap this catch into the EMIT_RPC_ERROR function, or a STANDARD_CATCH
      response->set_string("error", e.what());
    } catch (const std::runtime_error& e) {
      response->set_string("error", e.what());
    } catch (const std::exception& e) {
      response->set_string("error", e.what());
    }

    rtxn.abort();
}

bool readGBTConfigLocal(localArgs *la, const uint32_t ohN, const uint32_t gbtN, gbt::config_t &config)
{
    LOGGER->log_message(LogManager::INFO, stdsprintf("Writing the configuration of OH #%u - GBTX #%u.", ohN, gbtN));

    const uint32_t ohMax = readReg(la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
    if (ohN >= ohMax) {
        std::stringstream errmsg;
        errmsg << "The ohN parameter supplied (" << ohN
               << ") exceeds the number of OH's supported by the CTP7 ("
               << ohMax << ").";
        // EMIT_RPC_ERROR(la->response, errmsg.str(), true);
        throw std::runtime_error(errmsg.str());
    } else if (gbtN >= oh::GBTS_PER_OH) {
        std::stringstream errmsg;
        errmsg << "The gbtN parameter supplied (" << gbtN
               << ") exceeds the number of GBT's per OH ("
               << oh::GBTS_PER_OH << ").";
        // EMIT_RPC_ERROR(la->response, errmsg.str(), true);
        throw std::runtime_error(errmsg.str());
    }

    for (size_t address = 0; address < gbt::CONFIG_SIZE; ++address) {
        config[address] = readGBTRegLocal(la, ohN, gbtN, static_cast<uint16_t>(address));
    }

    return false;
}

uint8_t readGBTRegLocal(localArgs *la, const uint32_t ohN, const uint32_t gbtN, const uint16_t address)
{
    if (gbtN >= oh::GBTS_PER_OH) {
        std::stringstream errmsg;
        errmsg << "The gbtN parameter supplied (" << gbtN
               << ") is larger than the number of GBT's per OH ("
               << oh::GBTS_PER_OH << ").";
        // EMIT_RPC_ERROR(la->response, errmsg.str(), true);
        throw std::runtime_error(errmsg.str());
    } else if (address >= gbt::CONFIG_SIZE) {
        std::stringstream errmsg;
        errmsg << "The GBT has 0x" << std::hex << std::setw(8) << std::setfill('0') << gbt::CONFIG_SIZE-1 << std::dec
               << " writable addresses while the address provided is "
               << std::hex << std::setw(8) << std::setfill('0') << address << std::dec;
        // EMIT_RPC_ERROR(la->response, errmsg.str(), true);
        throw std::runtime_error(errmsg.str());
    }

    // GBT registers are 8 bits long
    writeReg(la, "GEM_AMC.SLOW_CONTROL.IC.READ_WRITE_LENGTH", 1);

    const uint32_t linkN = ohN*oh::GBTS_PER_OH + gbtN;
    writeReg(la, "GEM_AMC.SLOW_CONTROL.IC.GBTX_LINK_SELECT", linkN);

    writeReg(la, "GEM_AMC.SLOW_CONTROL.IC.ADDRESS", address);
    writeReg(la, "GEM_AMC.SLOW_CONTROL.IC.EXECUTE_READ", 1);
    uint32_t value = readReg(la, "GEM_AMC.SLOW_CONTROL.IC.WRITE_DATA");

    return value&0xff;
}

void readGBTReg(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    const uint32_t ohN   = request->get_word("ohN");
    const uint32_t gbtN  = request->get_word("gbtN");
    const uint16_t addr  = request->get_word("addr");

    try {
      // value = readGBTRegLocal(&la, ohN, gbtN, addr);
      response->set_word("value", readGBTRegLocal(&la, ohN, gbtN, addr));
    } catch (const std::runtime_error& e) {
      std::stringstream errmsg;
      errmsg << e.what();
      rtxn.abort();  // FIXME necessary?
      EMIT_RPC_ERROR(la.response, errmsg.str(), (void)"");
    }

    rtxn.abort();
}


extern "C" {
    const char *module_version_key = "gbt v1.0.1";
    int module_activity_color = 4;
    void module_init(ModuleManager *modmgr) {
        if (memhub_open(&memsvc) != 0) {
            LOGGER->log_message(LogManager::ERROR, stdsprintf("Unable to connect to memory service: %s", memsvc_get_last_error(memsvc)));
            LOGGER->log_message(LogManager::ERROR, "Unable to load module");
            return; // Do not register our functions, we depend on memsvc.
        }
        modmgr->register_method("gbt", "writeGBTConfig", writeGBTConfig);
        modmgr->register_method("gbt", "writeGBTPhase",  writeGBTPhase);
        modmgr->register_method("gbt", "scanGBTPhases",  scanGBTPhases);
        modmgr->register_method("gbt", "readGBTConfig",  readGBTConfig);
        modmgr->register_method("gbt", "writeGBTReg",    writeGBTReg);
        modmgr->register_method("gbt", "readGBTReg",     readGBTReg);
    }
}
