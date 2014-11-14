/*
 * simpleCPU.h
 *
 * Copyright (C) 2014, GreenSocs Ltd.
 *
 * Developped by Konrad Frederic <fred.konrad@greensocs.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses>.
 *
 * Linking GreenSocs code, statically or dynamically with other modules
 * is making a combined work based on GreenSocs code. Thus, the terms and
 * conditions of the GNU General Public License cover the whole
 * combination.
 *
 * In addition, as a special exception, the copyright holders, GreenSocs
 * Ltd, give you permission to combine GreenSocs code with free software
 * programs or libraries that are released under the GNU LGPL, under the
 * OSCI license, under the OCP TLM Kit Research License Agreement or
 * under the OVP evaluation license.You may copy and distribute such a
 * system following the terms of the GNU GPL and the licenses of the
 * other code concerned.
 *
 * Note that people who make modified versions of GreenSocs code are not
 * obligated to grant this special exception for their modified versions;
 * it is their choice whether to do so. The GNU General Public License
 * gives permission to release a modified version without this exception;
 * this exception also makes it possible to release a modified version
 * which carries forward this exception.
 *
 */

#include <systemc.h>
#include "tlm2CSCBridge.h"

#include "greencontrol/config.h"
#include "gsgpsocket/transport/GSGPMasterBlockingSocket.h"
#include "gsgpsocket/transport/GSGPSlaveSocket.h"
#include "gsgpsocket/transport/GSGPconfig.h"
#include "greensignalsocket/green_signal.h"

class SimpleCPU:
  public TLM2CSCBridge,
  public gs::payload_event_queue_output_if<gs::gp::master_atom>
{
  public:
  SimpleCPU(sc_core::sc_module_name name);
  ~SimpleCPU();

  typedef gs::gp::GenericMasterBlockingPort<32>::accessHandle transactionHandle;
  gs::gp::GenericMasterBlockingPort<32> master_socket;
  typedef gs_generic_signal::gs_generic_signal_payload irqPayload;
  gs_generic_signal::target_signal_multi_socket<SimpleCPU> irq_socket;
  void irq_b_transport(unsigned int port, irqPayload& payload,
                       sc_core::sc_time& time);

  void memory_bt(Payload *p);
  int memory_get_direct_mem_ptr(Payload *p, DMIData *d);
  private:
  /*
   * Internal tlm2c socket.
   */
  TargetSocket *targetSocket;
  InitiatorSocket *initiatorSocket;

  std::string getLibraryName();
  void additional_init();

  void notify(gs::gp::master_atom& tc) {};

  /* Kernel filename to be loaded by the CPU. */
  gs::gs_param<std::string> kernel;
};

