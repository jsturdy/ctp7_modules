/*! \file utils.h
 *  \brief util methods for RPC modules running on a Zynq
 *  \author Mykhailo Dalchenko <mykhailo.dalchenko@cern.ch>
 *  \author Brian Dorney <brian.l.dorney@cern.ch>
 */

#ifndef UTILS_H
#define UTILS_H

#include "moduleapi.h"
//#include <libmemsvc.h>
#include "memhub.h"
#include "lmdb_cpp_wrapper.h"
#include "xhal/utils/XHALXMLParser.h"

#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <vector>
#include <iterator>
#include <cstdio>

extern memsvc_handle_t memsvc; /// \var global memory service handle required for registers read/write operations

/*! \struct localArgs
 *  Contains arguments required to execute the method locally
 */
typedef struct localArgs {
    lmdb::txn & rtxn; /*!< LMDB transaction handle */
    lmdb::dbi & dbi;  /*!< LMDB individual database handle */
    RPCMsg *response; /*!< RPC response message */
} LocalArgs;

/*!
 * \brief returns a set up LocalArgs structure
 */
LocalArgs getLocalArgs(RPCMsg *response);

static constexpr uint32_t LMDB_SIZE = 1UL * 1024UL * 1024UL * 50UL; ///< Maximum size of the LMDB object, currently 50 MiB

// FIXME: to be replaced with the above function when the struct is properly implemented
#define GETLOCALARGS(response)                                  \
    auto env = lmdb::env::create();                             \
    env.set_mapsize(LMDB_SIZE);                                 \
    std::string gem_path       = std::getenv("GEM_PATH");       \
    std::string lmdb_data_file = gem_path+"/address_table.mdb"; \
    env.open(lmdb_data_file.c_str(), 0, 0664);                  \
    auto rtxn = lmdb::txn::begin(env, nullptr, MDB_RDONLY);     \
    auto dbi  = lmdb::dbi::open(rtxn, nullptr);                 \
    LocalArgs la = {.rtxn     = rtxn,                           \
                    .dbi      = dbi,                            \
                    .response = response};

struct localArgs getLocalArgs(RPCMsg *response);

template<typename Out>
void split(const std::string &s, char delim, Out result) {
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        *(result++) = item;
    }
}

std::vector<std::string> split(const std::string &s, char delim);

std::string serialize(xhal::utils::Node n);

/*!
 *  \brief This macro is used to terminate a function if an error occurs.
 *
 *   It logs the message, write it to the `error` RPC key (FIXME REMOVE? and returns the `error_code` value.)
 *   This method should only be called from an RPC callback
 *
 *  \param response A pointer to the RPC response object.
 *  \param message The `std::string` error message.
 *  \param error_code Value which is passed to the `return` statement. (FIXME REMOVE?)
 */
#define EMIT_RPC_ERROR(response, message, error_code) { \
    LOGGER->log_message(LogManager::ERROR, message);    \
    response->set_string("error", message);             \
    return error_code; }
    /* rtxn.abort(); } */

/*! \fn uint32_t getNumNonzeroBits(uint32_t value)
 *  \brief returns the number of nonzero bits in an integer
 *  \param value integer to check the number of nonzero bits
 */
uint32_t getNumNonzeroBits(uint32_t value);

/*! \fn uint32_t getMask(LocalArgs * la, const std::string & regName)
 *  \brief Returns the mask for a given register
 *  \param la Local arguments structure
 *  \param regName Register name
 */
uint32_t getMask(LocalArgs * la, const std::string & regName);

/*! \fn void writeRawAddress(uint32_t address, uint32_t value, RPCMsg *response)
 *  \brief Writes a value to a raw register address. Register mask is not applied
 *  \param address Register address
 *  \param value Value to write
 *  \param response RPC response message
 */
void writeRawAddress(uint32_t address, uint32_t value, RPCMsg *response);

/*! \fn uint32_t readRawAddress(uint32_t address, RPCMsg *response)
 *  \brief Reads a value from raw register address. Register mask is not applied
 *  \param address Register address
 *  \param response RPC response message
 */
uint32_t readRawAddress(uint32_t address, RPCMsg* response);

/*! \fn uint32_t getAddress(LocalArgs * la, const std::string & regName)
 *  \brief Returns an address of a given register
 *  \param la Local arguments structure
 *  \param regName Register name
 */
uint32_t getAddress(LocalArgs * la, const std::string & regName);

/*! \fn void writeAddress(lmdb::val & db_res, uint32_t value, RPCMsg *response)
 *  \brief Writes given value to the address. Register mask is not applied
 *  \param db_res LMDB call result
 *  \param value Value to write
 *  \param response RPC response message
 */
void writeAddress(lmdb::val & db_res, uint32_t value, RPCMsg *response);

/*! \fn uint32_t readAddress(lmdb::val & db_res, RPCMsg *response)
 *  \brief Reads given value to the address. Register mask is not applied
 *  \param db_res LMDB call result
 *  \param response RPC response message
 */
uint32_t readAddress(lmdb::val & db_res, RPCMsg *response);

/*! \fn void writeRawReg(LocalArgs * la, const std::string & regName, uint32_t value)
 *  \brief Writes a value to a raw register. Register mask is not applied
 *  \param la Local arguments structure
 *  \param regName Register name
 *  \param value Value to write
 */
void writeRawReg(LocalArgs * la, const std::string & regName, uint32_t value);

/*! \fn uint32_t uint32_t readRawReg(LocalArgs * la, const std::string & regName)
 *  \brief Reads a value from raw register. Register mask is not applied
 *  \param la Local arguments structure
 *  \param regName Register name
 */
uint32_t readRawReg(LocalArgs * la, const std::string & regName);

/*! \fn uint32_t applyMask(uint32_t data, uint32_t mask)
 *  \brief Returns the data with register mask applied
 *  \param data Register data
 *  \param mask Register mask
 */
uint32_t applyMask(uint32_t data, uint32_t mask);

/*! \fn uint32_t readReg(LocalArgs * la, const std::string & regName)
 *  \brief Reads a value from register. Register mask is applied. Will return 0xdeaddead if register is no accessible
 *  \param la Local arguments structure
 *  \param regName Register name
 */
uint32_t readReg(LocalArgs * la, const std::string & regName);

/*!
 *  \brief Reads a block of values from a contiguous address space.
 *  \param la Local arguments structure
 *  \param regName Register name of the block to be read
 *  \param size number of words to read (should this just come from the register properties?
 *  \param result Pointer to an array to hold the result
 *  \param offset Start reading from an offset from the base address returned by regName
 *  \returns the number of uint32_t words in the result (or better to return a std::vector?
 */
uint32_t readBlock(localArgs* la, const std::string& regName, uint32_t* result, const uint32_t& size, const uint32_t& offset=0);

/*!
 *  \brief Reads a block of values from a contiguous address space.
 *
 *  \param regAddr Register address of the block to be read
 *  \param size number of words to read
 *  \param result Pointer to an array to hold the result
 *  \param offset Start reading from an offset from the base address regAddr
 *  \returns the number of uint32_t words in the result (or better to return a std::vector?
 */
uint32_t readBlock(const uint32_t& regAddr,  uint32_t* result, const uint32_t& size, const uint32_t& offset=0);

/*! \fn void writeReg(localArgs * la, const std::string & regName, uint32_t value)
 *  \brief Writes a value to a register. Register mask is applied
 *  \param la Local arguments structure
 *  \param regName Register name
 *  \param value Value to write
 */
void writeReg(LocalArgs * la, const std::string & regName, uint32_t value);

/*!
 *  \brief Writes a block of values to a contiguous address space.
 *  \detail Block writes are allowed on 'single' registers, provided:
 *           * The size of the data to be written is 1
 *           * The register does not have a mask
 *          Block writes are allowed on 'block' and 'fifo/incremental/port' type addresses provided:
 *          * The size does not overrun the block as defined in the address table
 *          * Including cases where an offset is provided, baseaddr+offset+size > blocksize
 *  \param la Local arguments structure
 *  \param regName Register name of the block to be written
 *  \param values Values to write to the block
 *  \param offset Start writing at an offset from the base address returned by regName
 */
void writeBlock(localArgs* la, const std::string& regName, const uint32_t* values, const uint32_t& size, const uint32_t& offset=0);

/*!
 *  \brief Writes a block of values to a contiguous address space.
 *  \detail Block writes are allowed on 'single' registers, provided:
 *           * The size of the data to be written is 1
 *           * The register does not have a mask
 *          Block writes are allowed on 'block' and 'fifo/incremental/port' type addresses provided:
 *           * The size does not overrun the block as defined in the address table
 *           * Including cases where an offset is provided, baseaddr+offset+size > blocksize
 *  \param regAddr Register address of the block to be written
 *  \param values Values to write to the block
 *  \param size number of words to write
 *  \param offset Start writing at an offset from the base address regAddr
 */
void writeBlock(const uint32_t& regAddr, const uint32_t* values, const uint32_t& size, const uint32_t& offset=0);

#endif
