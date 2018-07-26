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

#include <pthread.h>
#include <systemc.h>
#include "tlm2CSCBridge.h"
#include "SimpleCPU/thread_safe_event.h"

#include "greencontrol/config.h"
#include "gsgpsocket/transport/GSGPMasterBlockingSocket.h"
#include "gsgpsocket/transport/GSGPSlaveSocket.h"
#include "gsgpsocket/transport/GSGPconfig.h"
#include "greensignalsocket/green_signal.h"
#if AWS_FPGA_PRESENT
#include "fpga_pci.h"
#endif
#ifndef SC_INCLUDE_DYNAMIC_PROCESSES
#define SC_INCLUDE_DYNAMIC_PROCESSES
#endif

#if REGISTER_ACCESS_TRACE
#include <iomanip>
#include <iostream>
#include <fstream>
#include <time.h>
#endif

class SimpleCPU:
  public TLM2CSCBridge,
  public gs::payload_event_queue_output_if<gs::gp::master_atom>
{
  SC_HAS_PROCESS(SimpleCPU);
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
  void set_dmi_mutex(pthread_mutex_t *mtx, bool is_fpga);//wrapper for cmod and fpga
  void set_dmi_mutex(pthread_mutex_t *mtx);//cmod function
  void set_dmi_mutex_fpga(pthread_mutex_t *mtx);//fpga function
  void set_dmi_base_addr(uint64_t addr);
#if AWS_FPGA_PRESENT
  bool set_pci_bar_handle(pci_bar_handle_t pci_bar_handle_in);
#endif
  private:
  /*
   * Internal tlm2c socket.
   */
  TargetSocket *targetSocket;
  InitiatorSocket *initiatorSocket;

  void additional_init();

  void notify(gs::gp::master_atom& tc) {};
  void end_of_elaboration();

  /* Kernel filename to be loaded by the CPU. */
  gs::gs_param<std::string> kernel;
  gs::gs_param<std::string> dtb;
  gs::gs_param<std::string> rootfs;
  gs::gs_param<std::string> kernel_cmd;

  // Extra parameters
  gs::gs_param<uint64_t> GDBPort;
  gs::gs_param<std::string> extraArguments;

  /* Transaction posting mechanism. */
  transactionHandle transaction;      /*<! Transaction to be posted. */
  bool transaction_pending;           /*<! CPU is waiting SystemC for a txn. */
  void post_a_transaction();
  void init_io();
  void destroy_io();
  pthread_mutex_t io_done_mtx;
  pthread_cond_t io_done_cond;
  bool io_completed;
  void finish_io();
  void wait_for_io_completion();
  void do_io();
  thread_safe_event io_evt;

  /* Synchronisation mechanism. */
  int systemc_running;                /*<! false when SystemC sleep. */
  void init_systemc_sleep();
  void wake_up_systemc();
  void systemc_sleep();
  void destroy_systemc_sleep();
  pthread_mutex_t sc_sleep_mtx;
  pthread_cond_t sc_sleep_cond;
  void quantum_notify();
  void end_of_quantum();
  sc_event quantum_evt;
  gs::gs_param<uint64_t> quantum;
  volatile bool cpu_has_finished;
  bool systemc_has_finished;
  bool cpu_init;                      /*<! CPU mutexes initialised. */
  /* CPU sleep. */
  void init_cpu_sleep();
  void wake_up_cpu();
  void cpu_sleep();
  void destroy_cpu_sleep();
  int cpu_running;
  pthread_mutex_t cpu_sleep_mtx;
  pthread_cond_t cpu_sleep_cond;

  // DMI
  void memory_invalidate_direct_mem_ptr(unsigned int index,
                                        sc_dt::uint64 start,
                                        sc_dt::uint64 end);
#if AWS_FPGA_PRESENT
  bool data_write(uint64_t addr, uint8_t *p_data, int len);
  bool data_read(uint64_t addr, uint8_t *p_data, int len);
#endif
  bool DMIInvalidatePending;
  uint64_t DMIInvalidateStart;
  uint64_t DMIInvalidateEnd;

  /* dummy event. */
  sc_event dummy_evt;
  void dummy();

  /* stop mechanism. */
  void stop_request();
  void stop();
  thread_safe_event stop_evt;

  /* dmi mode */
  pthread_mutex_t *dmi_mtx;
  bool is_dmi;
  bool is_dmi_fpga;
  uint64_t dmi_base_addr;
  uint64_t *ptr;
#if AWS_FPGA_PRESENT
  pci_bar_handle_t pci_bar_handle;
#endif

#if REGISTER_ACCESS_TRACE
  //this ofstream is used for generate perf time analysis report.
  ofstream  fout;
#endif
};

