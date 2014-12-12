/*
 * thread_safe_event.h
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

#ifndef THREAD_SAFE_EVENT_H
#define THREAD_SAFE_EVENT_H

#include <systemc.h>

class thread_safe_event_if:
  public sc_core::sc_interface
{
  virtual void notify(sc_core::sc_time delay = SC_ZERO_TIME) = 0;
  virtual const sc_core::sc_event& default_event(void) const = 0;
  protected:
  virtual void update(void) = 0;
};

class thread_safe_event:
  sc_core::sc_prim_channel,
  public thread_safe_event_if
{
  public:
  thread_safe_event(const char* name = "");
  void notify(sc_core::sc_time delay = SC_ZERO_TIME);
  const sc_core::sc_event& default_event(void) const;
  protected:
  virtual void update(void);
  private:
  sc_core::sc_event m_event;
  sc_core::sc_time m_delay;
};

#endif /*!THREAD_SAFE_EVENT_H*/
