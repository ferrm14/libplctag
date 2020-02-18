/***************************************************************************
 *   Copyright (C) 2020 by Kyle Hayes                                      *
 *   Author Kyle Hayes  kyle.hayes@gmail.com                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <string.h>
#include <stdlib.h>

#ifdef WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <netdb.h>
    #include <sys/socket.h>
    #include <sys/types.h>
#endif

#include <tests/simulator/ab.h>
#include <tests/simulator/handle_register_session.h>
#include <tests/simulator/packet.h>
#include <tests/simulator/socket.h>
#include <tests/simulator/utils.h>


#define BUF_SIZE (4200)

static void SIGINT_handler(int not_used);
static void setup_sigint_handler(void);
static slice_s dispatch_eip_packet(context_s *context, eip_packet_s header);


volatile sig_atomic_t sigint_received = 0;


int main(int argc, char **argv)
{
    int listen_socket = 0;
    uint8_t buffer[BUF_SIZE];
    context_s context;

    info("Simulator starting up.");

    (void)argc;
    (void)argv;

    /* set up a signal handler for ^C */
    setup_sigint_handler();

    memset(buffer, 0, sizeof buffer);
    memset(&context, 0, sizeof context);

    /* seed the random implementation. */
    srand((unsigned int)util_time_ms());

    /* start up socket */
    listen_socket = socket_open("0.0.0.0", "44818");

    if(listen_socket < 0) {
        error("Unable to open socket.   Open returned error %s.", plc_tag_decode_error(listen_socket));
    }

    /* create a slice to be used as a root buffer. */
    context.buffer = slice_make(buffer, BUF_SIZE);

    while(!sigint_received) {
        eip_packet_s eip_packet;
        slice_s response;
        int rc;

        info("Waiting for a client to connect.");
        context.client_sock = accept(listen_socket, NULL, NULL);

        if (context.client_sock < 0) {
            error("ERROR: accept() call failed: %s\n", gai_strerror(context.client_sock));
        }

        eip_packet = read_eip_packet(context.client_sock, context.buffer);
        if(eip_packet.status != 0) {
            error("Something went wrong when reading the client socket!");
        }

        info("Got packet from client!");


        response = dispatch_eip_packet(&context, eip_packet);
        if(slice_err(response)) {
            error("Unable to dispatch packet!");
        }

        rc = socket_write(context.client_sock, response);
    }

    return PLCTAG_STATUS_OK;
}



void SIGINT_handler(int not_used)
{
    (void)not_used;

    sigint_received = 1;
}


void setup_sigint_handler(void)
{
    struct sigaction SIGINT_act;

    /* set up child signal handler first. */
    memset(&SIGINT_act, 0, sizeof(SIGINT_act));
    SIGINT_act.sa_handler = SIGINT_handler;
    sigaction(SIGINT, &SIGINT_act, NULL);
}



slice_s dispatch_eip_packet(context_s *context, eip_packet_s packet)
{
    slice_s result;

    switch(packet.command) {
        case EIP_REGISTER_SESSION:
            result = handle_register_session(context, packet);
            break;


        default:
            info("Unsupported EIP packet type %x!", packet.command);
            result = slice_make_err(PLCTAG_ERR_UNSUPPORTED);
            break;
    }

    return result;
}