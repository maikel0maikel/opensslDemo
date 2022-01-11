//
// Created by maikel on 1/6/22.
//

#include <err.h>
#include "stun_manager.h"
#include "ns_turn_utils.h"
#include "apputils.h"
#include "stun_buffer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef __cplusplus
#include "TurnMsgLib.h"
#endif

on_address_cb reflexive_cb;
on_address_cb other_cb;
on_address_cb response_origin_cb;
int local_port = -1;
char local_addr[256]="\0";
int forceRfc5780 = 0;

int udp_fd = -1;
ioa_addr real_local_addr;
int counter = 0;

void notify_address(int type ,const ioa_addr *addr,on_address_cb cb){
    char addrbuf[INET6_ADDRSTRLEN];
    memset(addrbuf, 0, INET6_ADDRSTRLEN);
    inet_ntop(AF_INET,
              &addr->s4.sin_addr, addrbuf, INET6_ADDRSTRLEN);
    int port = nswap16(addr->s4.sin_port);
    if (cb){
        cb(type,addrbuf,port);
    }
}

#ifdef __cplusplus
int run_stunclient(const char* rip, int rport, int *port, int *rfc5780, int response_port, int change_ip, int change_port, int padding)
{

	ioa_addr remote_addr;
	int new_udp_fd = -1;

	memset((void *) &remote_addr, 0, sizeof(ioa_addr));
	if (make_ioa_addr((const uint8_t*) rip, rport, &remote_addr) < 0)
		err(-1, NULL);

	if (udp_fd < 0) {
		udp_fd = socket(remote_addr.ss.sa_family, SOCK_DGRAM, 0);
		if (udp_fd < 0)
			err(-1, NULL);

		if (!addr_any(&real_local_addr)) {
			if (addr_bind(udp_fd, &real_local_addr,0,1,UDP_SOCKET) < 0)
				err(-1, NULL);
		}
	}

	if (response_port >= 0) {

		new_udp_fd = socket(remote_addr.ss.sa_family, SOCK_DGRAM, 0);
		if (new_udp_fd < 0)
			err(-1, NULL);

		addr_set_port(&real_local_addr, response_port);

		if (addr_bind(new_udp_fd, &real_local_addr, 0, 1, UDP_SOCKET) < 0)
			err(-1, NULL);
	}

	turn::StunMsgRequest req(STUN_METHOD_BINDING);

	req.constructBindingRequest();

	if (response_port >= 0) {
	  turn::StunAttrResponsePort rpa;
		rpa.setResponsePort((uint16_t)response_port);
		try {
			req.addAttr(rpa);
		} catch(turn::WrongStunAttrFormatException &ex1) {
			printf("Wrong rp attr format\n");
			exit(-1);
		} catch(turn::WrongStunBufferFormatException &ex2) {
			printf("Wrong stun buffer format (1)\n");
			exit(-1);
		} catch(...) {
			printf("Wrong something (1)\n");
			exit(-1);
		}
	}
	if (change_ip || change_port) {
		turn::StunAttrChangeRequest cra;
		cra.setChangeIp(change_ip);
		cra.setChangePort(change_port);
		try {
			req.addAttr(cra);
		} catch(turn::WrongStunAttrFormatException &ex1) {
			printf("Wrong cr attr format\n");
			exit(-1);
		} catch(turn::WrongStunBufferFormatException &ex2) {
			printf("Wrong stun buffer format (2)\n");
			exit(-1);
		} catch(...) {
			printf("Wrong something (2)\n");
			exit(-1);
		}
	}
	if (padding) {
		turn::StunAttrPadding pa;
		pa.setPadding(1500);
		try {
			req.addAttr(pa);
		} catch(turn::WrongStunAttrFormatException &ex1) {
			printf("Wrong p attr format\n");
			exit(-1);
		} catch(turn::WrongStunBufferFormatException &ex2) {
			printf("Wrong stun buffer format (3)\n");
			exit(-1);
		} catch(...) {
			printf("Wrong something (3)\n");
			exit(-1);
		}
	}

	{
		int len = 0;
		int slen = get_ioa_addr_len(&remote_addr);

		do {
			len = sendto(udp_fd, req.getRawBuffer(), req.getSize(), 0, (struct sockaddr*) &remote_addr, (socklen_t) slen);
		} while (len < 0 && ((errno == EINTR) || (errno == ENOBUFS) || (errno == EAGAIN)));

		if (len < 0)
			err(-1, NULL);

	}

	if (addr_get_from_sock(udp_fd, &real_local_addr) < 0) {
		printf("%s: Cannot get address from local socket\n", __FUNCTION__);
	} else {
		*port = addr_get_port(&real_local_addr);
	}

	{
		if(new_udp_fd >= 0) {
			close(udp_fd);
			udp_fd = new_udp_fd;
			new_udp_fd = -1;
		}
	}

	{
		int len = 0;
		stun_buffer buf;
		uint8_t *ptr = buf.buf;
		int recvd = 0;
		const int to_recv = sizeof(buf.buf);

		do {
			len = recv(udp_fd, ptr, to_recv - recvd, 0);
			if (len > 0) {
				recvd += len;
				ptr += len;
				break;
			}
		} while (len < 0 && (errno == EINTR));

		if (recvd > 0)
			len = recvd;
		buf.len = len;

		try {
			turn::StunMsgResponse res(buf.buf, sizeof(buf.buf), (size_t)buf.len, true);

			if (res.isCommand()) {

				if(res.isSuccess()) {

					if (res.isBindingResponse()) {

						ioa_addr reflexive_addr;
						addr_set_any(&reflexive_addr);
						turn::StunAttrIterator iter(res,STUN_ATTRIBUTE_XOR_MAPPED_ADDRESS);
						if (!iter.eof()) {

							turn::StunAttrAddr addr(iter);
							addr.getAddr(reflexive_addr);

							turn::StunAttrIterator iter1(res,STUN_ATTRIBUTE_OTHER_ADDRESS);
							if (!iter1.eof()) {
								*rfc5780 = 1;
								printf("\n========================================\n");
								printf("RFC 5780 response %d\n",++counter);
								ioa_addr other_addr;
								turn::StunAttrAddr addr1(iter1);
								addr1.getAddr(other_addr);
								turn::StunAttrIterator iter2(res,STUN_ATTRIBUTE_RESPONSE_ORIGIN);
								if (!iter2.eof()) {
									ioa_addr response_origin;
									turn::StunAttrAddr addr2(iter2);
									addr2.getAddr(response_origin);
									notify_address(&response_origin,response_origin_cb);
								}
								notify_address(&other_addr,other_cb);
							}
							 notify_address(&reflexive_addr,reflexive_cb);

						} else {
							printf("Cannot read the response\n");
						}
					} else {
						printf("Wrong type of response\n");
					}
				} else {
					int err_code = res.getError();
					std::string reason = res.getReason();

					printf("The response is an error %d (%s)\n", err_code, reason.c_str());
				}
			} else {
				printf("The response is not a response message\n");
			}
		} catch(...) {
			printf("The response is not a well formed STUN message\n");
		}
	}

	return 0;
}
#else



int run_stunclient(const char* rip, int rport, int *port, int *rfc5780, int response_port, int change_ip, int change_port, int padding){
    ioa_addr remote_addr;
    int new_udp_fd = -1;
    stun_buffer buf;

    bzero(&remote_addr, sizeof(remote_addr));
    if (make_ioa_addr((const uint8_t*) rip, rport, &remote_addr) < 0)
        err(-1, NULL);

    if (udp_fd < 0) {
        udp_fd = socket(remote_addr.ss.sa_family, CLIENT_DGRAM_SOCKET_TYPE, CLIENT_DGRAM_SOCKET_PROTOCOL);
        if (udp_fd < 0)
            err(-1, NULL);

        if (!addr_any(&real_local_addr)) {
            if (addr_bind(udp_fd, &real_local_addr,0,1,UDP_SOCKET) < 0)
                err(-1, NULL);
        }
    }

    if (response_port >= 0) {

        new_udp_fd = socket(remote_addr.ss.sa_family, CLIENT_DGRAM_SOCKET_TYPE, CLIENT_DGRAM_SOCKET_PROTOCOL);
        if (new_udp_fd < 0)
            err(-1, NULL);

        addr_set_port(&real_local_addr, response_port);

        if (addr_bind(new_udp_fd, &real_local_addr,0,1,UDP_SOCKET) < 0)
            err(-1, NULL);
    }

    stun_prepare_binding_request(&buf);

    if (response_port >= 0) {
        stun_attr_add_response_port_str((uint8_t*) (buf.buf), (size_t*) &(buf.len), (uint16_t) response_port);
    }
    if (change_ip || change_port) {
        stun_attr_add_change_request_str((uint8_t*) buf.buf, (size_t*) &(buf.len), change_ip, change_port);
    }
    if (padding) {
        if(stun_attr_add_padding_str((uint8_t*) buf.buf, (size_t*) &(buf.len), 1500)<0) {
            printf("%s: ERROR: Cannot add padding\n",__FUNCTION__);
        }
    }

    {
        int len = 0;
        int slen = get_ioa_addr_len(&remote_addr);

        do {
            len = sendto(udp_fd, buf.buf, buf.len, 0, (struct sockaddr*) &remote_addr, (socklen_t) slen);
        } while (len < 0 && ((errno == EINTR) || (errno == ENOBUFS) || (errno == EAGAIN)));

        if (len < 0)
            err(-1, NULL);

    }

    if (addr_get_from_sock(udp_fd, &real_local_addr) < 0) {
        printf("%s: Cannot get address from local socket\n", __FUNCTION__);
    } else {
        *port = addr_get_port(&real_local_addr);
    }

    {
        if(new_udp_fd >= 0) {
            socket_closesocket(udp_fd);
            udp_fd = new_udp_fd;
            new_udp_fd = -1;
        }
    }

    {
        int len = 0;
        uint8_t *ptr = buf.buf;
        int recvd = 0;
        const int to_recv = sizeof(buf.buf);

        do {
            len = recv(udp_fd, ptr, to_recv - recvd, 0);
            if (len > 0) {
                recvd += len;
                ptr += len;
                break;
            }
        } while (len < 0 && ((errno == EINTR) || (errno == EAGAIN)));

        if (recvd > 0)
            len = recvd;
        buf.len = len;

        if (stun_is_command_message(&buf)) {

            if (stun_is_response(&buf)) {

                if (stun_is_success_response(&buf)) {

                    if (stun_is_binding_response(&buf)) {

                        ioa_addr reflexive_addr;
                        addr_set_any(&reflexive_addr);
                        if (stun_attr_get_first_addr(&buf, STUN_ATTRIBUTE_XOR_MAPPED_ADDRESS, &reflexive_addr, NULL) >= 0) {

                            stun_attr_ref sar = stun_attr_get_first_by_type_str(buf.buf, buf.len, STUN_ATTRIBUTE_OTHER_ADDRESS);
                            if (sar) {
                                *rfc5780 = 1;
                                printf("\n========================================\n");
                                printf("RFC 5780 response %d\n",++counter);
                                ioa_addr other_addr;
                                stun_attr_get_addr_str((uint8_t *) buf.buf, (size_t) buf.len, sar, &other_addr, NULL);
                                sar = stun_attr_get_first_by_type_str(buf.buf, buf.len, STUN_ATTRIBUTE_RESPONSE_ORIGIN);
                                if (sar) {
                                    ioa_addr response_origin;
                                    stun_attr_get_addr_str((uint8_t *) buf.buf, (size_t) buf.len, sar, &response_origin, NULL);
                                    //addr_debug_print(1, &response_origin, "Response origin: ");
                                    notify_address(REMOTE_IP,&response_origin,response_origin_cb);
                                }
                                //addr_debug_print(1, &other_addr, "Other addr: ");
                                notify_address(OTHER_IP,&other_addr,other_cb);
                            }
                            notify_address(REFLEXIVE_IP,&reflexive_addr,reflexive_cb);
                            notify_address(REFLEXIVE_IP,&real_local_addr,reflexive_cb);
                        } else {
                            printf("Cannot read the response\n");
                        }
                    } else {
                        printf("Wrong type of response\n");
                    }
                } else {
                    int err_code = 0;
                    uint8_t err_msg[1025] = "\0";
                    size_t err_msg_size = sizeof(err_msg);
                    if (stun_is_error_response(&buf, &err_code, err_msg, err_msg_size)) {
                        printf("The response is an error %d (%s)\n", err_code, (char*) err_msg);
                    } else {
                        printf("The response is an unrecognized error\n");
                    }
                }
            } else {
                printf("The response is not a response message\n");
            }
        } else {
            printf("The response is not a STUN message\n");
        }
    }

    return 0;
}
#endif

int run_stun(const char* rip, int rport){
    int local_port = -1;
    int rfc5780 = 0;

    addr_set_any(&real_local_addr);

    if(local_addr[0]) {
        if(make_ioa_addr((const uint8_t*)local_addr, 0, &real_local_addr)<0) {
            err(-1,NULL);
        }
    }

    run_stunclient(rip, rport, &local_port, &rfc5780,-1,0,0,0);

    if(rfc5780 || forceRfc5780) {
        run_stunclient(rip, rport, &local_port, &rfc5780,local_port+1,1,1,0);
        run_stunclient(rip, rport, &local_port, &rfc5780,-1,1,1,1);
    }
}

void set_forceRfc5780(int rfc5780){
    forceRfc5780 = rfc5780;
}

void set_local_address(const char * local_address){
    //STRCPY(local_addr, local_address);
    bzero(local_addr, sizeof(local_addr));
    strcpy(local_addr,local_address);
}

void init_stun(const char* remote_address,int port){
    int rfc5780 = 0;
    run_stunclient(remote_address, port, &local_port, &rfc5780,-1,0,0,0);
    printf("init_stun rfc5780=%d\n",rfc5780);
    if(rfc5780 || forceRfc5780) {
        run_stunclient(remote_address, port, &local_port, &rfc5780,local_port+1,1,1,0);
        run_stunclient(remote_address, port, &local_port, &rfc5780,-1,1,1,1);
    }
}

void set_reflexive_cb(on_address_cb cb){
    reflexive_cb = cb;
}
void set_other_cb(on_address_cb cb){
    other_cb = cb;
}
void set_response_origin_cb(on_address_cb cb){
    response_origin_cb = cb;
}