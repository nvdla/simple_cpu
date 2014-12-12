/*
 * tlm2CSCBridge.h
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

#ifndef TLM2C_SC_BRIDGE_H
#define TLM2C_SC_BRIDGE_H

#include <systemc>
#include "greencontrol/config.h"

extern "C"
{
#include <tlm2c/tlm2c.h>
}

class TLM2CSCBridge:
  public sc_core::sc_module
{
  public:
  SC_HAS_PROCESS(TLM2CSCBridge);
  TLM2CSCBridge(sc_core::sc_module_name name);
  ~TLM2CSCBridge();
  void addNotification(uint64_t time_ns);
  virtual void end_of_quantum() = 0;
  virtual void stop_request() = 0;

  protected:
  Socket *(*tlm2c_socket_get_by_name)(const char *name);

  private:
  /*
   * TLM2C interface.
   */
  Model *tlm2c_model;           /*<! Remote object to bind. */
  void before_end_of_elaboration();
  void end_of_elaboration();
  void notification();
  sc_core::sc_event tlm2c_method;
  Model *(*tlm2c_elaboration)(Environment *); /*<! Elaboration of tlm2c. */

  void init();
  Environment environment;
  virtual void additional_init() = 0;

  /*
   * Library loading system.
   */
  void loadLibrary();
  void cleanLibrary();
  static std::vector<std::string> libraryNames;
  static std::vector<int> libraryOccurence;
  std::string libraryAssociated; /*<! Filename for the associated library. */
  bool removeLibrary;            /*<! Remove the library in the destructor. */
  void *libraryHandle;           /*<! Handle for the opened library. */

  gs::gs_param<std::string> libraryName; /*<! Library name for this CPU */

};

#endif
