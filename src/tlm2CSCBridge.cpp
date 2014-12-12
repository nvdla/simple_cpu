/*
 * tlm2CBridge.cpp
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

#include "SimpleCPU/tlm2CSCBridge.h"

#include <dlfcn.h>
#include <sstream>
#include <iomanip>

static void request_notify(void *handler, uint64_t time_ns);
static uint64_t get_time_ns(void *handler);
static void request_stop(void *handler);
static void get_param_list(void *handler, char **list[], size_t *size);
static uint64_t get_uint_param(void *handler, const char *name);
static int64_t get_int_param(void *handler, const char *name);
static void get_string_param(void *handler, const char *name, char **param);
static void signal_end_of_quantum(void *handler);

std::vector<std::string> TLM2CSCBridge::libraryNames;
std::vector<int> TLM2CSCBridge::libraryOccurence;

TLM2CSCBridge::TLM2CSCBridge(sc_core::sc_module_name name):
  sc_core::sc_module(name),
  libraryName("library", "no")
{
  this->environment.get_time_ns = get_time_ns;
  this->environment.request_stop = request_stop;
  this->environment.request_notify = request_notify;
  this->environment.get_uint_param = get_uint_param;
  this->environment.get_int_param = get_int_param;
  this->environment.get_param_list = get_param_list;
  this->environment.get_string_param = get_string_param;
  this->environment.end_of_quantum = signal_end_of_quantum;
  this->environment.handler = this;

  SC_METHOD(notification);
  sensitive << tlm2c_method;
}

TLM2CSCBridge::~TLM2CSCBridge()
{
  this->cleanLibrary();
}

void TLM2CSCBridge::before_end_of_elaboration()
{
  this->loadLibrary();
  this->init();
}

void TLM2CSCBridge::end_of_elaboration()
{

}

void TLM2CSCBridge::notification()
{
  /*
   * This is called for every notified method in tlm2c.
   */
  model_notify(this->tlm2c_model);
}

void TLM2CSCBridge::addNotification(uint64_t time_ns)
{
  tlm2c_method.notify(sc_core::sc_time((double)(time_ns), sc_core::SC_NS));
}

void request_notify(void *handler, uint64_t time_ns)
{
  TLM2CSCBridge *_this = (TLM2CSCBridge *)handler;
  _this->addNotification(time_ns);
}

uint64_t get_time_ns(void *handler)
{
  return sc_core::sc_time_stamp().value() / 1000;
}

void get_param_list(void *handler, char **list[], size_t *size)
{
  gs::cnf::cnf_api *Api = gs::cnf::GCnf_Api::getApiInstance(NULL);
  assert(Api);

  std::vector<std::string> params = Api->getParamList();

  *list = (char **)malloc(params.size() * sizeof(char *));
  *size = params.size();

  for (size_t i = 0; i < params.size(); i++)
  {
    (*list)[i] = strdup(params[i].c_str());
  }
}

uint64_t get_uint_param(void *handler, const char *name)
{
  uint64_t value;
  gs::cnf::cnf_api *Api = gs::cnf::GCnf_Api::getApiInstance(NULL);
  assert(Api);

  Api->getPar(name)->getValue(value);

  return value;
}

int64_t get_int_param(void *handler, const char *name)
{
  int64_t value;
  gs::cnf::cnf_api *Api = gs::cnf::GCnf_Api::getApiInstance(NULL);
  assert(Api);

  Api->getPar(name)->getValue(value);

  return value;
}

void get_string_param(void *handler, const char *name, char **param)
{
  std::string value;
  gs::cnf::cnf_api *Api = gs::cnf::GCnf_Api::getApiInstance(NULL);
  assert(Api);

  *param = NULL;
  if (Api->getPar(name))
  {
    Api->getPar(name)->getValue(value);
    *param = strdup(value.c_str());
  }
}

void signal_end_of_quantum(void *handler)
{
  TLM2CSCBridge *_this = (TLM2CSCBridge *)handler;
  _this->end_of_quantum();
}

void request_stop(void *handler)
{
  TLM2CSCBridge *_this = (TLM2CSCBridge *)handler;
  _this->stop_request();
}

void TLM2CSCBridge::loadLibrary()
{
  char *error = NULL;
  std::string lib = libraryName;
  std::stringstream stream;
  std::string copyCommand;

  if (lib == "no")
  {
    SC_REPORT_ERROR(name(), "You need to specify a library name:\n"
                            "Set 'library' param with the library to load.");
  }

  /*
   * There are no way of loading a library twice without sharing the global
   * variables. So this make a copy of the library and load the copy.
   */
  for (size_t i = 0; i < libraryNames.size(); i++)
  {
    if (libraryNames[i] == lib)
    {
      /*
       * We already have this library. Just append the library occurence and
       * copy the file.
       */
      (libraryOccurence[i])++;
      stream.clear();
      stream.str("");
      stream << libraryOccurence[i];
      libraryAssociated = lib + stream.str();
      copyCommand = "cp " + lib + " " + libraryAssociated;
      system(copyCommand.c_str());
      this->removeLibrary = true;
      break;
    }
  }

  if (!this->removeLibrary)
  {
    /*
     * This is the first load.. We don't have to copy the library.
     * Just push the name and the occurence for the next library user.
     */
    libraryNames.push_back(lib);
    libraryOccurence.push_back(0);
    this->libraryAssociated = lib;
  }

  /*
   * More than one instance of a library can be inited.
   * This requires the library to be loaded more than one time.
   */
  dlerror();
  libraryHandle = dlopen(this->libraryAssociated.c_str(), RTLD_LAZY);
  error = dlerror();

  if (libraryHandle == NULL)
  {
    /*
     * Library not available just quit..
     */
    std::cout << "Can't load " << this->libraryAssociated << " :-(."
              << std::endl;

    if (error != NULL)
    {
      std::cout << error << std::endl;
    }

    abort();
  }

  this->tlm2c_elaboration =
    (Model *(*)(Environment *))dlsym(libraryHandle, "tlm2c_elaboration");
  this->tlm2c_socket_get_by_name =
    (Socket* (*)(const char*))dlsym(libraryHandle, "tlm2c_socket_get_by_name");

}

void TLM2CSCBridge::init()
{
  std::cout << "bridge: tlm2c_elaborate.." << std::endl;
  this->tlm2c_model = this->tlm2c_elaboration(&this->environment);
  this->additional_init();
}

void TLM2CSCBridge::cleanLibrary()
{
  if (this->libraryHandle != NULL)
  {
    dlclose(this->libraryHandle);
  }

  if (this->removeLibrary)
  {
    /*
     * Remove the shared library created int the libraryInit function.
     */
    system(std::string("rm " + this->libraryAssociated + " -f").c_str());
  }
}
