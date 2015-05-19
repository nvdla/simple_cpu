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
  kernel_cmd("kernel_cmd", ""),
  GDBPort("gdb_port", (uint64_t)0),
  quantum("quantum", 100000000)
{
  master_socket.out_port(*this);
  /*
   * IRQ socket configuration.
   */
  gs::socket::config<gs_generic_signal::gs_generic_signal_protocol_types> cnf;
  cnf.use_mandatory_extension<IRQ_LINE_EXTENSION>();
  irq_socket.set_config(cnf);
  irq_socket.register_b_transport(this, &SimpleCPU::irq_b_transport);

  SC_METHOD(quantum_notify);
  sensitive << quantum_evt;
  dont_initialize();
  quantum_evt.notify(quantum, sc_core::SC_NS);

  SC_METHOD(stop);
  sensitive << stop_evt;
  dont_initialize();

  this->cpu_has_finished = false;
  this->systemc_has_finished = false;
  this->cpu_init = false;

  init_io();
  init_systemc_sleep();
  init_cpu_sleep();
}

SimpleCPU::~SimpleCPU()
{
  destroy_io();
  destroy_systemc_sleep();
  destroy_cpu_sleep();
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

void SimpleCPU::end_of_elaboration()
{
  /* Create transaction. */
  transaction = master_socket.create_transaction();
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
  gs::GSDataType::dtype data = gs::GSDataType::dtype((unsigned char *)&value,
                                                     size);

  /* Fill the transaction. */
  this->transaction->reset();
  this->transaction->setMBurstLength(size);
  this->transaction->setMAddr(address);
  this->transaction->setMData(data);
  if (cmd == READ)
  {
    this->transaction->setMCmd(gs::Generic_MCMD_RD);
  }
  else if (cmd == WRITE)
  {
    this->transaction->setMCmd(gs::Generic_MCMD_WR);
  }

  /* Ask SystemC to do the transaction. */
  this->post_a_transaction();

  if (cmd == READ)
  {
    value = *((uint32_t *)data.getData());
    payload_set_value(p, value);
  }

  if (this->transaction->getSResp() == gs::Generic_SRESP_ERR)
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

void SimpleCPU::init_io()
{
  transaction_pending = false;
  pthread_mutex_init(&io_done_mtx, NULL);
  pthread_cond_init(&io_done_cond, NULL);

  SC_METHOD(do_io);
  sensitive << io_evt;
  dont_initialize();

  SC_METHOD(dummy);
  sensitive << dummy_evt;
  dont_initialize();
}

void SimpleCPU::destroy_io()
{
  pthread_mutex_destroy(&io_done_mtx);
  pthread_cond_destroy(&io_done_cond);
}

void SimpleCPU::do_io()
{
  /* Do all the IO for the CPU in the SystemC thread. */
  if (this->transaction_pending)
  {
    /*
     * At this time SystemC thread has the io_done_mtx mutex. Just call
     * b_transport with the pending transaction.
     */
    this->transaction_pending = false;
    master_socket.Transact(this->transaction);
    this->finish_io();
  }

  if (this->systemc_has_finished)
  {
    /* Notify quantum_notify() as SystemC has finished. */
    quantum_evt.notify();
  }
  else
  {
    /*
     * SystemC won't really sleep here. But it was woken up for nothing by
     * post_a_transaction.
     */
    systemc_sleep();
  }
}

void SimpleCPU::finish_io()
{
  pthread_mutex_lock(&io_done_mtx);
  this->io_completed = true;
  pthread_mutex_unlock(&this->io_done_mtx);
  pthread_cond_signal(&io_done_cond);
}

void SimpleCPU::wait_for_io_completion()
{
  pthread_mutex_lock(&io_done_mtx);
  while (!this->io_completed)
  {
    pthread_cond_wait(&io_done_cond, &io_done_mtx);
  }
  pthread_mutex_unlock(&io_done_mtx);
}

void SimpleCPU::post_a_transaction()
{
  /*
   * As SystemC is not thread safe at all, only SystemC can access SystemC code.
   * The transaction might come from an other thread so a post mechanism is
   * implemented to ensure that only SystemC call b_transport for memory access.
   */
  this->io_completed = false;
  this->transaction_pending = true;
  /* Notify the event ASAP. */
  io_evt.notify();
  this->wake_up_systemc();
  this->wait_for_io_completion();
}


void SimpleCPU::dummy()
{
  /* dummy does nothing.. */
}

void SimpleCPU::init_systemc_sleep()
{
  systemc_running = 1;
  pthread_mutex_init(&sc_sleep_mtx, NULL);
  pthread_cond_init(&sc_sleep_cond, NULL);
}

void SimpleCPU::wake_up_systemc()
{
  /* Wake up SystemC for IO or at the end of the CPU quantum. */
  pthread_mutex_lock(&sc_sleep_mtx);
  systemc_running++;
  pthread_mutex_unlock(&sc_sleep_mtx);
  pthread_cond_signal(&sc_sleep_cond);
}

void SimpleCPU::systemc_sleep()
{
  /* SystemC is sleeping here until somebody calls wake_up_systemc. */
  pthread_mutex_lock(&sc_sleep_mtx);
  systemc_running--;
  while (systemc_running <= 0)
  {
    pthread_cond_wait(&sc_sleep_cond, &sc_sleep_mtx);
  }
  pthread_mutex_unlock(&sc_sleep_mtx);
  /* Notify a dummy event just to not increase time for async events. */
  dummy_evt.notify();
}

void SimpleCPU::destroy_systemc_sleep()
{
  pthread_mutex_destroy(&sc_sleep_mtx);
  pthread_cond_destroy(&sc_sleep_cond);
}

void SimpleCPU::init_cpu_sleep()
{
  cpu_running = 1;
  pthread_mutex_init(&cpu_sleep_mtx, NULL);
  pthread_cond_init(&cpu_sleep_cond, NULL);
}

void SimpleCPU::wake_up_cpu()
{
  /* Wake up CPU when SystemC has finished it's quantum. */
  pthread_mutex_lock(&cpu_sleep_mtx);
  cpu_running++;
  pthread_mutex_unlock(&cpu_sleep_mtx);
  pthread_cond_signal(&cpu_sleep_cond);
}

void SimpleCPU::cpu_sleep()
{
  /* CPU is sleeping here until SystemC calls wake_up_cpu. */
  pthread_mutex_lock(&cpu_sleep_mtx);
  cpu_running--;
  while (cpu_running <= 0)
  {
    pthread_cond_wait(&cpu_sleep_cond, &cpu_sleep_mtx);
  }
  pthread_mutex_unlock(&cpu_sleep_mtx);
}

void SimpleCPU::destroy_cpu_sleep()
{
  pthread_mutex_destroy(&cpu_sleep_mtx);
  pthread_cond_destroy(&cpu_sleep_cond);
}

void SimpleCPU::quantum_notify()
{
  /* Wait for the CPU to be initialised. */
  while (!this->cpu_init);

  /*
   * SystemC is going to sleep. CPU thread wakes up SystemC if it posts an IO
   * or finishes it's quantum.
   */
  systemc_has_finished = true;
  systemc_sleep();

  if (!cpu_has_finished)
  {
    return;
  }

  cpu_has_finished = false;
  systemc_has_finished = false;

  /* Notify for the next quantum. */
  quantum_evt.notify(quantum, sc_core::SC_NS);
  /* Release CPU. */
  wake_up_cpu();
}

void SimpleCPU::end_of_quantum()
{
  /* First time called at zero for initialisation. */
  if (!cpu_init)
  {
    cpu_init = true;
    return;
  }

  /* The CPU has finished it's quantum. It just needs to wait for SystemC. */
  cpu_has_finished = true;
  wake_up_systemc();
  cpu_sleep();
}

void SimpleCPU::stop_request()
{
  stop_evt.notify();
  wake_up_systemc();
}

void SimpleCPU::stop()
{
  sc_core::sc_stop();
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
