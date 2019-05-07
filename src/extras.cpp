/*! \file extras.cpp
 *  \brief Extra register operations methods for RPC modules
 *  \author Mykhailo Dalchenko <mykhailo.dalchenko@cern.ch>
 */

#include "moduleapi.h"
//#include <libmemsvc.h>
#include "memhub.h"

memsvc_handle_t memsvc; /// \var global memory service handle required for registers read/write operations

/*! \fn void mblockread(const RPCMsg *request, RPCMsg *response)
 *  \brief Sequentially reads a block of values from a contiguous address space.
 *  Register mask is not applied
 *  \param request RPC request message
 *  \param response RPC response message
 */
void mblockread(const RPCMsg *request, RPCMsg *response) {
  uint32_t count = request->get_word("count");
  uint32_t addr  = request->get_word("address");
  uint32_t data[count];

  if (memhub_read(memsvc, addr, count, data) != 0) {
    response->set_string("error", memsvc_get_last_error(memsvc));
    LOGGER->log_message(LogManager::INFO, stdsprintf("read memsvc error: %s",
                                                     memsvc_get_last_error(memsvc)));
    return;
  }
  response->set_word_array("data", data, count);
}

/*! \fn void mfiforead(const RPCMsg *request, RPCMsg *response)
 *  \brief Sequentially reads a block of values from the same raw register address.
 *  Register mask is not applied
 *  The address acts like a port/FIFO
 *  \param request RPC request message
 *  \param response RPC response message
 */
void mfiforead(const RPCMsg *request, RPCMsg *response) {
  uint32_t count = request->get_word("count");
  uint32_t addr  = request->get_word("address");
  uint32_t data[count];

  for (unsigned int i=0; i<count; i++){
    if (memhub_read(memsvc, addr, 1, &data[i]) != 0) {
      response->set_string("error", memsvc_get_last_error(memsvc));
      LOGGER->log_message(LogManager::INFO, stdsprintf("read memsvc error: %s",
                                                       memsvc_get_last_error(memsvc)));
      return;
    }
  }
  response->set_word_array("data", data, count);
}

/*! \fn void mlistread(const RPCMsg *request, RPCMsg *response)
 *  \brief Reads a list of raw addresses
 *  \param request RPC request message
 *  \param response RPC response message
 */
void mlistread(const RPCMsg *request, RPCMsg *response) {
  uint32_t count = request->get_word("count");
  uint32_t addr[count];
  request->get_word_array("addresses", addr);
  uint32_t data[count];

  for (unsigned int i=0; i<count; i++){
    if (memhub_read(memsvc, addr[i], 1, &data[i]) != 0) {
      response->set_string("error", memsvc_get_last_error(memsvc));
      LOGGER->log_message(LogManager::INFO, stdsprintf("read memsvc error: %s",
                                                       memsvc_get_last_error(memsvc)));
      return;
    }
  }
  response->set_word_array("data", data, count);
}

/*! \fn void mblockwrite(const RPCMsg *request, RPCMsg *response)
 *  \brief writes a block of values to a contiguous memory block
 *  \param request RPC request message
 *  \param response RPC response message
 */
void mblockwrite(const RPCMsg *request, RPCMsg *response) {
  uint32_t count = request->get_word_array_size("data");
  uint32_t addr  = request->get_word("address");
  uint32_t data[count];
  request->get_word_array("data", data);

  if (memhub_write(memsvc, addr, count, data) != 0) {
    response->set_string("error", memsvc_get_last_error(memsvc));
    LOGGER->log_message(LogManager::ERROR, stdsprintf("blockwrite memsvc error: %s",
                                                      memsvc_get_last_error(memsvc)));
    // needs better error handling
    return;
  }
  // return type?
  response->set_word_array("data", data, count);
}


/*! \fn void mfifowrite(const RPCMsg *request, RPCMsg *response)
 *  \brief writes a set of values to an address that acts as a port or FIFO
 *  \param request RPC request message
 *  \param response RPC response message
 */
void mfifowrite(const RPCMsg *request, RPCMsg *response) {
  uint32_t count = request->get_word_array_size("data");
  uint32_t addr  = request->get_word("address");
  uint32_t data[count];
  request->get_word_array("data", data);

  for (unsigned int i=0; i<count; i++){
    if (memhub_write(memsvc, addr, 1, &data[i]) != 0) {
      response->set_string("error", memsvc_get_last_error(memsvc));
      LOGGER->log_message(LogManager::ERROR, stdsprintf("fifowrite memsvc error: %s",
                                                        memsvc_get_last_error(memsvc)));
      // needs better error handling
      return;
    }
  }
  // return type?
  response->set_word_array("data", data, count);
}

/*! \fn void mlistwrite(const RPCMsg *request, RPCMsg *response)
 *  \brief writes a set of values to a list of addresses
 *  \param request RPC request message
 *  \param response RPC response message
 */
void mlistwrite(const RPCMsg *request, RPCMsg *response) {
  // implicit expectation that data and addresses will be the same size
  uint32_t count = request->get_word_array_size("data");
  uint32_t addr[count];
  request->get_word_array("addresses", addr);
  uint32_t data[count];
  request->get_word_array("data", data);

  for (unsigned int i=0; i<count; i++){
    if (memhub_write(memsvc, addr[i], 1, &data[i]) != 0) {
      response->set_string("error", memsvc_get_last_error(memsvc));
      LOGGER->log_message(LogManager::ERROR, stdsprintf("listwrite memsvc error: %s",
                                                        memsvc_get_last_error(memsvc)));
      // needs better error handling
      return;
    }
  }
  // return type?
  response->set_word_array("data", data, count);
}


extern "C" {
  const char *module_version_key = "extras v1.0.1";
  int module_activity_color = 4;
  void module_init(ModuleManager *modmgr) {
    if (memhub_open(&memsvc) != 0) {
      LOGGER->log_message(LogManager::ERROR, stdsprintf("Unable to connect to memory service: %s",
                                                        memsvc_get_last_error(memsvc)));
      LOGGER->log_message(LogManager::ERROR, "Unable to load module");
      return; // Do not register our functions, we depend on memsvc.
    }
    modmgr->register_method("extras", "fiforead",  mfiforead);
    modmgr->register_method("extras", "blockread", mblockread);
    modmgr->register_method("extras", "listread",  mlistread);
    modmgr->register_method("extras", "fifowrite",  mfifowrite);
    modmgr->register_method("extras", "blockwrite", mblockwrite);
    modmgr->register_method("extras", "listwrite",  mlistwrite);
  }
}
