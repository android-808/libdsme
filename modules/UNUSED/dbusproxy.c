/**
   @file dbusproxy.c

   This module implements proxying of between DSME's internal message
   queue and D-Bus.
   <p>
   Copyright (C) 2009 Nokia Corporation.

   @author Semi Malinen <semi.malinen@nokia.com>

   This file is part of Dsme.

   Dsme is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License
   version 2.1 as published by the Free Software Foundation.

   Dsme is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with Dsme.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * An example command line to obtain dsme version number over D-Bus:
 * $ dbus-send --system --print-reply --dest=com.nokia.dsme /com/nokia/dsme com.nokia.dsme.request.get_version
 *
 * TODO:
 * - dsme.conf for D-Bus configuration
 * - dsme must not die if D-Bus goes down
 */
#include "dbusproxy.h"
#include "dsme_dbus.h"
#include "dsme_dbus_if.h"

#include "state.h"

#include "dsme/modules.h"
#include "dsme/logging.h"

#include <glib.h>
#include <stdlib.h>


static const char* const service       = dsme_service;
static const char* const req_interface = dsme_req_interface;
static const char* const sig_interface = dsme_sig_interface;
static const char* const sig_path      = dsme_sig_path;


static char* dsme_version = 0;

static void get_version(const DsmeDbusMessage* request, DsmeDbusMessage** reply)
{
  *reply = dsme_dbus_reply_new(request);
  dsme_dbus_message_append_string(*reply,
                                  dsme_version ? dsme_version : "unknown");
}

static void req_powerup(const DsmeDbusMessage* request, DsmeDbusMessage** reply)
{
  char* sender = dsme_dbus_endpoint_name(request);
  dsme_log(LOG_CRIT,
           "powerup request received over D-Bus from %s",
           sender ? sender : "(unknown)");
  free(sender);

  DSM_MSGTYPE_POWERUP_REQ req = DSME_MSG_INIT(DSM_MSGTYPE_POWERUP_REQ);
  broadcast_internally(&req);
}

static void req_reboot(const DsmeDbusMessage* request, DsmeDbusMessage** reply)
{
  char* sender = dsme_dbus_endpoint_name(request);
  dsme_log(LOG_CRIT,
           "reboot request received over D-Bus from %s",
           sender ? sender : "(unknown)");
  free(sender);

  DSM_MSGTYPE_REBOOT_REQ req = DSME_MSG_INIT(DSM_MSGTYPE_REBOOT_REQ);
  broadcast_internally(&req);
}

static void req_shutdown(const DsmeDbusMessage* request,
                         DsmeDbusMessage**      reply)
{
  char* sender = dsme_dbus_endpoint_name(request);
  dsme_log(LOG_CRIT,
           "shutdown request received over D-Bus from %s",
           sender ? sender : "(unknown)");
  free(sender);

  DSM_MSGTYPE_SHUTDOWN_REQ req = DSME_MSG_INIT(DSM_MSGTYPE_SHUTDOWN_REQ);

  broadcast_internally(&req);
}

static const dsme_dbus_binding_t methods[] = {
  { get_version,  dsme_get_version  },
  { req_powerup,  dsme_req_powerup  },
  { req_reboot,   dsme_req_reboot   },
  { req_shutdown, dsme_req_shutdown },
  { 0, 0 }
};


static bool bound = false;


static void emit_dsme_dbus_signal(const char* name)
{
  DsmeDbusMessage* sig = dsme_dbus_signal_new(sig_path, sig_interface, name);
  dsme_dbus_signal_emit(sig);
}

DSME_HANDLER(DSM_MSGTYPE_STATE_CHANGE_IND, server, msg)
{
  if (msg->state == DSME_STATE_SHUTDOWN ||
      msg->state == DSME_STATE_ACTDEAD  ||
      msg->state == DSME_STATE_REBOOT)
  {
    emit_dsme_dbus_signal(dsme_shutdown_ind);
  }
}

DSME_HANDLER(DSM_MSGTYPE_THERMAL_SHUTDOWN_IND, server, msg)
{
  emit_dsme_dbus_signal(dsme_thermal_shutdown_ind);
}

DSME_HANDLER(DSM_MSGTYPE_SAVE_DATA_IND, server, msg)
{
  emit_dsme_dbus_signal(dsme_save_unsaved_data_ind);
}

DSME_HANDLER(DSM_MSGTYPE_STATE_REQ_DENIED_IND, server, msg)
{
    const char* request = (msg->state == DSME_STATE_SHUTDOWN ?
                           "shutdown" : "reboot");

    dsme_log(LOG_CRIT,
             "proxying %s request denial due to %s to D-Bus",
             request,
             (const char*)DSMEMSG_EXTRA(msg));

    DsmeDbusMessage* sig = dsme_dbus_signal_new(sig_path,
                                                sig_interface,
                                                dsme_state_req_denied_ind);
    dsme_dbus_message_append_string(sig, request );
    dsme_dbus_message_append_string(sig, DSMEMSG_EXTRA(msg));

    dsme_dbus_signal_emit(sig);
}

DSME_HANDLER(DSM_MSGTYPE_DBUS_CONNECT, client, msg)
{
  dsme_log(LOG_DEBUG, "DBUS_CONNECT");
  dsme_dbus_bind_methods(&bound, methods, service, req_interface);
}

DSME_HANDLER(DSM_MSGTYPE_DBUS_DISCONNECT, client, msg)
{
  dsme_log(LOG_DEBUG, "DBUS_DISCONNECT");
  dsme_dbus_unbind_methods(&bound, methods, service, req_interface);
}

DSME_HANDLER(DSM_MSGTYPE_DSME_VERSION, server, msg)
{
  if (!dsme_version) {
      dsme_version = g_strdup(DSMEMSG_EXTRA(msg));
  }
}


module_fn_info_t message_handlers[] = {
  DSME_HANDLER_BINDING(DSM_MSGTYPE_STATE_CHANGE_IND),
  DSME_HANDLER_BINDING(DSM_MSGTYPE_THERMAL_SHUTDOWN_IND),
  DSME_HANDLER_BINDING(DSM_MSGTYPE_SAVE_DATA_IND),
  DSME_HANDLER_BINDING(DSM_MSGTYPE_STATE_REQ_DENIED_IND),
  DSME_HANDLER_BINDING(DSM_MSGTYPE_DBUS_CONNECT),
  DSME_HANDLER_BINDING(DSM_MSGTYPE_DBUS_DISCONNECT),
  DSME_HANDLER_BINDING(DSM_MSGTYPE_DSME_VERSION),
  { 0 }
};


void module_init(module_t* handle)
{
  /* get dsme version so that we can report it over D-Bus if asked to */
  DSM_MSGTYPE_GET_VERSION req = DSME_MSG_INIT(DSM_MSGTYPE_GET_VERSION);
  broadcast_internally(&req);

  /* Do not connect to D-Bus; it is probably not started yet.
   * Instead, wait for DSM_MSGTYPE_DBUS_CONNECT.
   */

  dsme_log(LOG_DEBUG, "libdbusproxy.so loaded");
}

void module_fini(void)
{
  dsme_dbus_unbind_methods(&bound, methods, service, req_interface);

  g_free(dsme_version);
  dsme_version = 0;

  dsme_log(LOG_DEBUG, "libdbusproxy.so unloaded");
}
