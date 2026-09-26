static void reset(void) {
    memset(&conn,0,sizeof(conn)); memset(&ctrl,0,sizeof(ctrl)); memset(&sig,0,sizeof(sig));
    errors=connected=bounds=disconnected=responses=next_out=0; last_error[0]=0;
    ctrl.rx.cid=0x40; ctrl.tx.cid=0x41; ctrl.rx.mtu=ctrl.tx.mtu=672;
    ctrl.chan.conn=sig.chan.chan.conn=&conn; ctrl.chan.ops=&chan_ops;
    ctrl.chan.state=BT_L2CAP_CONFIG; ctrl.chan.rtx_work=1;
}
static void receive(uint8_t *bytes,size_t len) {
    struct net_buf b={bytes,len};
    if(!setjmp(bailout)) l2cap_br_recv(&sig.chan.chan,&b);
}
static void result(const char *name) {
    printf("%s: connected=%d local_conf=%d remote_conf=%d errors=%d bounds=%d timer=%d last_error=%s\n",
        name,connected,atomic_test_bit(ctrl.flags,0),atomic_test_bit(ctrl.flags,1),
        errors,bounds,ctrl.chan.rtx_work,last_error);
}
static void expect_ok(void) { assert(connected==1 && !errors && !bounds && !ctrl.chan.rtx_work); }
static void pair(const uint8_t *first,size_t a,const uint8_t *second,size_t b) {
    uint8_t packet[96]; memcpy(packet,first,a); memcpy(packet+a,second,b); receive(packet,a+b);
}
int main(void) {
    uint8_t req[]={4,0x21,4,0,0x40,0,0,0};
    uint8_t rsp[]={5,0x22,6,0,0x40,0,0,0,0,0};
    uint8_t mtu[]={4,0x21,8,0,0x40,0,0,0,1,2,0xa0,2};
    reset(); receive(req,sizeof(req)); receive(rsp,sizeof(rsp)); expect_ok();
    reset(); pair(req,sizeof(req),rsp,sizeof(rsp)); expect_ok();
    reset(); pair(rsp,sizeof(rsp),req,sizeof(req)); expect_ok();
    reset(); pair(mtu,sizeof(mtu),rsp,sizeof(rsp)); expect_ok();
    reset(); receive(mtu,sizeof(mtu)); receive(rsp,sizeof(rsp)); expect_ok();
    uint8_t info_then_pair[]={11,0x31,8,0,2,0,0,0,0,0,0,0,
                            4,0x21,4,0,0x40,0,0,0,5,0x22,6,0,0x40,0,0,0,0,0};
    reset(); sig.info_ident=0x31; receive(info_then_pair,sizeof(info_then_pair)); expect_ok();
    /* A handler must not borrow bytes from the following command. */
    uint8_t short_info[]={11,0x31,4,0,2,0,0,0};
    reset(); sig.info_ident=0x31; pair(short_info,sizeof(short_info),req,sizeof(req));
    assert(errors==1 && !bounds && !connected && atomic_test_bit(ctrl.flags,1));
    uint8_t short_fixed[]={11,0x31,5,0,3,0,0,0,0};
    reset(); sig.info_ident=0x31; receive(short_fixed,sizeof(short_fixed)); assert(errors==1 && !bounds);
    uint8_t short_mtu[]={4,0x21,7,0,0x40,0,0,0,1,2,0xa0};
    reset(); pair(short_mtu,sizeof(short_mtu),rsp,sizeof(rsp)); assert(errors && !bounds && !connected);
    uint8_t oversized[]={4,0x21,20,0,0x40,0,0,0};
    reset(); receive(oversized,sizeof(oversized)); assert(errors==1 && !bounds && !connected);
    uint8_t zero_id[]={4,0,4,0,0x40,0,0,0};
    reset(); receive(zero_id,sizeof(zero_id)); assert(errors==1 && !bounds && !connected);
    uint8_t tail[]={4,0x21,4,0,0x40,0,0,0,0xff};
    reset(); receive(tail,sizeof(tail)); assert(errors==1 && !bounds && !connected);
    reset(); ctrl.chan.ident=0x21; atomic_set_bit(ctrl.flags,L2CAP_FLAG_CONN_ACCEPTOR);
    l2cap_br_conn_req_reply(&ctrl.chan,BT_L2CAP_BR_PENDING);
    assert(ctrl.chan.ident==0x21);
    l2cap_br_conn_req_reply(&ctrl.chan,0);
    assert(reply_ident[0]==0x21 && reply_ident[1]==0x21 && ctrl.chan.ident==0);
    uint8_t send_bytes[]={4,0x55,4,0,0x40,0,0,0};
    struct net_buf send_buf={send_bytes,sizeof(send_bytes)};
    send_error=-EINVAL; trace_tx_errors=trace_tx_submitted=0;
    l2cap_br_send_sig(&conn,&send_buf);
    assert(trace_tx_submitted==1 && trace_tx_errors==1);
    assert((int16_t)trace_error_detail==-EINVAL);
    assert(trace_error_command[0]==4 && trace_error_command[1]==0x55);
    puts("PASS: 14 L2CAP packet/transaction/trace regression cases (real SDK function bodies)");
    return 0;
}
