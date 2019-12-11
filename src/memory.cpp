#include "xhal/common/rpc/register.h"
#include "moduleapi.h"
#include <libmemsvc.h>
#include "memhub.h"

memsvc_handle_t memsvc;

namespace memory {
    struct mwrite : public xhal::common::rpc::Method
    {
        void operator()(const std::string &at_xml) const;
    };

    struct mread : public xhal::common::rpc::Method
    {
        void operator()(const std::string &at_xml) const;
    };

};

void mread(const RPCMsg *request, RPCMsg *response) {
    uint32_t count = request->get_word("count");
    uint32_t addr = request->get_word("address");
    uint32_t data[count];

    if (memhub_read(memsvc, addr, count, data) == 0) {
        response->set_word_array("data", data, count);
    } else {
        response->set_string("error", memsvc_get_last_error(memsvc));
        LOG4CPLUS_INFO(logger, stdsprintf("read memsvc error: %s", memsvc_get_last_error(memsvc)));
    }
}

void mwrite(const RPCMsg *request, RPCMsg *response) {
    uint32_t count = request->get_word_array_size("data");
    uint32_t data[count];
    request->get_word_array("data", data);
    uint32_t addr = request->get_word("address");

    if (memhub_write(memsvc, addr, count, data) != 0) {
        response->set_string("error", std::string("memsvc error: ")+memsvc_get_last_error(memsvc));
        LOG4CPLUS_INFO(logger, stdsprintf("write memsvc error: %s", memsvc_get_last_error(memsvc)));
    }
}

extern "C" {
    const char *module_version_key = "memory v1.0.1";
    int module_activity_color = 4;

    void module_init(ModuleManager *modmgr) {
        if (memhub_open(&memsvc) != 0) {
            LOG4CPLUS_ERROR(logger, stdsprintf("Unable to connect to memory service: %s", memsvc_get_last_error(memsvc)));
            LOG4CPLUS_ERROR(logger, "Unable to load module");
            return;
        }

        xhal::common::rpc::registerMethod<memory::mread>(modmgr);
        xhal::common::rpc::registerMethod<memory::mwrite>(modmgr);
    }
}
