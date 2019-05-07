#include "utils.h"

memsvc_handle_t memsvc;

struct localArgs getLocalArgs(RPCMsg *response)
{
  auto env = lmdb::env::create();
  env.set_mapsize(LMDB_SIZE);
  std::string gem_path       = std::getenv("GEM_PATH");
  std::string lmdb_data_file = gem_path+"/address_table.mdb";
  env.open(lmdb_data_file.c_str(), 0, 0664);
  auto rtxn = lmdb::txn::begin(env, nullptr, MDB_RDONLY);
  auto dbi  = lmdb::dbi::open(rtxn, nullptr);
  struct localArgs la = {.rtxn     = rtxn,
                         .dbi      = dbi,
                         .response = response};
  return la;
}

std::vector<std::string> split(const std::string &s, char delim)
{
  std::vector<std::string> elems;
  split(s, delim, std::back_inserter(elems));
  return elems;
}

std::string serialize(xhal::utils::Node n)
{
  std::stringstream node;
  node << std::hex << n.real_address << std::dec
       << "|" << n.permission
       << "|" << std::hex << n.mask << std::dec
       << "|" << n.mode
       << "|" << std::hex << n.size << std::dec;
  return node.str();
}

void update_address_table(const RPCMsg *request, RPCMsg *response)
{
  LOGGER->log_message(LogManager::INFO, "START UPDATE ADDRESS TABLE");
  std::string at_xml = request->get_string("at_xml");
  std::string gem_path = std::getenv("GEM_PATH");
  std::string lmdb_data_file = gem_path+"/address_table.mdb/data.mdb";
  std::string lmdb_lock_file = gem_path+"/address_table.mdb/lock.mdb";
  std::string lmdb_area_file = gem_path+"/address_table.mdb";
  xhal::utils::XHALXMLParser * m_parser = new xhal::utils::XHALXMLParser(at_xml.c_str());
  try {
    m_parser->setLogLevel(0);
    m_parser->parseXML();
  } catch (...) {
    response->set_string("error", "XML parser failed");
    LOGGER->log_message(LogManager::INFO, "XML parser failed");
    return;
  }
  LOGGER->log_message(LogManager::INFO, "XML PARSING DONE");
  std::unordered_map<std::string,xhal::utils::Node> m_parsed_at;
  m_parsed_at = m_parser->getAllNodes();
  m_parsed_at.erase("top");
  xhal::utils::Node t_node;

  // Remove old DB
  LOGGER->log_message(LogManager::INFO, "REMOVE OLD DB");
  std::remove(lmdb_data_file.c_str());
  std::remove(lmdb_lock_file.c_str());

  auto env = lmdb::env::create();
  env.set_mapsize(LMDB_SIZE);
  env.open(lmdb_area_file.c_str(), 0, 0664);

  LOGGER->log_message(LogManager::INFO, "LMDB ENV OPEN");

  lmdb::val key;
  lmdb::val value;
  auto wtxn = lmdb::txn::begin(env);
  auto dbi  = lmdb::dbi::open(wtxn, nullptr);

  LOGGER->log_message(LogManager::INFO, "START ITERATING OVER MAP");

  std::string t_key;
  std::string t_value;
  for (auto it:m_parsed_at) {
    t_key = it.first;
    t_node = it.second;
    t_value = serialize(t_node);
    key.assign(t_key);
    value.assign(t_value);
    dbi.put(wtxn, key, value);
  }
  wtxn.commit();
  LOGGER->log_message(LogManager::INFO, "COMMIT DB");
  wtxn.abort();
}

void readRegFromDB(const RPCMsg *request, RPCMsg *response)
{
  std::string regName = request->get_string("reg_name");

  GETLOCALARGS(response);

  LOGGER->log_message(LogManager::INFO, "LMDB ENV OPEN");
  lmdb::val key;
  lmdb::val value;

  key.assign(regName.c_str());
  bool found = dbi.get(rtxn,key,value);
  if (found) {
    LOGGER->log_message(LogManager::INFO, stdsprintf("Key: %s is found", regName.c_str()));
    std::string t_value = std::string(value.data());
    t_value = t_value.substr(0,value.size());
    std::vector<std::string> tmp = split(t_value,'|');
    uint32_t raddr = stoull(tmp[0], nullptr, 16);
    uint32_t rmask = stoull(tmp[2], nullptr, 16);
    uint32_t rsize = stoull(tmp[4], nullptr, 16);
    std::string rperm = tmp[1];
    std::string rmode = tmp[3];
    LOGGER->log_message(LogManager::DEBUG, stdsprintf("node %s properties: 0x%x  0x%x  0x%x  %s  %s",
                                                      regName.c_str(), raddr, rmask, rsize, rmode.c_str(), rperm.c_str()));

    response->set_string("permissions", rperm);
    response->set_string("mode",        rmode);
    response->set_word("address",       raddr);
    response->set_word("mask",          rmask);
    response->set_word("size",          rsize);
  } else {
    LOGGER->log_message(LogManager::ERROR, stdsprintf("Key: %s is NOT found", regName.c_str()));
    response->set_string("error", "Register not found");
  }
  rtxn.abort();
}

uint32_t getNumNonzeroBits(uint32_t value)
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

uint32_t getMask(localArgs * la, const std::string & regName)
{
  lmdb::val key, db_res;
  bool found=false;
  key.assign(regName.c_str());
  found = la->dbi.get(la->rtxn,key,db_res);
  uint32_t rmask = 0x0;
  if (found) {
    std::string t_db_res = std::string(db_res.data());
    t_db_res = t_db_res.substr(0,db_res.size());
    std::vector<std::string> tmp = split(t_db_res,'|');
    rmask = stoull(tmp[2], nullptr, 16);
  } else {
    LOGGER->log_message(LogManager::ERROR, stdsprintf("Key: %s is NOT found", regName.c_str()));
    la->response->set_string("error", "Register not found");
  }
  return rmask;
}

void writeRawAddress(uint32_t address, uint32_t value, RPCMsg *response)
{
  uint32_t data[] = {value};
  if (memhub_write(memsvc, address, 1, data) != 0) {
    response->set_string("error", std::string("memsvc error: ")+memsvc_get_last_error(memsvc));
    LOGGER->log_message(LogManager::INFO, stdsprintf("write memsvc error: %s", memsvc_get_last_error(memsvc)));
  }
}

uint32_t readRawAddress(uint32_t address, RPCMsg* response)
{
  uint32_t data[1];
  if (memhub_read(memsvc, address, 1, data) != 0) {
    response->set_string("error", std::string("memsvc error: ")+memsvc_get_last_error(memsvc));
    LOGGER->log_message(LogManager::ERROR, stdsprintf("read memsvc error: %s", memsvc_get_last_error(memsvc)));
    return 0xdeaddead;
  }
  return data[0];
}

uint32_t getAddress(localArgs * la, const std::string & regName)
{
  lmdb::val key, db_res;
  bool found;
  key.assign(regName.c_str());
  found = la->dbi.get(la->rtxn,key,db_res);
  uint32_t raddr;
  if (found){
    std::string t_db_res = std::string(db_res.data());
    t_db_res = t_db_res.substr(0,db_res.size());
    std::vector<std::string> tmp = split(t_db_res,'|');
    raddr = stoull(tmp[0], nullptr, 16);
  } else {
    LOGGER->log_message(LogManager::ERROR, stdsprintf("Key: %s is NOT found", regName.c_str()));
    la->response->set_string("error", "Register not found");
    return 0xdeaddead;
  }
  return raddr;
}

void writeAddress(lmdb::val & db_res, uint32_t value, RPCMsg *response)
{
  std::string t_db_res = std::string(db_res.data());
  t_db_res = t_db_res.substr(0,db_res.size());
  std::vector<std::string> tmp = split(t_db_res,'|');
  uint32_t data[1];
  uint32_t raddr = stoull(tmp[0], nullptr, 16);
  data[0] = value;
  if (memhub_write(memsvc, raddr, 1, data) != 0) {
    response->set_string("error", std::string("memsvc error: ")+memsvc_get_last_error(memsvc));
    LOGGER->log_message(LogManager::INFO, stdsprintf("write memsvc error: %s", memsvc_get_last_error(memsvc)));
  }
}

uint32_t readAddress(lmdb::val & db_res, RPCMsg *response)
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
        LOGGER->log_message(LogManager::ERROR, stdsprintf("Reading reg %08X failed %i times.", raddr, n_current_tries));
      } else {
        response->set_string("error", std::string("memsvc error: ")+memsvc_get_last_error(memsvc));
        LOGGER->log_message(LogManager::ERROR, stdsprintf("read memsvc error: %s failed 10 times", memsvc_get_last_error(memsvc)));
        return 0xdeaddead;
      }
    } else {
      break;
    }
  }
  return data[0];
}

void writeRawReg(localArgs * la, const std::string & regName, uint32_t value)
{
  lmdb::val key, db_res;
  key.assign(regName.c_str());
  bool found = la->dbi.get(la->rtxn,key,db_res);
  if (found) {
    writeAddress(db_res, value, la->response);
  } else {
    LOGGER->log_message(LogManager::ERROR, stdsprintf("Key: %s is NOT found", regName.c_str()));
    la->response->set_string("error", "Register not found");
  }
}

uint32_t readRawReg(localArgs * la, const std::string & regName)
{
  lmdb::val key, db_res;
  key.assign(regName.c_str());
  bool found = la->dbi.get(la->rtxn,key,db_res);
  if (found) {
    return readAddress(db_res, la->response);
  } else {
    LOGGER->log_message(LogManager::ERROR, stdsprintf("Key: %s is NOT found", regName.c_str()));
    la->response->set_string("error", "Register not found");
    return 0xdeaddead;
  }
}

uint32_t applyMask(uint32_t data, uint32_t mask)
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

uint32_t readReg(localArgs * la, const std::string & regName)
{
  lmdb::val key, db_res;
  key.assign(regName.c_str());
  bool found = la->dbi.get(la->rtxn,key,db_res);
  if (found) {
    std::string t_db_res = std::string(db_res.data());
    t_db_res = t_db_res.substr(0,db_res.size());
    std::vector<std::string> tmp = split(t_db_res,'|');
    std::size_t found = tmp[1].find_first_of("r");
    if (found==std::string::npos) {
      // response->set_string("error", std::string("No read permissions"));
      LOGGER->log_message(LogManager::ERROR, stdsprintf("No read permissions for %s: %s", regName.c_str(), tmp[1].c_str()));
      return 0xdeaddead;
    }
    uint32_t data[1];
    uint32_t raddr = stoull(tmp[0], nullptr, 16);
    uint32_t rmask = stoull(tmp[2], nullptr, 16);
    if (memhub_read(memsvc, raddr, 1, data) != 0) {
      // response->set_string("error", std::string("memsvc error: ")+memsvc_get_last_error(memsvc));
      LOGGER->log_message(LogManager::ERROR, stdsprintf("read memsvc error: %s", memsvc_get_last_error(memsvc)));
      return 0xdeaddead;
    }
    if (rmask!=0xFFFFFFFF) {
      return applyMask(data[0],rmask);
    } else {
      return data[0];
    }
  } else {
    // response->set_string("error", "Register not found");
    LOGGER->log_message(LogManager::ERROR, stdsprintf("Key: %s is NOT found", regName.c_str()));
    return 0xdeaddead;
  }
}

uint32_t readBlock(localArgs* la, const std::string& regName, uint32_t* result, const uint32_t& size, const uint32_t& offset)
{
  lmdb::val key, db_res;
  key.assign(regName.c_str());
  bool found = la->dbi.get(la->rtxn,key,db_res);
  if (found) {
    std::string t_db_res = std::string(db_res.data());
    t_db_res = t_db_res.substr(0,db_res.size());
    std::vector<std::string> tmp = split(t_db_res,'|');
    uint32_t raddr = stoull(tmp[0], nullptr, 16);
    uint32_t rmask = stoull(tmp[2], nullptr, 16);
    uint32_t rsize = stoull(tmp[4], nullptr, 16);
    std::string rperm = tmp[1];
    std::string rmode = tmp[3];
    LOGGER->log_message(LogManager::DEBUG, stdsprintf("node %s properties: 0x%x  0x%x  0x%x  %s  %s",
                                                      regName.c_str(), raddr, rmask, rsize, rmode.c_str(), rperm.c_str()));

    if (rmask != 0xFFFFFFFF) {
      // deny block read on masked register, but what if mask is None?
      std::stringstream errmsg;
      errmsg << "Block read attempted on masked register";
      la->response->set_string("error", errmsg.str());
      LOGGER->log_message(LogManager::ERROR, stdsprintf("block read error: %s", errmsg.str().c_str()));
      // throw std::range_error(errmsg.str());
    } else if (rmode.rfind("single") != std::string::npos && size > 1) {
      // only allow block read of size 1 on single registers?
      std::stringstream errmsg;
      errmsg << "Block read attempted on single register with size greater than 1";
      la->response->set_string("error", errmsg.str());
      LOGGER->log_message(LogManager::ERROR, stdsprintf("block read error: %s", errmsg.str().c_str()));
      // throw std::range_error(errmsg.str());
    } else if ((offset+size) > rsize) {
      // don't allow the read to go beyond the range
      std::stringstream errmsg;
      errmsg << "Block read attempted would go beyond the size of the RAM: "
             << "raddr: 0x"    << std::hex << raddr
             << ", offset: 0x" << std::hex << offset
             << ", size: 0x"   << std::hex << size
             << ", rsize: 0x"  << std::hex << rsize;
      la->response->set_string("error", errmsg.str());
      LOGGER->log_message(LogManager::ERROR, stdsprintf("block read error: %s", errmsg.str().c_str()));
      // throw std::range_error(errmsg.str());
    } else {
      if (memhub_read(memsvc, raddr+offset, size, result) != 0) {
        std::stringstream errmsg;
        errmsg << "Read memsvc error: " << memsvc_get_last_error(memsvc);
        la->response->set_string("error", errmsg.str());
        LOGGER->log_message(LogManager::ERROR, stdsprintf("readBlock: %s", errmsg.str().c_str()));
        // throw std::runtime_error(errmsg.str());
      } else {
        std::stringstream msg;
        msg << "Block read succeeded.";
        la->response->set_string("debug", msg.str());
        LOGGER->log_message(LogManager::DEBUG, stdsprintf("readBlock: %s", msg.str().c_str()));
      }
    }
    return size;
  }
  return 0;
}

uint32_t readBlock(const uint32_t& regAddr, uint32_t* result, const uint32_t& size, const uint32_t& offset)
{
  // Might not make sense, as it would be impossible to do any validation at this level
  return 0;
}

void writeReg(localArgs * la, const std::string & regName, uint32_t value)
{
  lmdb::val key, db_res;
  key.assign(regName.c_str());
  bool found = la->dbi.get(la->rtxn,key,db_res);
  if (found) {
    std::string t_db_res = std::string(db_res.data());
    t_db_res = t_db_res.substr(0,db_res.size());
    std::vector<std::string> tmp = split(t_db_res,'|');
    uint32_t rmask = stoull(tmp[2], nullptr, 16);
    if (rmask==0xFFFFFFFF) {
      writeAddress(db_res, value, la->response);
    } else {
      uint32_t current_value = readAddress(db_res, la->response);
      if (current_value == 0xdeaddead) {
        std::stringstream errmsg;
        errmsg << "Writing masked register failed due to problem reading: " << regName;
        la->response->set_string("error", errmsg.str());
        LOGGER->log_message(LogManager::ERROR, errmsg.str().c_str());
        return;
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
      writeAddress(db_res, val_to_write, la->response);
    }
  } else {
    std::stringstream errmsg;
    errmsg << "Register " << regName << " key not found";
    la->response->set_string("error", errmsg.str());
    LOGGER->log_message(LogManager::ERROR, errmsg.str().c_str());
  }
}

void writeBlock(localArgs* la, const std::string& regName, const uint32_t* values, const uint32_t& size, const uint32_t& offset)
{
  lmdb::val key, db_res;
  key.assign(regName.c_str());
  bool found = la->dbi.get(la->rtxn,key,db_res);
  if (found) {
    std::string t_db_res = std::string(db_res.data());
    t_db_res = t_db_res.substr(0,db_res.size());
    std::vector<std::string> tmp = split(t_db_res,'|');
    uint32_t raddr = stoull(tmp[0], nullptr, 16);
    uint32_t rmask = stoull(tmp[2], nullptr, 16);
    uint32_t rsize = stoull(tmp[4], nullptr, 16);
    std::string rmode = tmp[3];
    std::string rperm = tmp[1];
    LOGGER->log_message(LogManager::DEBUG, stdsprintf("node %s properties: 0x%x  0x%x  0x%x  %s  %s",
                                                      regName.c_str(), raddr, rmask, rsize, rmode.c_str(), rperm.c_str()));

    if (rmask != 0xFFFFFFFF) {
      // deny block write on masked register
      std::stringstream errmsg;
      errmsg << "Block write attempted on masked register";
      la->response->set_string("error", errmsg.str());
      LOGGER->log_message(LogManager::ERROR, stdsprintf("block write error: %s", errmsg.str().c_str()));
    } else if (rmode.rfind("single") != std::string::npos && size > 1) {
      // only allow block write of size 1 on single registers
      std::stringstream errmsg;
      errmsg << "Block write attempted on single register with size greater than 1";
      la->response->set_string("error", errmsg.str());
      LOGGER->log_message(LogManager::ERROR, stdsprintf("block write error: %s", errmsg.str().c_str()));
    } else if ((offset+size) > rsize) {
      // don't allow the write to go beyond the block range
      std::stringstream errmsg;
      errmsg << "Block write attempted would go beyond the size of the RAM: "
             << "raddr: 0x"    << std::hex << raddr
             << ", offset: 0x" << std::hex << offset
             << ", size: 0x"   << std::hex << size
             << ", rsize: 0x"  << std::hex << rsize;
      la->response->set_string("error", errmsg.str());
      LOGGER->log_message(LogManager::ERROR, stdsprintf("block write error: %s", errmsg.str().c_str()));
    } else {
      if (memhub_write(memsvc, raddr+offset, size, values) != 0) {
        std::stringstream errmsg;
        errmsg << "Write memsvc error: " << memsvc_get_last_error(memsvc);
        la->response->set_string("error", errmsg.str());
        LOGGER->log_message(LogManager::ERROR, stdsprintf("writeBlock: %s", errmsg.str().c_str()));
      } else {
        std::stringstream msg;
        msg << "Block write succeeded.";
        la->response->set_string("debug", msg.str());
        LOGGER->log_message(LogManager::DEBUG, stdsprintf("writeBlock: %s", msg.str().c_str()));
      }
    }
  }
}

void writeBlock(const uint32_t& regAddr, const uint32_t* values, const size_t& size, const uint32_t& offset)
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
    if (memhub_open(&memsvc) != 0) {
      LOGGER->log_message(LogManager::ERROR, stdsprintf("Unable to connect to memory service: %s", memsvc_get_last_error(memsvc)));
      LOGGER->log_message(LogManager::ERROR, "Unable to load module");
      return; // Do not register our functions, we depend on memsvc.
    }
    modmgr->register_method("utils", "update_address_table", update_address_table);
    modmgr->register_method("utils", "readRegFromDB",        readRegFromDB);
  }
}
