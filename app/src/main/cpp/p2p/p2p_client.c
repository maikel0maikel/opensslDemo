//
// Created by DELL on 2022/1/21.
//

#include "p2p_client.h"

app_ur_session **elems = NULL;
struct event_base *client_event_base = NULL;
static char buffer_to_send[65536] = "\0";
#define SLEEP_INTERVAL (234)

#define MAX_LISTENING_CYCLE_NUMBER (7)
static uint64_t current_mstime = 0;

static int refresh_channel(app_ur_session *elem, uint16_t method, uint32_t lt);
void set_notify_address_cb(local_address_cb cb) {
    set_local_addr_cb(cb);
}
app_ur_session *_init_app_session(app_ur_session *ss) {
    if (ss) {
        bzero(ss, sizeof(app_ur_session));
        ss->pinfo.fd = -1;
    }
    return ss;
}

app_ur_session *create_new_ss(void) {
    return _init_app_session((app_ur_session *) malloc(sizeof(app_ur_session)));
}

int start_client(const char *remote_address, int port, const unsigned char *ifname,
                 const char *local_address) {

    app_ur_session *ss = create_new_ss();

    app_ur_conn_info *clnet_info = &(ss->pinfo);

    uint16_t chnum = 0;

    if (start_connection(port, remote_address,
                         ifname, local_address,
                         clnet_verbose,
                         clnet_info, &chnum) < 0) {
        LOGE("start_connection error");
        return -1;
    }

    socket_set_nonblocking(clnet_info->fd);
    struct event *ev = event_new(client_event_base, clnet_info->fd,
                                 EV_READ | EV_PERSIST, client_input_handler,
                                 ss);

    event_add(ev, NULL);
    ss->state = UR_STATE_READY;

    ss->input_ev = ev;
    //ss->tot_msgnum = messagenumber;
    ss->recvmsgnum = -1;
    ss->chnum = chnum;

    elems[0] = ss;

    refresh_channel(ss, 0, 600);
    return 0;
}


static int wait_fd(int fd, unsigned int cycle) {

    if (fd >= (int) FD_SETSIZE) {
        return 1;
    } else {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        if (dos && cycle == 0)
            return 0;

        struct timeval start_time;
        struct timeval ctime;
        gettimeofday(&start_time, NULL);

        ctime.tv_sec = start_time.tv_sec;
        ctime.tv_usec = start_time.tv_usec;

        int rc = 0;

        do {
            struct timeval timeout = {0, 0};
            if (cycle == 0) {
                timeout.tv_usec = 500000;
            } else {

                timeout.tv_sec = 1;
                while (--cycle) timeout.tv_sec = timeout.tv_sec + timeout.tv_sec;

                if (ctime.tv_sec > start_time.tv_sec) {
                    if (ctime.tv_sec >= start_time.tv_sec + timeout.tv_sec) {
                        break;
                    } else {
                        timeout.tv_sec -= (ctime.tv_sec - start_time.tv_sec);
                    }
                }
            }
            rc = select(fd + 1, &fds, NULL, NULL, &timeout);
            if ((rc < 0) && (errno == EINTR)) {
                gettimeofday(&ctime, NULL);
            } else {
                break;
            }
        } while (1);

        return rc;
    }
}
int recv_buffer(app_ur_conn_info *clnet_info, stun_buffer *message, int sync, int data_connection,
                app_tcp_conn_info *atc, stun_buffer *request_message) {

    int rc = 0;

    stun_tid tid;
    uint16_t method = 0;

    if (request_message) {
        stun_tid_from_message(request_message, &tid);
        method = stun_get_method(request_message);
    }

    ioa_socket_raw fd = clnet_info->fd;
    if (atc)
        fd = atc->tcp_data_fd;

    SSL *ssl = clnet_info->ssl;
    if (atc)
        ssl = atc->tcp_data_ssl;

    recv_again:

    if (!use_tcp && sync && request_message && (fd >= 0)) {

        unsigned int cycle = 0;
        while (cycle < MAX_LISTENING_CYCLE_NUMBER) {
            int serc = wait_fd(fd, cycle);
            if (serc > 0)
                break;
            if (serc < 0) {
                return -1;
            }
            if (send_buffer(clnet_info, request_message, data_connection, atc) <= 0)
                return -1;
            ++cycle;
        }
    }

    if (!use_secure && !use_tcp && fd >= 0) {

        /* Plain UDP */

        do {
            rc = recv(fd, message->buf, sizeof(message->buf) - 1, 0);
            if (rc < 0 && errno == EAGAIN && sync)
                errno = EINTR;
        } while (rc < 0 && (errno == EINTR));

        if (rc < 0) {
            return -1;
        }

        message->len = rc;

    } else if (use_secure && !use_tcp && ssl && !(clnet_info->broken)) {

        /* DTLS */

        int message_received = 0;
        int cycle = 0;
        while (!message_received && cycle++ < 100) {

            if (SSL_get_shutdown(ssl))
                return -1;

            rc = 0;
            do {
                rc = SSL_read(ssl, message->buf, sizeof(message->buf) - 1);
                if (rc < 0 && errno == EAGAIN && sync)
                    continue;
            } while (rc < 0 && (errno == EINTR));

            if (rc > 0) {

                if (clnet_verbose) {
                    TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO,
                                  "response received: size=%d\n", rc);
                }
                message->len = rc;
                message_received = 1;

            } else {

                int sslerr = SSL_get_error(ssl, rc);

                switch (sslerr) {
                    case SSL_ERROR_NONE:
                        /* Try again ? */
                        break;
                    case SSL_ERROR_WANT_WRITE:
                        /* Just try again later */
                        break;
                    case SSL_ERROR_WANT_READ:
                        /* continue with reading */
                        break;
                    case SSL_ERROR_ZERO_RETURN:
                        /* Try again */
                        break;
                    case SSL_ERROR_SYSCALL:
                        TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO,
                                      "Socket read error 111.999: \n");
                        if (handle_socket_error())
                            break;
                        /* Falls through. */
                    case SSL_ERROR_SSL: {
                        TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, "SSL write error: \n");
                        char buf[1024];
                        TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, "%s (%d)\n",
                                      ERR_error_string(ERR_get_error(), buf),
                                      SSL_get_error(ssl, rc));
                    }
                        /* Falls through. */
                    default:
                        clnet_info->broken = 1;
                        TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO,
                                      "Unexpected error while reading: rc=%d, sslerr=%d\n",
                                      rc, sslerr);
                        return -1;
                }

                if (!sync)
                    break;
            }
        }

    } else if (use_secure && use_tcp && ssl && !(clnet_info->broken)) {

        /* TLS*/

        int message_received = 0;
        int cycle = 0;
        while (!message_received && cycle++ < 100) {

            if (SSL_get_shutdown(ssl))
                return -1;
            rc = 0;
            do {
                rc = SSL_read(ssl, message->buf, sizeof(message->buf) - 1);
                if (rc < 0 && errno == EAGAIN && sync)
                    continue;
            } while (rc < 0 && (errno == EINTR));

            if (rc > 0) {

                if (clnet_verbose) {
                    TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO,
                                  "response received: size=%d\n", rc);
                }
                message->len = rc;
                message_received = 1;

            } else {

                int sslerr = SSL_get_error(ssl, rc);

                switch (sslerr) {
                    case SSL_ERROR_NONE:
                        /* Try again ? */
                        break;
                    case SSL_ERROR_WANT_WRITE:
                        /* Just try again later */
                        break;
                    case SSL_ERROR_WANT_READ:
                        /* continue with reading */
                        break;
                    case SSL_ERROR_ZERO_RETURN:
                        /* Try again */
                        break;
                    case SSL_ERROR_SYSCALL:
                        TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO,
                                      "Socket read error 111.999: \n");
                        if (handle_socket_error())
                            break;
                        /* Falls through. */
                    case SSL_ERROR_SSL: {
                        TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, "SSL write error: \n");
                        char buf[1024];
                        TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, "%s (%d)\n",
                                      ERR_error_string(ERR_get_error(), buf),
                                      SSL_get_error(ssl, rc));
                    }
                        /* Falls through. */
                    default:
                        clnet_info->broken = 1;
                        TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO,
                                      "Unexpected error while reading: rc=%d, sslerr=%d\n",
                                      rc, sslerr);
                        return -1;
                }

                if (!sync)
                    break;
            }
        }

    } else if (!use_secure && use_tcp && fd >= 0) {

        /* Plain TCP */

        do {
            rc = recv(fd, message->buf, sizeof(message->buf) - 1, MSG_PEEK);
            if ((rc < 0) && (errno == EAGAIN) && sync) {
                errno = EINTR;
            }
        } while (rc < 0 && (errno == EINTR));

        if (rc > 0) {
            int mlen = rc;
            size_t app_msg_len = (size_t) rc;
            if (!atc) {
                mlen = stun_get_message_len_str(message->buf, rc, 1,
                                                &app_msg_len);
            } else {
                if (!sync)
                    mlen = clmessage_length;

                if (mlen > clmessage_length)
                    mlen = clmessage_length;

                app_msg_len = (size_t) mlen;
            }

            if (mlen > 0) {

                int rcr = 0;
                int rsf = 0;
                int cycle = 0;
                while (rsf < mlen && cycle++ < 128) {
                    do {
                        rcr = recv(fd, message->buf + rsf,
                                   (size_t) mlen - (size_t) rsf, 0);
                        if (rcr < 0 && errno == EAGAIN && sync)
                            errno = EINTR;
                    } while (rcr < 0 && (errno == EINTR));

                    if (rcr > 0)
                        rsf += rcr;

                }

                if (rsf < 1)
                    return -1;

                if (rsf < (int) app_msg_len) {
                    if ((size_t) (app_msg_len / (size_t) rsf) * ((size_t) (rsf))
                        != app_msg_len) {
                        return -1;
                    }
                }

                message->len = app_msg_len;

                rc = app_msg_len;

            } else {
                rc = 0;
            }
        }
    }

    if (rc > 0) {
        if (request_message) {

            stun_tid recv_tid;
            uint16_t recv_method = 0;

            stun_tid_from_message(message, &recv_tid);
            recv_method = stun_get_method(message);

            if (method != recv_method) {
                TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO,
                              "Received wrong response method: 0x%x, expected 0x%x; trying again...\n",
                              (unsigned int) recv_method, (unsigned int) method);
                goto recv_again;
            }

            if (memcmp(tid.tsx_id, recv_tid.tsx_id, STUN_TID_SIZE)) {
                TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO,
                              "Received wrong response tid; trying again...\n");
                goto recv_again;
            }
        }
    }

    return rc;
}

static int client_read(app_ur_session *elem, int is_tcp_data, app_tcp_conn_info *atc) {

    if (!elem)
        return -1;

    if (elem->state != UR_STATE_READY)
        return -1;

    //elem->ctime = current_time;

    app_ur_conn_info *clnet_info = &(elem->pinfo);
    int err_code = 0;
    uint8_t err_msg[129];
    int rc = 0;
    int applen = 0;

//    if (clnet_verbose && verbose_packets) {
//        TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, "before read ...\n");
//    }

    rc = recv_buffer(clnet_info, &(elem->in_buffer), 0, is_tcp_data, atc, NULL);

//    if (clnet_verbose && verbose_packets) {
//        TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, "read %d bytes\n", (int) rc);
//    }

    if (rc > 0) {

        elem->in_buffer.len = rc;

        uint16_t chnumber = 0;

        message_info mi;
        int miset = 0;
        size_t buffers = 1;

        if (is_tcp_data) {
            if ((int) elem->in_buffer.len == clmessage_length) {
                bcopy((elem->in_buffer.buf), &mi, sizeof(message_info));
                miset = 1;
            } else {
                /* TODO: make a more clean fix */
                buffers = (int) elem->in_buffer.len / clmessage_length;
            }
        } else if (stun_is_indication(&(elem->in_buffer))) {

            uint16_t method = stun_get_method(&elem->in_buffer);

            if ((method == STUN_METHOD_CONNECTION_ATTEMPT) && is_TCP_relay()) {
                stun_attr_ref sar = stun_attr_get_first(&(elem->in_buffer));
                uint32_t cid = 0;
                while (sar) {
                    int attr_type = stun_attr_get_type(sar);
                    if (attr_type == STUN_ATTRIBUTE_CONNECTION_ID) {
                        cid = *((const uint32_t *) stun_attr_get_value(sar));
                        break;
                    }
                    sar = stun_attr_get_next_str(elem->in_buffer.buf, elem->in_buffer.len, sar);
                }
//                if (negative_test) {
//                    tcp_data_connect(elem, (uint64_t) random());
//                } else {
//                    /* positive test */
//                    tcp_data_connect(elem, cid);
//                }
                return rc;
            } else if (method != STUN_METHOD_DATA) {
                TURN_LOG_FUNC(
                        TURN_LOG_LEVEL_INFO,
                        "ERROR: received indication message has wrong method: 0x%x\n",
                        (int) method);
                return rc;
            } else {

                stun_attr_ref sar = stun_attr_get_first_by_type(&(elem->in_buffer),
                                                                STUN_ATTRIBUTE_DATA);
                if (!sar) {
                    TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO,
                                  "ERROR: received DATA message has no data, size=%d\n", rc);
                    return rc;
                }

                int rlen = stun_attr_get_len(sar);
                applen = rlen;
                if (rlen != clmessage_length) {
                    TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO,
                                  "ERROR: received DATA message has wrong len: %d, must be %d\n",
                                  rlen, clmessage_length);
                    //tot_recv_bytes += applen;
                    return rc;
                }

                const uint8_t *data = stun_attr_get_value(sar);

                bcopy(data, &mi, sizeof(message_info));
                miset = 1;
            }

        } else if (stun_is_success_response(&(elem->in_buffer))) {

            if (elem->pinfo.nonce[0]) {
                if (check_integrity(&(elem->pinfo), &(elem->in_buffer)) < 0)
                    return -1;
            }

            if (is_TCP_relay() && (stun_get_method(&(elem->in_buffer)) == STUN_METHOD_CONNECT)) {
                stun_attr_ref sar = stun_attr_get_first(&(elem->in_buffer));
                uint32_t cid = 0;
                while (sar) {
                    int attr_type = stun_attr_get_type(sar);
                    if (attr_type == STUN_ATTRIBUTE_CONNECTION_ID) {
                        cid = *((const uint32_t *) stun_attr_get_value(sar));
                        break;
                    }
                    sar = stun_attr_get_next_str(elem->in_buffer.buf, elem->in_buffer.len, sar);
                }
                //tcp_data_connect(elem, cid);
            }

            return rc;
        } else if (stun_is_challenge_response_str(elem->in_buffer.buf, (size_t) elem->in_buffer.len,
                                                  &err_code, err_msg, sizeof(err_msg),
                                                  clnet_info->realm, clnet_info->nonce,
                                                  clnet_info->server_name, &(clnet_info->oauth))) {
            if (is_TCP_relay() && (stun_get_method(&(elem->in_buffer)) == STUN_METHOD_CONNECT)) {
                //turn_tcp_connect(clnet_verbose, &(elem->pinfo), &(elem->pinfo.peer_addr));
            } else if (stun_get_method(&(elem->in_buffer)) == STUN_METHOD_REFRESH) {
                refresh_channel(elem, stun_get_method(&elem->in_buffer), 600);
            }
            return rc;
        } else if (stun_is_error_response(&(elem->in_buffer), NULL, NULL, 0)) {
            return rc;
        } else if (stun_is_channel_message(&(elem->in_buffer), &chnumber, use_tcp)) {
            if (elem->chnum != chnumber) {
                TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO,
                              "ERROR: received message has wrong channel: %d\n",
                              (int) chnumber);
                return rc;
            }

            if (elem->in_buffer.len >= 4) {
                if (((int) (elem->in_buffer.len - 4) < clmessage_length) ||
                    ((int) (elem->in_buffer.len - 4) > clmessage_length + 3)) {
                    TURN_LOG_FUNC(
                            TURN_LOG_LEVEL_INFO,
                            "ERROR: received buffer have wrong length: %d, must be %d, len=%d\n",
                            rc, clmessage_length + 4, (int) elem->in_buffer.len);
                    return rc;
                }

                bcopy(elem->in_buffer.buf + 4, &mi, sizeof(message_info));
                miset = 1;
                applen = elem->in_buffer.len - 4;
            }
        } else {
            TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO,
                          "ERROR: Unknown message received of size: %d\n",
                          (int) (elem->in_buffer.len));
            return rc;
        }

        if (miset) {
            /*
            printf("%s: 111.111: msgnum=%d, rmsgnum=%d, sent=%lu, recv=%lu\n",__FUNCTION__,
                mi->msgnum,elem->recvmsgnum,(unsigned long)mi->mstime,(unsigned long)current_mstime);
                */
            if (mi.msgnum != elem->recvmsgnum + 1)
                ++(elem->loss);
            else {
//                uint64_t clatency = (uint64_t) time_minus(current_mstime, mi.mstime);
//                if (clatency > max_latency)
//                    max_latency = clatency;
//                if (clatency < min_latency)
//                    min_latency = clatency;
//                elem->latency += clatency;
//                if (elem->rmsgnum > 0) {
//                    uint64_t cjitter = abs(
//                            (int) (current_mstime - elem->recvtimems) - RTP_PACKET_INTERVAL);
//
//                    if (cjitter > max_jitter)
//                        max_jitter = cjitter;
//                    if (cjitter < min_jitter)
//                        min_jitter = cjitter;
//
//                    elem->jitter += cjitter;
//                }
            }

            elem->recvmsgnum = mi.msgnum;
        }

        elem->rmsgnum += buffers;
//        tot_recv_messages += buffers;
//        if (applen > 0)
//            tot_recv_bytes += applen;
//        else
//            tot_recv_bytes += elem->in_buffer.len;
        elem->recvtimems = current_mstime;
        elem->wait_cycles = 0;

    } else if (rc == 0) {
        return 0;
    } else {
        return -1;
    }

    return rc;
}

void client_input_handler(evutil_socket_t fd, short what, void *arg) {
    if (!(what & EV_READ) || !arg) return;
    UNUSED_ARG(fd);
    app_ur_session *elem = (app_ur_session *) arg;
    if (!elem) {
        return;
    }

    switch (elem->state) {
        case UR_STATE_READY:
            do {
                app_tcp_conn_info *atc = NULL;
                int is_tcp_data = 0;
                if (elem->pinfo.tcp_conn) {
                    int i = 0;
                    for (i = 0; i < (int) (elem->pinfo.tcp_conn_number); ++i) {
                        if (elem->pinfo.tcp_conn[i]) {
                            if ((fd == elem->pinfo.tcp_conn[i]->tcp_data_fd) &&
                                (elem->pinfo.tcp_conn[i]->tcp_data_bound)) {
                                is_tcp_data = 1;
                                atc = elem->pinfo.tcp_conn[i];
                                break;
                            }
                        }
                    }
                }
                int rc = client_read(elem, is_tcp_data, atc);
                if (rc <= 0) break;
            } while (1);

            break;
        default:;
    }
}

static int start_c2c(const char *remote_address, int port,
                     const unsigned char *ifname, const char *local_address) {
    LOGE("start_c2c-------------------------------------->");
    app_ur_session *ss1 = create_new_ss();
    app_ur_conn_info *clnet_info1 = &(ss1->pinfo);
    uint16_t chnum1 = 0;
    start_c2c_connection(port, remote_address,
                         ifname, local_address,
                         clnet_verbose,
                         clnet_info1, &chnum1
    );

    socket_set_nonblocking(clnet_info1->fd);

    struct event *ev1 = event_new(client_event_base, clnet_info1->fd,
                                  EV_READ | EV_PERSIST, client_input_handler,
                                  ss1);
    event_add(ev1, NULL);
    ss1->state = UR_STATE_READY;
    ss1->input_ev = ev1;
    ss1->recvmsgnum = -1;
    ss1->chnum = chnum1;
    elems[0] = ss1;
    return 0;
}

//static void timer_handler(evutil_socket_t fd, short event, void *arg) {
//    UNUSED_ARG(fd);
//    UNUSED_ARG(event);
//    UNUSED_ARG(arg);
//
//    __turn_getMSTime();
//
//    if (start_full_timer) {
//        int i = 0;
//        int done = 0;
//        for (i = 0; i < total_clients; ++i) {
//            if (elems[i]) {
//                int finished = client_timer_handler(elems[i], &done);
//                if (finished) {
//                    elems[i] = NULL;
//                }
//            }
//        }
//        if (done > 5 && (dos || random_disconnect)) {
//            for (i = 0; i < total_clients; ++i) {
//                if (elems[i]) {
//                    close(elems[i]->pinfo.fd);
//                    elems[i]->pinfo.fd = -1;
//                }
//            }
//        }
//    }
//}

void start_p2p_client(const char *remote_address, int port, const unsigned char *ifname,
                      const char *local_address) {
    int mclient = 1;
    if (c2c) {
        //mclient must be a multiple of 4:
        if (!no_rtcp)
            mclient += ((4 - (mclient & 0x00000003)) & 0x00000003);
        else if (mclient & 0x1)
            ++mclient;
    } else {
        if (!no_rtcp)
            if (mclient & 0x1)
                ++mclient;
    }
    elems = (app_ur_session **) malloc(
            sizeof(app_ur_session) * ((mclient * 2) + 1) + sizeof(void *));

    memset(buffer_to_send, 7, clmessage_length);
    client_event_base = turn_event_base_new();

    int i = 0;
    int tot_clients = 0;

    if (c2c) {
        if (!no_rtcp) {
            LOGE("elems index=%d", i);
            usleep(SLEEP_INTERVAL);
            if (start_c2c(remote_address, port, ifname, local_address) < 0) {
                LOGE("zbq  start_c2c error");
                free(elems);
                elems = NULL;
                return;
            }
        } else {
            usleep(SLEEP_INTERVAL);
            if (start_c2c(remote_address, port, ifname, local_address) < 0) {
                LOGE("zbq no_rtcp  start_c2c error");
                free(elems);
                elems = NULL;
                return;
            }
            tot_clients += 2;
        }
    } else {
        LOGE("zbq no_rtcp=%d  start_client start", no_rtcp);
        if (!no_rtcp) {
            usleep(SLEEP_INTERVAL);
            if (start_client(remote_address, port, ifname, local_address) < 0) {
                LOGE("zbq rtcp  start_client error");
                free(elems);
                elems = NULL;
                return;
            }
        } else
            for (i = 0; i < mclient; i++) {
                usleep(SLEEP_INTERVAL);
                if (start_client(remote_address, port, ifname, local_address) < 0) {
                    LOGE("zbq no_rtcp  start_client error");
                    free(elems);
                    elems = NULL;
                    return;
                }
                tot_clients++;
            }
    }


//    struct event *ev = event_new(client_event_base, -1, EV_TIMEOUT | EV_PERSIST, timer_handler,
//                                 NULL);
//    struct timeval tv;
//
//    tv.tv_sec = 0;
//    tv.tv_usec = 1000;
//
//    evtimer_add(ev, &tv);

//    if (is_TCP_relay()) {
//        if (passive_tcp) {
//            if (elems[i]->pinfo.is_peer) {
//                int connect_err = 0;
//                socket_connect(elems[i]->pinfo.fd, &(elems[i]->pinfo.remote_addr),
//                               &connect_err);
//            }
//        } else {
//            int j = 0;
//            for (j = i + 1; j < total_clients; j++) {
//                if (turn_tcp_connect(clnet_verbose, &(elems[i]->pinfo),
//                                     &(elems[j]->pinfo.relay_addr)) < 0) {
//                    LOGE("turn_tcp_connect error ");
//                    return;
//                }
//            }
//        }
//    }
//    run_events(1);


    if (client_event_base) {
        event_base_loopexit(client_event_base, 0);
        event_base_got_exit(client_event_base);
        event_base_free(client_event_base);
        client_event_base = NULL;
    }

    //free(elems);
    //elems = NULL;
}

//void add_origin(stun_buffer *message) {
//    if (message && origin[0]) {
//        const char *some_origin = "https://carleon.gov:443";
//        stun_attr_add(message, STUN_ATTRIBUTE_ORIGIN, some_origin, strlen(some_origin));
//        stun_attr_add(message, STUN_ATTRIBUTE_ORIGIN, origin, strlen(origin));
//        some_origin = "ftp://uffrith.net";
//        stun_attr_add(message, STUN_ATTRIBUTE_ORIGIN, some_origin, strlen(some_origin));
//    }
//}
static int refresh_channel(app_ur_session *elem, uint16_t method, uint32_t lt) {

    stun_buffer message;
    app_ur_conn_info *clnet_info = &(elem->pinfo);

    if (clnet_info->is_peer)
        return 0;

    if (!method || (method == STUN_METHOD_REFRESH)) {
        stun_init_request(STUN_METHOD_REFRESH, &message);
        lt = htonl(lt);
        stun_attr_add(&message, STUN_ATTRIBUTE_LIFETIME, (const char *) &lt, 4);

        if (dual_allocation && !mobility) {
            int t = ((uint8_t) random()) % 3;
            if (t) {
                uint8_t field[4];
                field[0] = (t == 1) ? (uint8_t) STUN_ATTRIBUTE_REQUESTED_ADDRESS_FAMILY_VALUE_IPV4
                                    : (uint8_t) STUN_ATTRIBUTE_REQUESTED_ADDRESS_FAMILY_VALUE_IPV6;
                field[1] = 0;
                field[2] = 0;
                field[3] = 0;
                stun_attr_add(&message, STUN_ATTRIBUTE_REQUESTED_ADDRESS_FAMILY,
                              (const char *) field, 4);
            }
        }

        add_origin(&message);
        if (add_integrity(clnet_info, &message) < 0) return -1;
        if (use_fingerprints)
            stun_attr_add_fingerprint_str(message.buf, (size_t *) &(message.len));
        send_buffer(clnet_info, &message, 0, 0);
    }

    if (lt && !addr_any(&(elem->pinfo.peer_addr))) {

        if (!no_permissions) {
            if (!method || (method == STUN_METHOD_CREATE_PERMISSION)) {
                stun_init_request(STUN_METHOD_CREATE_PERMISSION, &message);
                stun_attr_add_addr(&message, STUN_ATTRIBUTE_XOR_PEER_ADDRESS,
                                   &(elem->pinfo.peer_addr));
                add_origin(&message);
                if (add_integrity(clnet_info, &message) < 0) return -1;
                if (use_fingerprints)
                    stun_attr_add_fingerprint_str(message.buf, (size_t *) &(message.len));
                send_buffer(&(elem->pinfo), &message, 0, 0);
            }
        }

        if (!method || (method == STUN_METHOD_CHANNEL_BIND)) {
            if (STUN_VALID_CHANNEL(elem->chnum)) {
                stun_set_channel_bind_request(&message, &(elem->pinfo.peer_addr), elem->chnum);
                add_origin(&message);
                if (add_integrity(clnet_info, &message) < 0) return -1;
                if (use_fingerprints)
                    stun_attr_add_fingerprint_str(message.buf, (size_t *) &(message.len));
                send_buffer(&(elem->pinfo), &message, 1, 0);
            }
        }
    }

    elem->refresh_time = current_mstime + 30 * 1000;

    return 0;
}


int add_integrity(app_ur_conn_info *clnet_info, stun_buffer *message) {
    if (clnet_info->nonce[0]) {

        if (oauth && clnet_info->oauth) {

            uint16_t method = stun_get_method_str(message->buf, message->len);

            int cok = clnet_info->cok;

            if (((method == STUN_METHOD_ALLOCATE) || (method == STUN_METHOD_REFRESH)) ||
                !(clnet_info->key_set)) {

                cok = ((unsigned short) random()) % 3;
                clnet_info->cok = cok;
                oauth_token otoken;
                encoded_oauth_token etoken;
                uint8_t nonce[12];
                RAND_bytes((unsigned char *) nonce, 12);
                long halflifetime = OAUTH_SESSION_LIFETIME / 2;
                long random_lifetime = 0;
                while (!random_lifetime) {
                    random_lifetime = random();
                }
                if (random_lifetime < 0) random_lifetime = -random_lifetime;
                random_lifetime = random_lifetime % halflifetime;
                otoken.enc_block.lifetime = (uint32_t) (halflifetime + random_lifetime);
                otoken.enc_block.timestamp = ((uint64_t) turn_time()) << 16;
                if (shatype == SHATYPE_SHA256) {
                    otoken.enc_block.key_length = 32;
                } else if (shatype == SHATYPE_SHA384) {
                    otoken.enc_block.key_length = 48;
                } else if (shatype == SHATYPE_SHA512) {
                    otoken.enc_block.key_length = 64;
                } else {
                    otoken.enc_block.key_length = 20;
                }
                RAND_bytes((unsigned char *) (otoken.enc_block.mac_key),
                           otoken.enc_block.key_length);
                if (encode_oauth_token(clnet_info->server_name, &etoken, &(okey_array[cok]),
                                       &otoken, nonce) < 0) {
                    TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, " Cannot encode token\n");
                    return -1;
                }
                stun_attr_add_str(message->buf, (size_t *) &(message->len),
                                  STUN_ATTRIBUTE_OAUTH_ACCESS_TOKEN,
                                  (const uint8_t *) etoken.token, (int) etoken.size);

                bcopy(otoken.enc_block.mac_key, clnet_info->key, otoken.enc_block.key_length);
                clnet_info->key_set = 1;
            }

            if (stun_attr_add_integrity_by_key_str(message->buf, (size_t *) &(message->len),
                                                   (uint8_t *) okey_array[cok].kid,
                                                   clnet_info->realm, clnet_info->key,
                                                   clnet_info->nonce, shatype) < 0) {
                TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, " Cannot add integrity to the message\n");
                return -1;
            }

            //self-test:
            {
                password_t pwd;
                if (stun_check_message_integrity_by_key_str(get_turn_credentials_type(),
                                                            message->buf, (size_t) (message->len),
                                                            clnet_info->key, pwd, shatype) < 1) {
                    TURN_LOG_FUNC(TURN_LOG_LEVEL_ERROR,
                                  " Self-test of integrity does not comple correctly !\n");
                    return -1;
                }
            }
        } else {
            if (stun_attr_add_integrity_by_user_str(message->buf, (size_t *) &(message->len),
                                                    g_uname,
                                                    clnet_info->realm, g_upwd, clnet_info->nonce,
                                                    shatype) < 0) {
                TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, " Cannot add integrity to the message\n");
                return -1;
            }
        }
    }

    return 0;
}

int check_integrity(app_ur_conn_info *clnet_info, stun_buffer *message) {
    SHATYPE sht = shatype;

    if (oauth && clnet_info->oauth) {

        password_t pwd;

        return stun_check_message_integrity_by_key_str(get_turn_credentials_type(),
                                                       message->buf, (size_t) (message->len),
                                                       clnet_info->key, pwd, sht);

    } else {

        if (stun_check_message_integrity_str(get_turn_credentials_type(),
                                             message->buf, (size_t) (message->len), g_uname,
                                             clnet_info->realm, g_upwd, sht) < 1) {
            TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO,
                          "Wrong integrity in a message received from server\n");
            return -1;
        }
    }

    return 0;
}

int send_buffer(app_ur_conn_info *clnet_info, stun_buffer *message, int data_connection,
                app_tcp_conn_info *atc) {

    int rc = 0;
    int ret = -1;

    char *buffer = (char *) (message->buf);

    if (negative_protocol_test && (message->len > 0)) {
        if (random() % 10 == 0) {
            int np = (int) ((unsigned long) random() % 10);
            while (np-- > 0) {
                int pos = (int) ((unsigned long) random() % (unsigned long) message->len);
                int val = (int) ((unsigned long) random() % 256);
                message->buf[pos] = (uint8_t) val;
            }
        }
    }

    SSL *ssl = clnet_info->ssl;
    ioa_socket_raw fd = clnet_info->fd;

    if (data_connection) {
        if (atc) {
            ssl = atc->tcp_data_ssl;
            fd = atc->tcp_data_fd;
        } else if (is_TCP_relay()) {
            TURN_LOG_FUNC(TURN_LOG_LEVEL_ERROR,
                          "trying to send tcp data buffer over unready connection: size=%d\n",
                          (int) (message->len));
            return -1;
        }
    }

    if (ssl) {

        int message_sent = 0;
        while (!message_sent) {

            if (SSL_get_shutdown(ssl)) {
                return -1;
            }

            int len = 0;
            do {
                len = SSL_write(ssl, buffer, (int) message->len);
            } while (len < 0 && ((errno == EINTR) || (errno == ENOBUFS)));

            if (len == (int) message->len) {
                if (clnet_verbose) {
                    TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO,
                                  "buffer sent: size=%d\n", len);
                }

                message_sent = 1;
                ret = len;
            } else {
                switch (SSL_get_error(ssl, len)) {
                    case SSL_ERROR_NONE:
                        /* Try again ? */
                        break;
                    case SSL_ERROR_WANT_WRITE:
                        /* Just try again later */
                        break;
                    case SSL_ERROR_WANT_READ:
                        /* continue with reading */
                        break;
                    case SSL_ERROR_ZERO_RETURN:
                        /* Try again */
                        break;
                    case SSL_ERROR_SYSCALL:
                        TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, "Socket write error 111.666: \n");
                        if (handle_socket_error())
                            break;
                        /* Falls through. */
                    case SSL_ERROR_SSL: {
                        TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, "SSL write error: \n");
                        char buf[1024];
                        TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO,
                                      "%s (%d)\n",
                                      ERR_error_string(ERR_get_error(), buf),
                                      SSL_get_error(ssl, len));
                    }
                        /* Falls through. */
                    default:
                        clnet_info->broken = 1;
                        TURN_LOG_FUNC(TURN_LOG_LEVEL_INFO, "Unexpected error while writing!\n");
                        return -1;
                }
            }
        }

    } else if (fd >= 0) {

        size_t left = (size_t) (message->len);

        while (left > 0) {
            do {
                rc = send(fd, buffer, left, 0);
            } while (rc <= 0 && ((errno == EINTR) || (errno == ENOBUFS) || (errno == EAGAIN)));
            if (rc > 0) {
                left -= (size_t) rc;
                buffer += rc;
            } else {
                //tot_send_dropped += 1;
                break;
            }
        }

        if (left > 0)
            return -1;

        ret = (int) message->len;
    }

    return ret;
}

turn_credential_type get_turn_credentials_type(void) {
    return TURN_CREDENTIALS_LONG_TERM;
}

SOCKET_TYPE get_socket_type() {
    if (use_sctp) {
        if (use_secure) {
            return TLS_SCTP_SOCKET;
        } else {
            return SCTP_SOCKET;
        }
    } else if (use_tcp) {
        if (use_secure) {
            return TLS_SOCKET;
        } else {
            return TCP_SOCKET;
        }
    } else {
        if (use_secure) {
            return DTLS_SOCKET;
        } else {
            return UDP_SOCKET;
        }
    }
}

void bind_client(int type,const char * address,int port){
    if (!dos) {
        if (!do_not_use_channel) {
            /* These multiple "channel bind" requests are here only because
             * we are playing with the TURN server trying to screw it */
            if (turn_channel_bind(verbose, chn, clnet_info, &peer_addr_rtcp)
                < 0) {
                LOGE("zbq ---- turn_channel_bind error");
                return ;
            }
            if (rare_event()) return ;

            if (turn_channel_bind(verbose, chn, clnet_info, &peer_addr_rtcp)
                < 0) {
                LOGE("zbq ---- turn_channel_bind error 2");
                return ;
            }
            if (rare_event()) return ;
            *chn = 0;
            if (turn_channel_bind(verbose, chn, clnet_info, &peer_addr) < 0) {
                LOGE("zbq ---- turn_channel_bind error 3");
                return ;
            }

            if (rare_event()) return ;
            if (turn_channel_bind(verbose, chn, clnet_info, &peer_addr) < 0) {
                LOGE("zbq ---- turn_channel_bind error 4");
                return ;
            }
            if (rare_event()) return ;

            if (extra_requests) {
                const char *sarbaddr = "164.156.178.190";
                if (random() % 2 == 0)
                    sarbaddr = "2001::172";
                ioa_addr arbaddr;
                make_ioa_addr((const uint8_t *) sarbaddr, 333, &arbaddr);
                int i;
                int maxi = (unsigned short) random() % EXTRA_CREATE_PERMS;
                for (i = 0; i < maxi; i++) {
                    uint16_t chni = 0;
                    int port = (unsigned short) random();
                    if (port < 1024) port += 1024;
                    addr_set_port(&arbaddr, port);
                    uint8_t *u = (uint8_t *) &(arbaddr.s4.sin_addr);
                    u[(unsigned short) random() % 4] = u[(unsigned short) random() % 4] + 1;
                    //char sss[128];
                    //addr_to_string(&arbaddr,(uint8_t*)sss);
                    //printf("%s: 111.111: %s\n",__FUNCTION__,sss);
                    turn_channel_bind(verbose, &chni, clnet_info, &arbaddr);
                }
            }

            if (!no_rtcp) {
                if (turn_channel_bind(verbose, chn_rtcp, clnet_info_rtcp,
                                      &peer_addr_rtcp) < 0) {
                    LOGE("zbq ---- turn_channel_bind error 5");
                    return ;
                }
            }
            if (rare_event()) return ;

            if (extra_requests) {
                const char *sarbaddr = "64.56.78.90";
                if (random() % 2 == 0)
                    sarbaddr = "2001::172";
                ioa_addr arbaddr[EXTRA_CREATE_PERMS];
                make_ioa_addr((const uint8_t *) sarbaddr, 333, &arbaddr[0]);
                int i;
                int maxi = (unsigned short) random() % EXTRA_CREATE_PERMS;
                for (i = 0; i < maxi; i++) {
                    if (i > 0)
                        addr_cpy(&arbaddr[i], &arbaddr[0]);
                    addr_set_port(&arbaddr[i], (unsigned short) random());
                    uint8_t *u = (uint8_t *) &(arbaddr[i].s4.sin_addr);
                    u[(unsigned short) random() % 4] = u[(unsigned short) random() % 4] + 1;
                    //char sss[128];
                    //addr_to_string(&arbaddr[i],(uint8_t*)sss);
                    //printf("%s: 111.111: %s\n",__FUNCTION__,sss);
                }
                turn_create_permission(verbose, clnet_info, arbaddr, maxi);
            }
        } else {

            int before = (random() % 2 == 0);

            if (before) {
                if (turn_create_permission(verbose, clnet_info, &peer_addr, 1) < 0) {
                    LOGE("zbq ---- turn_create_permission error ");
                    return -1;
                }
                if (rare_event()) return 0;
                if (turn_create_permission(verbose, clnet_info, &peer_addr_rtcp, 1)
                    < 0) {
                    LOGE("zbq ---- turn_create_permission error 2");
                    return ;
                }
                if (rare_event()) return ;
            }

            if (extra_requests) {
                const char *sarbaddr = "64.56.78.90";
                if (random() % 2 == 0)
                    sarbaddr = "2001::172";
                ioa_addr arbaddr[EXTRA_CREATE_PERMS];
                make_ioa_addr((const uint8_t *) sarbaddr, 333, &arbaddr[0]);
                int i;
                int maxi = (unsigned short) random() % EXTRA_CREATE_PERMS;
                for (i = 0; i < maxi; i++) {
                    if (i > 0)
                        addr_cpy(&arbaddr[i], &arbaddr[0]);
                    addr_set_port(&arbaddr[i], (unsigned short) random());
                    uint8_t *u = (uint8_t *) &(arbaddr[i].s4.sin_addr);
                    u[(unsigned short) random() % 4] = u[(unsigned short) random() % 4] + 1;
                    //char sss[128];
                    //addr_to_string(&arbaddr,(uint8_t*)sss);
                    //printf("%s: 111.111: %s\n",__FUNCTION__,sss);
                }
                turn_create_permission(verbose, clnet_info, arbaddr, maxi);
            }

            if (!before) {
                if (turn_create_permission(verbose, clnet_info, &peer_addr, 1) < 0) {
                    LOGE("zbq ---- turn_create_permission error 3");
                    return ;
                }
                if (rare_event()) return 0;
                if (turn_create_permission(verbose, clnet_info, &peer_addr_rtcp, 1)
                    < 0) {
                    LOGE("zbq ---- turn_create_permission error 4");
                    return ;
                }
                if (rare_event()) return ;
            }

            if (!no_rtcp) {
                if (turn_create_permission(verbose, clnet_info_rtcp,
                                           &peer_addr_rtcp, 1) < 0) {
                    LOGE("zbq ---- turn_create_permission error 5");
                    return -;
                }
                if (rare_event()) return ;
                if (turn_create_permission(verbose, clnet_info_rtcp, &peer_addr, 1)
                    < 0) {
                    LOGE("zbq ---- turn_create_permission error 6");
                    return ;
                }
                if (rare_event()) return ;
            }
        }
    }
}