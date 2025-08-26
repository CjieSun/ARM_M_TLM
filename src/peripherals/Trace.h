#ifndef TRACE_H
#define TRACE_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <fstream>
#include <iostream>

using namespace sc_core;
using namespace tlm;

class Trace : public sc_module
{
public:
    /**
    * @brief Bus socket
    */
    tlm_utils::simple_target_socket<Trace> socket;

    /**
    * @brief Constructor
    * @param name Module name
    */
    explicit Trace(sc_core::sc_module_name const &name);

    /**
    * @brief Destructor
    */
    ~Trace() override;

private:

    // TLM-2 blocking transport method
    virtual void b_transport(tlm::tlm_generic_payload &trans,
                                sc_core::sc_time &delay);

    void xtermLaunch(char *slaveName) const;

    void xtermKill();

    void xtermSetup();

    int ptSlave{};
    int ptMaster{};
    int xtermPid{};
};

#endif // TRACE_H