/*
 * Minimal RDMA ping-pong example (C, libibverbs + librdmacm)
 * Purpose: measure latency/throughput between two nodes using RDMA SEND/RECV.
 *
 * Usage:
 *   Server: ./rdma_pingpong --server --bind 10.0.0.10
 *   Client: ./rdma_pingpong --connect 10.0.0.10
 *
 * NOTE: Educational demo; robust error handling and edge cases are omitted.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <rdma/rdma_cma.h>

#define MSG_SIZE 4096    // message bytes per SEND/RECV
#define ITERS    1000    // iterations for ping-pong

static void die(const char* m){ perror(m); exit(1); }

int main(int argc, char** argv){
    int is_server = 0;
    const char* ip = NULL;
    static struct option opts[] = {
        {"server",  no_argument,       0, 's'},
        {"bind",    required_argument, 0, 'b'},
        {"connect", required_argument, 0, 'c'},
        {0,0,0,0}
    };
    for(;;){
        int idx=0; int c=getopt_long(argc, argv, "sb:c:", opts, &idx);
        if (c==-1) break;
        if (c=='s') is_server=1;
        else if (c=='b') ip=optarg, is_server=1;
        else if (c=='c') ip=optarg, is_server=0;
    }
    if (!ip){
        fprintf(stderr, "Usage: --server --bind <IP>  OR  --connect <IP>\n");
        return 2;
    }

    struct rdma_cm_id* id = NULL, *listen_id = NULL;
    struct rdma_event_channel* ec = rdma_create_event_channel();
    if (!ec) die("rdma_create_event_channel");

    if (is_server){
        // Resolve IP and bind for listening
        struct addrinfo *res;
        struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM, .ai_flags = AI_PASSIVE };
        if (getaddrinfo(ip, "20079", &hints, &res)) die("getaddrinfo");
        if (rdma_create_id(ec, &listen_id, NULL, RDMA_PS_TCP)) die("rdma_create_id");
        if (rdma_bind_addr(listen_id, res->ai_addr)) die("rdma_bind_addr");
        if (rdma_listen(listen_id, 1)) die("rdma_listen");
        printf("[server] listening on %s:20079 ...\n", ip);

        struct rdma_cm_event* ev;
        if (rdma_get_cm_event(ec, &ev)) die("rdma_get_cm_event");
        if (ev->event != RDMA_CM_EVENT_CONNECT_REQUEST) die("CONNECT_REQUEST expected");
        id = ev->id;
        rdma_ack_cm_event(ev);
    } else {
        // Client: resolve route + connect
        if (rdma_create_id(ec, &id, NULL, RDMA_PS_TCP)) die("rdma_create_id");
        struct addrinfo *res;
        struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
        if (getaddrinfo(ip, "20079", &hints, &res)) die("getaddrinfo");
        if (rdma_resolve_addr(id, NULL, res->ai_addr, 2000)) die("rdma_resolve_addr");
        struct rdma_cm_event* ev;
        if (rdma_get_cm_event(ec, &ev)) die("rdma_get_cm_event");
        if (ev->event != RDMA_CM_EVENT_ADDR_RESOLVED) die("ADDR_RESOLVED expected");
        rdma_ack_cm_event(ev);

        if (rdma_resolve_route(id, 2000)) die("rdma_resolve_route");
        if (rdma_get_cm_event(ec, &ev)) die("rdma_get_cm_event");
        if (ev->event != RDMA_CM_EVENT_ROUTE_RESOLVED) die("ROUTE_RESOLVED expected");
        rdma_ack_cm_event(ev);
    }

    // PD + CQ + QP
    struct ibv_pd* pd = ibv_alloc_pd(id->verbs);
    if (!pd) die("ibv_alloc_pd");
    struct ibv_cq* cq = ibv_create_cq(id->verbs, 16, NULL, NULL, 0);
    if (!cq) die("ibv_create_cq");

    struct ibv_qp_init_attr qpia = {0};
    qpia.qp_type = IBV_QPT_RC;     // reliable connection
    qpia.send_cq = cq;
    qpia.recv_cq = cq;
    qpia.cap.max_send_wr = 16;
    qpia.cap.max_recv_wr = 16;
    qpia.cap.max_send_sge = 1;
    qpia.cap.max_recv_sge = 1;
    if (rdma_create_qp(id, pd, &qpia)) die("rdma_create_qp");

    // Memory
    char *send_buf = NULL, *recv_buf = NULL;
    posix_memalign((void**)&send_buf, sysconf(_SC_PAGESIZE), MSG_SIZE);
    posix_memalign((void**)&recv_buf,  sysconf(_SC_PAGESIZE), MSG_SIZE);
    memset(send_buf, 0xA5, MSG_SIZE);
    memset(recv_buf,  0x5A, MSG_SIZE);
    struct ibv_mr* send_mr = ibv_reg_mr(pd, send_buf, MSG_SIZE, IBV_ACCESS_LOCAL_WRITE);
    struct ibv_mr* recv_mr = ibv_reg_mr(pd, recv_buf,  MSG_SIZE, IBV_ACCESS_LOCAL_WRITE);
    if (!send_mr || !recv_mr) die("ibv_reg_mr");

    // Post one recv
    struct ibv_sge rsge = { .addr=(uintptr_t)recv_buf, .length=MSG_SIZE, .lkey=recv_mr->lkey };
    struct ibv_recv_wr rwr = { .wr_id=1, .sg_list=&rsge, .num_sge=1 };
    struct ibv_recv_wr* rbad = NULL;
    if (ibv_post_recv(id->qp, &rwr, &rbad)) die("ibv_post_recv");

    // Connect or accept
    struct rdma_conn_param cparam = { .initiator_depth = 1, .responder_resources = 1, .rnr_retry_count = 7 };
    if (is_server){
        if (rdma_accept(id, &cparam)) die("rdma_accept");
        struct rdma_cm_event* ev;
        if (rdma_get_cm_event(ec, &ev)) die("rdma_get_cm_event");
        if (ev->event != RDMA_CM_EVENT_ESTABLISHED) die("ESTABLISHED expected");
        rdma_ack_cm_event(ev);
        printf("[server] connection established\\n");
    } else {
        if (rdma_connect(id, &cparam)) die("rdma_connect");
        struct rdma_cm_event* ev;
        if (rdma_get_cm_event(ec, &ev)) die("rdma_get_cm_event");
        if (ev->event != RDMA_CM_EVENT_ESTABLISHED) die("ESTABLISHED expected");
        rdma_ack_cm_event(ev);
        printf("[client] connection established\\n");
    }

    // Ping-pong loop
    struct ibv_sge ssge = { .addr=(uintptr_t)send_buf, .length=MSG_SIZE, .lkey=send_mr->lkey };
    struct ibv_send_wr swr = { .wr_id=2, .sg_list=&ssge, .num_sge=1, .opcode=IBV_WR_SEND, .send_flags=IBV_SEND_SIGNALED };
    struct ibv_send_wr* sbad = NULL;
    struct ibv_wc wc;
    int i;
    if (!is_server){
        for (i=0;i<ITERS;i++){
            if (ibv_post_send(id->qp, &swr, &sbad)) die("ibv_post_send");
            while (ibv_poll_cq(cq, 1, &wc) <= 0) {}
            if (wc.status != IBV_WC_SUCCESS) die("send wc");
            if (ibv_post_recv(id->qp, &rwr, &rbad)) die("ibv_post_recv");
            while (ibv_poll_cq(cq, 1, &wc) <= 0) {}
            if (wc.status != IBV_WC_SUCCESS) die("recv wc");
        }
        printf("[client] completed %d ping-pong iterations\\n", ITERS);
    } else {
        for (i=0;i<ITERS;i++){
            while (ibv_poll_cq(cq, 1, &wc) <= 0) {}
            if (wc.status != IBV_WC_SUCCESS) die("recv wc");
            if (ibv_post_send(id->qp, &swr, &sbad)) die("ibv_post_send");
            while (ibv_poll_cq(cq, 1, &wc) <= 0) {}
            if (wc.status != IBV_WC_SUCCESS) die("send wc");
            if (ibv_post_recv(id->qp, &rwr, &rbad)) die("ibv_post_recv");
        }
        printf("[server] completed %d ping-pong iterations\\n", ITERS);
    }

    rdma_disconnect(id);
    ibv_dereg_mr(send_mr); ibv_dereg_mr(recv_mr);
    free(send_buf); free(recv_buf);
    rdma_destroy_qp(id);
    ibv_destroy_cq(cq);
    ibv_dealloc_pd(pd);
    rdma_destroy_id(id);
    if (listen_id) rdma_destroy_id(listen_id);
    rdma_destroy_event_channel(ec);
    return 0;
}
