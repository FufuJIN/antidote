/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/**
 * \file sample_agent.c
 * \brief Main application implementation.
 *
 * Copyright (C) 2011 Signove Tecnologia Corporation.
 * All rights reserved.
 * Contact: Signove Tecnologia Corporation (contact@signove.com)
 *
 * $LICENSE_TEXT:BEGIN$
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation and appearing
 * in the file LICENSE included in the packaging of this file; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 * $LICENSE_TEXT:END$
 *
 * \author Walter Guerra
 * \author Elvis Pfutzenreuter
 * \date May 31, 2011
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include <ieee11073.h>
#include "communication/plugin/plugin_fifo.h"
#include "communication/plugin/plugin_tcp_agent.h"
#include "specializations/pulse_oximeter.h"
#include "agent.h"

static const intu8 AGENT_SYSTEM_ID_VALUE[] = { 0x11, 0x33, 0x55, 0x77, 0x99,
		0xbb, 0xdd, 0xff};

/**
 * /brief The main application is a command-line-based tool that simply receives
 * raw data from IEEE devices and print them.
 *
 */

/**
 * Port used by agent to send network data
 */
static ContextId CONTEXT_ID = 0;

/**
 * PLugin definition
 */
static CommunicationPlugin comm_plugin = COMMUNICATION_PLUGIN_NULL;

static int alarms = 4;

/**
 * SIGALRM handler
 */
void sigalrm(int dummy)
{
	// This is not incredibly safe, because signal may interrupt
	// processing, and is not a technique for a production agent,
	// but suffices for this quick 'n dirty sample
	
	if (alarms > 1) {
		agent_send_data(CONTEXT_ID);
		alarm(3);
	} else if (alarms == 1) {
		agent_request_association_release(CONTEXT_ID);
		alarm(3);
	} else {
		agent_disconnect(CONTEXT_ID);
	}

	--alarms;
}

/**
 * Callback function that is called whenever a new device
 * has associated
 *
 * @param ctx current context.
 */
void device_associated(Context *ctx)
{
	fprintf(stderr, " main: Associated\n");
	alarm(3);
}

/**
 * Callback function that is called whenever a device
 * has disassociated
 *
 * @param ctx current context.
 */
void device_unavailable(Context *ctx)
{
	fprintf(stderr, " main: Disasociated\n");
	alarms = 0;
}

/**
 * Callback function that is called whenever a new device
 * has connected (but not associated)
 *
 * @param ctx current context.
 */
void device_connected(Context *ctx)
{
	fprintf(stderr, "main: Connected\n");

	// ok, make it proceed with association
	// (agent has the initiative)
	agent_associate(ctx->id);
}

/**
 * Prints utility command-line tool help.
 */
static void print_help()
{
	printf(
		"Utility tool to simulate IEEE 11073 agent\n\n"
		"Usage: ieee_agent [OPTION]\n"
		"Options:\n"
		"        --help                Print this help\n"
		"        --fifo                Run FIFO mode with default file descriptors\n"
		"        --tcp                 Run TCP mode on default port\n");
}

/**
 * Fake implementation of the reset timeout function.
 * @param ctx current context.
 */
static void timer_reset_timeout(Context *ctx)
{
}

/**
 * Waits 0 milliseconds for timeout.
 *
 * @param ctx current context.
 * @return fake timeout id
 */
static int timer_count_timeout(Context *ctx)
{
	return 1;
}

/**
 * Configure application to use fifo plugin
 */
static void fifo_mode()
{
	plugin_network_fifo_setup(&comm_plugin, CONTEXT_ID, 0);
}

/**
 * Configure application to use tcp plugin
 */
static void tcp_mode()
{
	int port = 6024;
	CONTEXT_ID = port;
	plugin_network_tcp_agent_setup(&comm_plugin, port);
}

/**
 * Generate data for oximeter event report
 */
static void *event_report_cb()
{
	time_t now;
	struct tm nowtm;
	struct oximeter_event_report_data* data =
		malloc(sizeof(struct oximeter_event_report_data));

	time(&now);
	localtime_r(&now, &nowtm);

	data->beats = 60 + random() % 20;
	data->oximetry = 90 + random() % 10;
	data->century = nowtm.tm_year / 100;
	data->year = nowtm.tm_year % 100;
	data->month = nowtm.tm_mon + 1;
	data->day = nowtm.tm_mday;
	data->hour = nowtm.tm_hour;
	data->minute = nowtm.tm_min;
	data->second = nowtm.tm_sec;
	data->sec_fractions = 50;

	return data;
}

/**
 * Generate data for MDS
 */
static struct mds_system_data *mds_data_cb()
{
	struct mds_system_data* data = malloc(sizeof(struct mds_system_data));
	memcpy(&data->system_id, AGENT_SYSTEM_ID_VALUE, 8);
	return data;
}

/**
 * Main function
 */
int main(int argc, char **argv)
{
	comm_plugin = communication_plugin();

	if (argc == 2) {

		if (strcmp(argv[1], "--help") == 0) {
			print_help();
			exit(0);
		} else if (strcmp(argv[1], "--tcp") == 0) {
			tcp_mode();
		} else if (strcmp(argv[1], "--fifo") == 0) {
			fifo_mode();
		} else {
			fprintf(stderr, "ERROR: invalid option: %s\n", argv[1]);
			fprintf(stderr, "Try `%s --help'"
				" for more information.\n", argv[0]);
			exit(1);
		}

	} else if (argc > 2) {
		fprintf(stderr, "ERROR: Invalid number of options\n");
		fprintf(stderr, "Try `%s --help'"
			" for more information.\n", argv[0]);
		exit(1);
	} else {
		// FIFO is default mode
		fifo_mode();
	}

	fprintf(stderr, "\nIEEE 11073 sample agent\n");

	comm_plugin.timer_count_timeout = timer_count_timeout;
	comm_plugin.timer_reset_timeout = timer_reset_timeout;
	agent_init(&comm_plugin, 0x0190 /* oximeter */,
			event_report_cb, mds_data_cb);

	AgentListener listener = AGENT_LISTENER_EMPTY;
	listener.device_connected = &device_connected;
	listener.device_associated = &device_associated;
	listener.device_unavailable = &device_unavailable;

	agent_add_listener(listener);

	agent_start();

	signal(SIGALRM, sigalrm);
	agent_connection_loop(CONTEXT_ID);

	agent_finalize();

	return 0;
}
