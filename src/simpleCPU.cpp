/*
 * simpleCPU.cpp
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

#include "SimpleCPU/simpleCPU.h"
#include "SimpleCPU/IRQ.h"

static void _memory_bt(void *handle, Payload *p)
{
  SimpleCPU *_this = (SimpleCPU *)handle;
  _this->memory_bt(p);
}

static int _memory_get_direct_mem_ptr(void *handle, Payload *p, DMIData *d)
{
  SimpleCPU *_this = (SimpleCPU *)handle;
  return _this->memory_get_direct_mem_ptr(p, d);
}

SimpleCPU::SimpleCPU(sc_core::sc_module_name name):
  TLM2CSCBridge(name),
  master_socket("iport"),
  irq_socket("interrupt_socket"),
  kernel("kernel", ""),
  dtb("dtb", ""),
  rootfs("rootfs", ""),
  kernel_cmd("kernel_cmd", "")
{
  master_socket.out_port(*this);
  /*
   * IRQ socket configuration.
   */
  gs::socket::config<gs_generic_signal::gs_generic_signal_protocol_types> cnf;
  cnf.use_mandatory_extension<IRQ_LINE_EXTENSION>();
  irq_socket.set_config(cnf);
  irq_socket.register_b_transport(this, &SimpleCPU::irq_b_transport);
}

SimpleCPU::~SimpleCPU()
{

}

void SimpleCPU::additional_init()
{
  /*
   * Specific to M3: Create the sockets and bind everything together.
   */

  /*
   * Target for the memory bus.
   */
  this->targetSocket = socket_target_create("wrapper_memory");

  /*
   * Initiator for the IRQ.
   */
  this->initiatorSocket = socket_initiator_create("wrapper_irq");

  socket_target_register_b_transport(this->targetSocket, this, _memory_bt);
  tlm2c_socket_target_register_dmi(this->targetSocket,
                                   _memory_get_direct_mem_ptr);

  /*
   * tlm2c bindings.
   */
  TargetSocket *remote_target =
    (TargetSocket *)this->tlm2c_socket_get_by_name("qbox.irq_slave");
  InitiatorSocket *remote_initiator =
    (InitiatorSocket *)this->tlm2c_socket_get_by_name("qbox.memory_master");

  if (!remote_initiator)
  {
    std::cout << "error can't find 'qbox.memory_master' socket" << std::endl;
    abort();
  }
  if (!remote_target)
  {
    std::cout << "error can't find 'qbox.irq_slave' socket" << std::endl;
    abort();
  }

  tlm2c_bind(this->initiatorSocket, remote_target);
  tlm2c_bind(remote_initiator, this->targetSocket);
}

std::string SimpleCPU::getLibraryName()
{
  return std::string("libqbox-cortex-m3.so");
}

void SimpleCPU::memory_bt(Payload *payload)
{
  /*
   * Get info from the source payload.
   */
  GenericPayload *p = (GenericPayload *)payload;
  uint64_t address = payload_get_address(p);
  uint64_t value = payload_get_value(p);
  uint64_t size = payload_get_size(p);
  Command cmd = payload_get_command(p);

  static transactionHandle tHandle = master_socket.create_transaction();
  gs::GSDataType::dtype data = gs::GSDataType::dtype((unsigned char *)&value,
                                                     size);
  tHandle->reset();

  /*
   * Make the transaction.
   */
  tHandle->setMBurstLength(size);
  tHandle->setMAddr(address);
  tHandle->setMData(data);

  if (cmd == READ)
  {
    tHandle->setMCmd(gs::Generic_MCMD_RD);
    master_socket.Transact(tHandle);
    value = *((uint32_t *)data.getData());
    payload_set_value(p, value);
  }
  else if (cmd == WRITE)
  {
    tHandle->setMCmd(gs::Generic_MCMD_WR);
    master_socket.Transact(tHandle);
  }
  else
  {
    sc_core::sc_stop();
  }

  if (tHandle->getSResp() == gs::Generic_SRESP_ERR)
  {
    payload_set_response_status(p, ADDRESS_ERROR_RESPONSE);
  }
  else
  {
    payload_set_response_status(p, OK_RESPONSE);
  }
}

int SimpleCPU::memory_get_direct_mem_ptr(Payload *p, DMIData *d)
{
  uint64_t address = payload_get_address((GenericPayload *)p);

  tlm::tlm_generic_payload payload;
  tlm::tlm_dmi dmi_data;

  payload.set_address(address);
  if (this->master_socket->get_direct_mem_ptr(payload, dmi_data))
  {
    d->pointer = dmi_data.get_dmi_ptr();
    return 1;
  }
  else
  {
    return 0;
  }
}

void SimpleCPU::irq_b_transport(unsigned int port, irqPayload& payload,
			                         sc_core::sc_time& time)
{
  IRQ_ext_data *data = (IRQ_ext_data *)(payload.get_data_ptr());

  static GenericPayload *p = payload_create();
  payload_set_address(p, data->irq_line);
  payload_set_value(p, data->value);
  payload_set_command(p, WRITE);
  b_transport(this->initiatorSocket, (Payload *)p);
}
