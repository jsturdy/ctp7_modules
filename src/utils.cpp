#include "utils.h"

#include <log4cplus/configurator.h>
#include <log4cplus/hierarchy.h>

#include <cstdlib>

#include "xhal/rpc/register.h"

memsvc_handle_t memsvc;

// lmdb::env env = lmdb::env::create();
// env.set_mapsize(utils::LMDB_SIZE);
// std::string gem_path       = std::getenv("GEM_PATH");
// std::string lmdb_data_file = gem_path+"/address_table.mdb";
// env.open(lmdb_data_file.c_str(), 0, 0664);
// static utils::LocalArgs utils::la = std::make_unique({.rtxn = lmdb::txn::begin(env, nullptr, MDB_RDONLY),
//       .dbi  = lmdb::dbi::open(rtxn, nullptr)});


utils::LMDBSingleton::LMDBSingleton() //:
  // rtxn(lmdb::env::create().open("/dev/null",0,0664), nullptr, MDB_RDONLY),
  // dbi(rtxn, nullptr)
{
    auto env = lmdb::env::create();
    env.set_mapsize(utils::LMDB_SIZE);
    std::string gem_path       = std::getenv("GEM_PATH");
    std::string lmdb_data_file = gem_path+"/address_table.mdb";
    env.open(lmdb_data_file.c_str(), 0, 0664);
    auto trtxn = lmdb::txn::begin(env, nullptr, MDB_RDONLY);
    auto tdbi  = lmdb::dbi::open(trtxn, nullptr);
}

utils::LMDBSingleton::~LMDBSingleton()
{
    // rtxn.abort();
}

utils::LMDBSingleton& utils::LMDBSingleton::GetInstance()
{
    static utils::LMDBSingleton instance;
    return instance;
}

std::vector<std::string> utils::split(const std::string &s, char delim)
{
  std::vector<std::string> elems;
  split(s, delim, std::back_inserter(elems));
  return elems;
}

std::string utils::serialize(xhal::utils::Node n)
{
  std::stringstream node;
  node << std::hex << n.real_address << std::dec
       << "|" << n.permission
       << "|" << std::hex << n.mask << std::dec
       << "|" << n.mode
       << "|" << std::hex << n.size << std::dec;
  return node.str();
}

void initLogging()
{
    log4cplus::initialize();

    // Loading the same configuration twice seems to create issues
    // Prefer to start from scratch
    log4cplus::Logger::getDefaultHierarchy().resetConfiguration();

    // Try to get the configuration from the envrionment
    const char * const configurationFilename = std::getenv(LOGGING_CONFIGURATION_ENV);

    std::ifstream configurationFile{};
    if (configurationFilename)
        configurationFile.open(configurationFilename);

    if (configurationFile.is_open()) {
        log4cplus::PropertyConfigurator configurator{configurationFile};
        configurator.configure();
    } else {
        // Fallback to the default embedded configuration
        std::istringstream defaultConfiguration{LOGGING_DEFAULT_CONFIGURATION};
        log4cplus::PropertyConfigurator configurator{defaultConfiguration};
        configurator.configure();

        // FIXME: Cannot use a one-liner, because move constructors are disabled in the compiled library
        auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));
        LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("Impossible to read the configuration file; using the default embedded configuration."));
    }
}

void utils::update_address_table::operator()(const std::string &at_xml) const
{
  LOG4CPLUS_INFO(logger, "START UPDATE ADDRESS TABLE");
  std::string gem_path = std::getenv("GEM_PATH");
  std::string lmdb_data_file = gem_path+"/address_table.mdb/data.mdb";
  std::string lmdb_lock_file = gem_path+"/address_table.mdb/lock.mdb";
  std::string lmdb_area_file = gem_path+"/address_table.mdb";
  auto m_parser = std::make_unique<xhal::utils::XHALXMLParser>(at_xml.c_str());
  try {
    m_parser->setLogLevel(0);
    m_parser->parseXML();
  } catch (...) {
    LOG4CPLUS_ERROR(logger, "XML parser failed");
    return;
  }
  LOG4CPLUS_INFO(logger, "XML PARSING DONE ");
  std::unordered_map<std::string,xhal::utils::Node> m_parsed_at;
  m_parsed_at = m_parser->getAllNodes();
  m_parsed_at.erase("top");
  xhal::utils::Node t_node;

  // Remove old DB
  LOG4CPLUS_INFO(logger, "REMOVE OLD DB");
  std::remove(lmdb_data_file.c_str());
  std::remove(lmdb_lock_file.c_str());

  auto env = lmdb::env::create();
  env.set_mapsize(LMDB_SIZE);
  env.open(lmdb_area_file.c_str(), 0, 0664);

  LOG4CPLUS_INFO(logger, "LMDB ENV OPEN");

  lmdb::val key;
  lmdb::val value;
  auto wtxn = lmdb::txn::begin(env);
  auto wdbi  = lmdb::dbi::open(wtxn, nullptr);

  LOG4CPLUS_INFO(logger, "START ITERATING OVER MAP");

  std::string t_key;
  std::string t_value;
  for (auto it:m_parsed_at) {
    t_key = it.first;
    t_node = it.second;
    t_value = serialize(t_node);
    key.assign(t_key);
    value.assign(t_value);
    wdbi.put(wtxn, key, value);
  }
  wtxn.commit();
  LOG4CPLUS_INFO(logger, "COMMIT DB");
  wtxn.abort();
}

utils::RegInfo utils::readRegFromDB::operator()(const std::string &regName) const
{
  GETLOCALARGS();

  LOG4CPLUS_INFO(logger, "LMDB ENV OPEN");
  lmdb::val key;
  lmdb::val value;

  key.assign(regName.c_str());
  bool found = dbi.get(rtxn,key,value);
  if (found) {
    std::stringstream msg;
    msg << "Key: " << regName << " was found";
    // std::string msg = "Key: ";
    // msg += regName;
    // msg += " was found";
    LOG4CPLUS_INFO(logger, msg.str());
    std::string t_value = std::string(value.data());
    t_value = t_value.substr(0,value.size());
    std::vector<std::string> tmp = split(t_value,'|');
    uint32_t raddr = stoull(tmp[0], nullptr, 16);
    uint32_t rmask = stoull(tmp[2], nullptr, 16);
    uint32_t rsize = stoull(tmp[4], nullptr, 16);
    std::string rperm = tmp[1];
    std::string rmode = tmp[3];

    utils::RegInfo regInfo = {
      .permissions = rperm,
      .mode        = rmode,
      .address     = raddr,
      .mask        = rmask,
      .size        = rsize
    };

    msg.clear();
    msg.str(std::string());
    msg << " node " << regName << " properties: " << regInfo;
        // << "0x" << std::hex << std::setw(8) << std::setfill('0') << raddr << std::dec << "  "
        // << "0x" << std::hex << std::setw(8) << std::setfill('0') << mask  << std::dec << "  "
        // << "0x" << std::hex << std::setw(8) << std::setfill('0') << size  << std::dec << "  "
        // << rmode << "  " << rperm;
    LOG4CPLUS_DEBUG(logger, msg.str());

    rtxn.abort();
    return regInfo;
  } else {
    std::stringstream errmsg;
    errmsg << "Key: " << regName << " was NOT found!";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    rtxn.abort(); // FIXME duplicated! fixed with a `LocalArgs object that is destroyed when going out of scope
    throw std::runtime_error(errmsg.str());
  }
}

uint32_t utils::bitCheck(uint32_t word, int bit)
{
  if (bit > 31)
    throw std::invalid_argument("Invalid request to shift 32-bit word by more than 31 bits");
  return (word >> bit) & 0x1;
}

uint32_t utils::getNumNonzeroBits(uint32_t value)
{
  // https://stackoverflow.com/questions/4244274/how-do-i-count-the-number-of-zero-bits-in-an-integer
  uint32_t numNonzeroBits = 0;
  for (size_t i=0; i < CHAR_BIT * sizeof value; ++i) {
    if ((value & (1 << i)) == 1) {
      numNonzeroBits++;
    }
  }

  return numNonzeroBits;
}

bool utils::regExists(const std::string & regName, lmdb::val * db_res)
{
  GETLOCALARGS();

  lmdb::val key;
  key.assign(regName.c_str());
  if (db_res != nullptr) {
    return la->dbi.get(la->rtxn,key, *db_res);
  } else {
    lmdb::val db_res_loc;
    return la->dbi.get(la->rtxn,key,db_res_loc);
  }
}

uint32_t utils::getMask(const std::string & regName)
{
  lmdb::val db_res;
  uint32_t rmask = 0x0;
  if (regExists(regName, &db_res)) {
    std::string t_db_res = std::string(db_res.data());
    t_db_res = t_db_res.substr(0,db_res.size());
    std::vector<std::string> tmp = split(t_db_res,'|');
    rmask = stoull(tmp[2], nullptr, 16);
  } else {
    std::stringstream errmsg;
    errmsg << "Key: " << regName << " was NOT found";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
  }
  return rmask;
}

void utils::writeRawAddress(uint32_t address, uint32_t value)
{
  uint32_t data[] = {value};
  if (memhub_write(memsvc, address, 1, data) != 0) {
    std::stringstream errmsg;
    errmsg << "memsvc error: " << memsvc_get_last_error(memsvc);
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
  }
}

uint32_t utils::readRawAddress(uint32_t address)
{
  uint32_t data[1];
  if (memhub_read(memsvc, address, 1, data) != 0) {
    std::stringstream errmsg;
    errmsg << "memsvc error: " << memsvc_get_last_error(memsvc);
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
    // return 0xdeaddead;
  }
  return data[0];
}

uint32_t utils::getAddress(const std::string & regName)
{
  uint32_t raddr;
  lmdb::val db_res;
  if (regExists(regName, &db_res)) {
    std::string t_db_res = std::string(db_res.data());
    t_db_res = t_db_res.substr(0,db_res.size());
    std::vector<std::string> tmp = split(t_db_res,'|');
    raddr = stoull(tmp[0], nullptr, 16);
  } else {
    std::stringstream errmsg;
    errmsg << "Key: " << regName << " was NOT found";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
    // return 0xdeaddead;
  }
  return raddr;
}

 void utils::writeAddress(lmdb::val & db_res, uint32_t value)
{
  std::string t_db_res = std::string(db_res.data());
  t_db_res = t_db_res.substr(0,db_res.size());
  std::vector<std::string> tmp = split(t_db_res,'|');
  uint32_t data[1];
  uint32_t raddr = stoull(tmp[0], nullptr, 16);
  data[0] = value;
  if (memhub_write(memsvc, raddr, 1, data) != 0) {
    std::stringstream errmsg;
    errmsg << "memsvc error: " << memsvc_get_last_error(memsvc);
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
  }
}

uint32_t utils::readAddress(lmdb::val & db_res)
{
  std::string t_db_res = std::string(db_res.data());
  t_db_res = t_db_res.substr(0,db_res.size());
  std::vector<std::string> tmp = split(t_db_res,'|');
  uint32_t data[1];
  uint32_t raddr = stoull(tmp[0], nullptr, 16);
  int n_current_tries = 0;
  while (true) {
    if (memhub_read(memsvc, raddr, 1, data) != 0) {
      if (n_current_tries < 9) {
        n_current_tries++;
        std::stringstream errmsg;
        errmsg << "Reading reg "
               << "0x" << std::hex << std::setw(8) << std::setfill('0') << raddr << std::dec
               << " failed " << n_current_tries << " times.";
        LOG4CPLUS_WARN(logger, errmsg.str());
      } else {
        std::stringstream errmsg;
        errmsg << "memsvc error: " << memsvc_get_last_error(memsvc)
               << " failed 10 times";
        LOG4CPLUS_ERROR(logger, errmsg.str());
        throw std::runtime_error(errmsg.str());
        // return 0xdeaddead;
      }
    } else {
      break;
    }
  }
  return data[0];
}

void utils::writeRawReg(const std::string & regName, uint32_t value)
{
  lmdb::val db_res;
  if (regExists(regName, &db_res)) {
    utils::writeAddress(db_res, value);
  } else {
    std::stringstream errmsg;
    errmsg << "Key: " << regName << " was NOT found";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
  }
}

uint32_t utils::readRawReg(const std::string & regName)
{
  lmdb::val db_res;
  if (regExists(regName, &db_res)) {
    return utils::readAddress(db_res);
  } else {
    std::stringstream errmsg;
    errmsg << "Key: " << regName << " was NOT found";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
    // return 0xdeaddead;
  }
}

uint32_t utils::applyMask(uint32_t data, uint32_t mask)
{
  uint32_t result = data & mask;
  for (int i = 0; i < 32; i++) {
    if (mask & 1) {
      break;
    } else {
      mask = mask >> 1;
      result = result >> 1;
    }
  }
  return result;
}

uint32_t utils::readReg(const std::string & regName)
{
  lmdb::val db_res;
  if (regExists(regName, &db_res)) {
    std::string t_db_res = std::string(db_res.data());
    t_db_res = t_db_res.substr(0,db_res.size());
    std::vector<std::string> tmp = split(t_db_res,'|');
    std::size_t found = tmp[1].find_first_of("r");
    if (found==std::string::npos) {
      std::stringstream errmsg;
      errmsg << "No read permissions for "
             << regName << ": " << tmp[1].c_str();
      LOG4CPLUS_ERROR(logger, errmsg.str());
      throw std::runtime_error(errmsg.str());
      // return 0xdeaddead;
    }
    uint32_t data[1];
    uint32_t raddr = stoull(tmp[0], nullptr, 16);
    uint32_t rmask = stoull(tmp[2], nullptr, 16);
    if (memhub_read(memsvc, raddr, 1, data) != 0) {
      std::stringstream errmsg;
      errmsg << "read memsvc error " << memsvc_get_last_error(memsvc);
      LOG4CPLUS_ERROR(logger, errmsg.str());
      throw std::runtime_error(errmsg.str());
      // return 0xdeaddead;
    }
    if (rmask!=0xFFFFFFFF) {
      return utils::applyMask(data[0],rmask);
    } else {
      return data[0];
    }
  } else {
    std::stringstream errmsg;
    errmsg << "Key: " << regName << " was NOT found";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
    // return 0xdeaddead;
  }
}

uint32_t utils::readBlock(const std::string& regName, uint32_t* result, const uint32_t& size, const uint32_t& offset)
{
  lmdb::val db_res;
  if (regExists(regName, &db_res)) {
    std::string t_db_res = std::string(db_res.data());
    t_db_res = t_db_res.substr(0,db_res.size());
    std::vector<std::string> tmp = split(t_db_res,'|');
    uint32_t raddr = stoull(tmp[0], nullptr, 16);
    uint32_t rmask = stoull(tmp[2], nullptr, 16);
    uint32_t rsize = stoull(tmp[4], nullptr, 16);
    std::string rperm = tmp[1];
    std::string rmode = tmp[3];
    LOG4CPLUS_DEBUG(logger, stdsprintf("node %s properties: 0x%x  0x%x  0x%x  %s  %s",
                                                      regName.c_str(), raddr, rmask, rsize, rmode.c_str(), rperm.c_str()));

    if (rmask != 0xFFFFFFFF) {
      // deny block read on masked register, but what if mask is None?
      std::stringstream errmsg;
      errmsg << "Block read attempted on masked register";
      LOG4CPLUS_ERROR(logger, errmsg.str().c_str());
      throw std::range_error(errmsg.str());
    } else if (rmode.rfind("single") != std::string::npos && size > 1) {
      // only allow block read of size 1 on single registers?
      std::stringstream errmsg;
      errmsg << "Block read attempted on single register with size greater than 1";
      LOG4CPLUS_ERROR(logger, errmsg.str());
      throw std::range_error(errmsg.str());
    } else if ((offset+size) > rsize) {
      // don't allow the read to go beyond the range
      std::stringstream errmsg;
      errmsg << "Block read attempted would go beyond the size of the RAM: "
             << "raddr: 0x"    << std::hex << raddr
             << ", offset: 0x" << std::hex << offset
             << ", size: 0x"   << std::hex << size
             << ", rsize: 0x"  << std::hex << rsize;
      LOG4CPLUS_ERROR(logger, errmsg.str());
      throw std::range_error(errmsg.str());
    } else {
      if (memhub_read(memsvc, raddr+offset, size, result) != 0) {
        std::stringstream errmsg;
        errmsg << "Read memsvc error: " << memsvc_get_last_error(memsvc);
        LOG4CPLUS_ERROR(logger, errmsg.str());
        throw std::runtime_error(errmsg.str());
      } else {
        std::stringstream msg;
        msg << "Block read succeeded.";
        LOG4CPLUS_DEBUG(logger, msg.str());
      }
    }
    return size;
  }
  return 0;
}

uint32_t utils::readBlock(const uint32_t& regAddr, uint32_t* result, const uint32_t& size, const uint32_t& offset)
{
  // Might not make sense, as it would be impossible to do any validation at this level
  return 0;
}

slowCtrlErrCntVFAT utils::repeatedRegReadLocal(const std::string & regName, bool breakOnFailure, uint32_t nReads)
{
    // Create the output error counter container
    slowCtrlErrCntVFAT vfatErrs;

    // Issue a link reset to reset counters under GEM_AMC.SLOW_CONTROL.VFAT3
    writeReg("GEM_AMC.GEM_SYSTEM.CTRL.LINK_RESET", 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(90));

    for (uint32_t i=0; i<nReads; ++i) {
        // Any time a bus error occurs for VFAT slow control the TIMEOUT_ERROR_CNT will increment
        bool goodRead = (readReg(regName) != 0xdeaddead);
        std::this_thread::sleep_for(std::chrono::microseconds(20));

        if (!goodRead && breakOnFailure) {
            break;
        }
    }

    std::string baseReg     = "GEM_AMC.SLOW_CONTROL.VFAT3.";
    vfatErrs.crc            = readReg(baseReg+"CRC_ERROR_CNT");
    vfatErrs.packet         = readReg(baseReg+"PACKET_ERROR_CNT");
    vfatErrs.bitstuffing    = readReg(baseReg+"BITSTUFFING_ERROR_CNT");
    vfatErrs.timeout        = readReg(baseReg+"TIMEOUT_ERROR_CNT");
    vfatErrs.axi_strobe     = readReg(baseReg+"AXI_STROBE_ERROR_CNT");
    vfatErrs.nTransactions  = readReg(baseReg+"TRANSACTION_CNT");
    vfatErrs.sumErrors();

    return vfatErrs;
}

void writeReg(localArgs * la, const std::string & regName, uint32_t value)
{
  lmdb::val db_res;
  if (regExists(regName, &db_res)) {
    std::string t_db_res = std::string(db_res.data());
    t_db_res = t_db_res.substr(0,db_res.size());
    std::vector<std::string> tmp = split(t_db_res,'|');
    uint32_t rmask = stoull(tmp[2], nullptr, 16);
    if (rmask==0xFFFFFFFF) {
      utils::writeAddress(db_res, value);
    } else {
      uint32_t current_value = readAddress(db_res);
      if (current_value == 0xdeaddead) {
        std::stringstream errmsg;
        errmsg << "Writing masked register failed due to problem reading: " << regName;
        LOG4CPLUS_ERROR(logger, errmsg.str().c_str());
        throw std::runtime_error(errmsg.str());
      }
      int shift_amount = 0;
      uint32_t mask_copy = rmask;
      for (int i = 0; i < 32; i++) {
        if (rmask & 1) {
          break;
        } else {
          shift_amount +=1;
          rmask = rmask >> 1;
        }
      }
      uint32_t val_to_write = value << shift_amount;
      val_to_write = (val_to_write & mask_copy) | (current_value & ~mask_copy);
      utils::writeAddress(db_res, val_to_write);
    }
  } else {
    std::stringstream errmsg;
    errmsg << "Key " << regName << " was NOT found";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
  }
}

void utils::writeBlock(const std::string& regName, const uint32_t* values, const uint32_t& size, const uint32_t& offset)
{
  lmdb::val db_res;
  if (regExists(regName, &db_res)) {
    std::string t_db_res = std::string(db_res.data());
    t_db_res = t_db_res.substr(0,db_res.size());
    std::vector<std::string> tmp = split(t_db_res,'|');
    uint32_t raddr = stoull(tmp[0], nullptr, 16);
    uint32_t rmask = stoull(tmp[2], nullptr, 16);
    uint32_t rsize = stoull(tmp[4], nullptr, 16);
    std::string rmode = tmp[3];
    std::string rperm = tmp[1];
    LOG4CPLUS_DEBUG(logger, stdsprintf("node %s properties: 0x%x  0x%x  0x%x  %s  %s",
                                                      regName.c_str(), raddr, rmask, rsize, rmode.c_str(), rperm.c_str()));

    if (rmask != 0xFFFFFFFF) {
      // deny block write on masked register
      std::stringstream errmsg;
      errmsg << "Block write attempted on masked register";
      LOG4CPLUS_ERROR(logger, errmsg.str());
      throw std::runtime_error(errmsg.str());
    } else if (rmode.rfind("single") != std::string::npos && size > 1) {
      // only allow block write of size 1 on single registers
      std::stringstream errmsg;
      errmsg << "Block write attempted on single register with size greater than 1";
      LOG4CPLUS_ERROR(logger, errmsg.str());
      throw std::runtime_error(errmsg.str());
    } else if ((offset+size) > rsize) {
      // don't allow the write to go beyond the block range
      std::stringstream errmsg;
      errmsg << "Block write attempted would go beyond the size of the RAM: "
             << "raddr: 0x"    << std::hex << raddr
             << ", offset: 0x" << std::hex << offset
             << ", size: 0x"   << std::hex << size
             << ", rsize: 0x"  << std::hex << rsize;
      LOG4CPLUS_ERROR(logger, errmsg.str());
      throw std::runtime_error(errmsg.str());
    } else {
      if (memhub_write(memsvc, raddr+offset, size, values) != 0) {
        std::stringstream errmsg;
        errmsg << "Write memsvc error: " << memsvc_get_last_error(memsvc);
        LOG4CPLUS_ERROR(logger, errmsg.str());
        throw std::runtime_error(errmsg.str());
      } else {
        std::stringstream msg;
        msg << "Block write succeeded.";
        LOG4CPLUS_DEBUG(logger, msg.str());
      }
    }
  }
}

void utils::writeBlock(const uint32_t& regAddr, const uint32_t* values, const size_t& size, const uint32_t& offset)
{
  // This function doesn't make sense with an offset, why would we specify an offset when accessing by register address?
  // Maybe just to do validation checks on the size?
  return;
}

extern "C" {
    const char *module_version_key = "utils v1.0.1";
    int module_activity_color = 4;

    void module_init(ModuleManager *modmgr)
    {
        initLogging();

        if (memhub_open(&memsvc) != 0) {
            auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));
            LOG4CPLUS_ERROR(logger, LOG4CPLUS_TEXT("Unable to connect to memory service: ") << memsvc_get_last_error(memsvc));
            LOG4CPLUS_ERROR(logger, "Unable to load module");
            return;
        }

        xhal::rpc::registerMethod<utils::update_address_table>(modmgr);
        xhal::rpc::registerMethod<utils::readRegFromDB>(modmgr);
    }
}
