#include <linux/module.h>
//#include <rdma/ib_verbs.h>
//#include <rdma/ib_cm.h>
#include <linux/inet.h>

#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>

#if defined(SERVER)
#  define IP      "192.168.1.34"
#  define PORT    4444
#elif defined(CLIENT)
#  define IP      "192.168.1.33"
#  define PORT    4444
#else
#error SERVER or CLIENT must be defined!
#endif


static int debug = 1;
module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "Debug level (0=none, 1=all)");
#define DRV KBUILD_MODNAME ": "
#define DEBUG_LOG(...) if (debug) printk(DRV  __VA_ARGS__)
#define ERROR_LOG(...) printk( KERN_ERR DRV  __VA_ARGS__)
//------------------------------------------------------

// struct ib_client ib_cache;
// struct ibv_context  *context;

// #define MAX_DEV 2
// struct ib_device *m_devs[MAX_DEV];
// int devices;


//------------------------------------------------------


enum test_state {
    IDLE = 1,
    CONNECT_REQUEST,
    ADDR_RESOLVED,
    ROUTE_RESOLVED,
    CONNECTED,
    RDMA_READ_ADV,
    RDMA_READ_COMPLETE,
    RDMA_WRITE_ADV,
    RDMA_WRITE_COMPLETE,
    ERROR
};


struct krping_rdma_info {
    uint64_t buf;
    uint32_t rkey;
    uint32_t size;
};


struct cache_cb {
    uint16_t port;
    char addr[4];

	struct ib_cq *cq;
	struct ib_pd *pd;
	struct ib_qp *qp;

    enum test_state state;           // used for cond/signalling
    wait_queue_head_t sem;

    struct rdma_cm_id *cm_id;       // connection on client side?
    struct rdma_cm_id *child_cm_id; // connection on server side?
};

static int krping_cma_event_handler( struct rdma_cm_id *cma_id,
				   struct rdma_cm_event *event )
{
            int         ret = 0;
    struct  cache_cb    *cb = cma_id->context;

    DEBUG_LOG( "cma_event type %d cma_id %p (%s)\n", event->event, cma_id,
        ( cma_id == cb->cm_id ) ? "parent" : "child" );

    switch( event->event ) {
    case RDMA_CM_EVENT_ADDR_RESOLVED:
        cb->state = ADDR_RESOLVED;
        ret = rdma_resolve_route( cma_id, 2000 );
        if (ret) {
            ERROR_LOG( "rdma_resolve_route error %d\n", ret );
            wake_up_interruptible( &cb->sem );
        }
        break;

    case RDMA_CM_EVENT_ROUTE_RESOLVED:
        cb->state = ROUTE_RESOLVED;
        wake_up_interruptible( &cb->sem );
        break;

//*********
    //case ???:
//*********

    default:
        ERROR_LOG( "oof bad type!\n" );
        wake_up_interruptible( &cb->sem );
        break;
    }

    DEBUG_LOG( "krping_cma_event_handler exit\n");
    return 0;
}



static void krping_cq_event_handler(struct ib_cq *cq, void *ctx)
{
    struct cache_cb *cb = ctx;
    struct ib_wc wc;
//     struct ib_recv_wr *bad_wr;
    int ret;

    BUG_ON(cb->cq != cq);
    if (cb->state == ERROR) {
        ERROR_LOG( "cq completion in ERROR state\n");
        return;
    }

//     if (!cb->wlat && !cb->rlat && !cb->bw)
        ib_req_notify_cq(cb->cq, IB_CQ_NEXT_COMP);
    while ((ret = ib_poll_cq(cb->cq, 1, &wc)) == 1) {
        if (wc.status) {
            if (wc.status == IB_WC_WR_FLUSH_ERR) {
                DEBUG_LOG("cq flushed\n");
                continue;
            } else {
                ERROR_LOG( "cq completion failed with "
                    "wr_id %Lx status %d opcode %d vender_err %x\n",
                    wc.wr_id, wc.status, wc.opcode, wc.vendor_err);
                goto error;
            }
        }

        switch (wc.opcode) {
        case IB_WC_SEND:
            DEBUG_LOG("send completion\n");
//             cb->stats.send_bytes += cb->send_sgl.length;
//             cb->stats.send_msgs++;
            break;

        case IB_WC_RDMA_WRITE:
            DEBUG_LOG("rdma write completion\n");
//             cb->stats.write_bytes += cb->rdma_sq_wr.wr.sg_list->length;
//             cb->stats.write_msgs++;
            cb->state = RDMA_WRITE_COMPLETE;
            wake_up_interruptible(&cb->sem);
            break;

        case IB_WC_RDMA_READ:
            DEBUG_LOG("rdma read completion\n");
//             cb->stats.read_bytes += cb->rdma_sq_wr.wr.sg_list->length;
//             cb->stats.read_msgs++;
            cb->state = RDMA_READ_COMPLETE;
            wake_up_interruptible(&cb->sem);
            break;

        case IB_WC_RECV:
            DEBUG_LOG("recv completion\n");
//             cb->stats.recv_bytes += sizeof(cb->recv_buf);
//             cb->stats.recv_msgs++;
#if 0
            if (cb->wlat || cb->rlat || cb->bw)
                ret = server_recv(cb, &wc);
            else
                ret = cb->server ? server_recv(cb, &wc) :
                        client_recv(cb, &wc);
            if (ret) {
                ERROR_LOG( "recv wc error: %d\n", ret);
                goto error;
            }

            ret = ib_post_recv(cb->qp, &cb->rq_wr, &bad_wr);
            if (ret) {
                ERROR_LOG( "post recv error: %d\n", 
                    ret);
                goto error;
            }
#endif
            wake_up_interruptible(&cb->sem);
            break;

        default:
            ERROR_LOG(
                "%s:%d Unexpected opcode %d, Shutting down\n",
                __func__, __LINE__, wc.opcode);
            goto error;
        }
    }
    if (ret) {
        ERROR_LOG( "poll error %d\n", ret);
        goto error;
    }
    return;
error:
    cb->state = ERROR;
    wake_up_interruptible(&cb->sem);
}


static int krping_create_qp(struct cache_cb *cb)
{
    struct ib_qp_init_attr init_attr;
    int ret;

    memset(&init_attr, 0, sizeof(init_attr));
    init_attr.cap.max_send_wr = 64/*cb->txdepth*/;
    init_attr.cap.max_recv_wr = 2;
    
    /* For flush_qp() */
    init_attr.cap.max_send_wr++;
    init_attr.cap.max_recv_wr++;

    init_attr.cap.max_recv_sge = 1;
    init_attr.cap.max_send_sge = 1;
    init_attr.qp_type = IB_QPT_RC;
    init_attr.send_cq = cb->cq;
    init_attr.recv_cq = cb->cq;
    init_attr.sq_sig_type = IB_SIGNAL_REQ_WR;

#if defined(SERVER)//    if (cb->server)
    {
        ret = rdma_create_qp(cb->child_cm_id, cb->pd, &init_attr);
        if (!ret)
            cb->qp = cb->child_cm_id->qp;
    } 
#elif defined(CLIENT)//    else
    {
        ret = rdma_create_qp(cb->cm_id, cb->pd, &init_attr);
        if (!ret)
            cb->qp = cb->cm_id->qp;
    }
#else
#error SERVER or CLIENT must be defined!
#endif

    return ret;
}

static void krping_free_qp(struct cache_cb *cb)
{
    ib_destroy_qp(cb->qp);
    ib_destroy_cq(cb->cq);
    ib_dealloc_pd(cb->pd);
}

static int krping_setup_qp(struct cache_cb *cb, struct rdma_cm_id *cm_id)
{
    int ret;
    struct ib_cq_init_attr attr = {0};

    cb->pd = ib_alloc_pd(cm_id->device, 0);
    if (IS_ERR(cb->pd)) {
        ERROR_LOG( "ib_alloc_pd failed\n");
        return PTR_ERR(cb->pd);
    }
    DEBUG_LOG("created pd %p\n", cb->pd);

    attr.cqe = 64/*cb->txdepth*/ * 2;
    attr.comp_vector = 0;
    cb->cq = ib_create_cq(cm_id->device, krping_cq_event_handler, NULL,
                cb, &attr);
    if (IS_ERR(cb->cq)) {
        ERROR_LOG( "ib_create_cq failed\n");
        ret = PTR_ERR(cb->cq);
        goto err1;
    }
    DEBUG_LOG("created cq %p\n", cb->cq);

//     if (!cb->wlat && !cb->rlat && !cb->bw && !cb->frtest)
    {
        ret = ib_req_notify_cq(cb->cq, IB_CQ_NEXT_COMP);
        if (ret) {
            ERROR_LOG( "ib_create_cq failed\n");
            goto err2;
        }
    }

    ret = krping_create_qp(cb);
    if (ret) {
        ERROR_LOG( "krping_create_qp failed: %d\n", ret);
        goto err2;
    }
    DEBUG_LOG("created qp %p\n", cb->qp);

    return 0;

err2:
    ib_destroy_cq(cb->cq);

err1:
    ib_dealloc_pd(cb->pd);
    return ret;
}


static void fill_sockaddr(struct sockaddr_storage *sin, struct cache_cb *cb)
{
    struct sockaddr_in *sin4 = (struct sockaddr_in *)sin;

    memset(sin, 0, sizeof(*sin));

    in4_pton( IP, -1, cb->addr, -1, NULL );
    cb->port = htons( PORT );

    sin4->sin_family = AF_INET;
    memcpy((void *)&sin4->sin_addr.s_addr, cb->addr, 4);
    sin4->sin_port = cb->port;
}


static int krping_bind_server(struct cache_cb *cb)
{
    struct  sockaddr_storage sin;
            int              ret     = 0;

    fill_sockaddr(&sin, cb);
    
    ret = rdma_bind_addr(cb->cm_id, (struct sockaddr *)&sin);
    if( ret )
    {
        ERROR_LOG( "rdma_bind_addr error %d\n", ret );
        return ret;
    }
    DEBUG_LOG("rdma_bind_addr successful\n");

    DEBUG_LOG("rdma_listen\n");
    ret = rdma_listen( cb->cm_id, 3 );
    if( ret )
    {
        ERROR_LOG( "rdma_listen failed: %d\n", ret );
        return ret;
    }
    DEBUG_LOG("rdma_listen successful\n");

    wait_event_interruptible(cb->sem, cb->state >= CONNECT_REQUEST);
    DEBUG_LOG("We have event!\n");
    if( cb->state != CONNECT_REQUEST )
    {
        ERROR_LOG( "wait for CONNECT_REQUEST state %d\n", cb->state );
        return -1;
    }

//     if( !reg_supported( cb->child_cm_id->device ) )
//         return -EINVAL;

    return 0;
}




static void krping_run_server(struct cache_cb *cb)
{
    int ret;

    ret = krping_bind_server(cb);
    if (ret)
    {
        ERROR_LOG( "ERROR BIND SERVER\n" );
        return;
    }
}


static int krping_bind_client(struct cache_cb *cb)
{
    struct  sockaddr_storage sin;
            int              ret     = 0;

    fill_sockaddr(&sin, cb);


    ret = rdma_resolve_addr(cb->cm_id, NULL, (struct sockaddr *)&sin, 2000);
    if (ret) {
        ERROR_LOG( "rdma_resolve_addr error %d\n", ret);
        return ret;
    }

    wait_event_interruptible(cb->sem, cb->state >= ROUTE_RESOLVED);
    if (cb->state != ROUTE_RESOLVED) {
        ERROR_LOG( "addr/route resolution did not resolve: state %d\n", cb->state );
        return -EINTR;
    }

//     if (!reg_supported(cb->cm_id->device))
//         return -EINVAL;

    DEBUG_LOG("rdma_resolve_addr - rdma_resolve_route successful\n");
    return 0;

}


static void krping_run_client(struct cache_cb *cb)
{
//    struct ib_recv_wr *bad_wr;
    int ret;

    /* set type of service, if any
    if (cb->tos != 0)
        rdma_set_service_type(cb->cm_id, cb->tos);
    */

    ret = krping_bind_client(cb);
    if (ret)
    {
        ERROR_LOG( "ERROR BIND SERVER\n" );
        return;
    }
    ret = krping_setup_qp(cb, cb->cm_id);
    if (ret)
    {
        ERROR_LOG( "setup_qp failed: %d\n", ret );
        return;
    }
    goto err1;

err1:
    krping_free_qp(cb);
}

static int __init client_module_init(void)
{
    struct  cache_cb    *cb;
            int         ret             = 0;
// Not need - we use CM
//                         devices         = 0;
//                         ib_cache.name   = "DISAG_MEM";
//                         ib_cache.add    = add_device;
//                         ib_cache.remove = remove_device;
//     ret = ib_register_client( &ib_cache );
//     if (ret)
//     {
//         ERROR_LOG( "Failed to register IB client\n" );
//         return ret;
//     }


    cb = kzalloc(sizeof(*cb), GFP_KERNEL);
    if (!cb)
        return -ENOMEM;
    init_waitqueue_head(&cb->sem);
    cb->cm_id = rdma_create_id(&init_net, krping_cma_event_handler, cb, RDMA_PS_TCP, IB_QPT_RC);
    if ( IS_ERR( cb->cm_id ) ) {
        ret = PTR_ERR( cb->cm_id );
        ERROR_LOG( "rdma_create_id error %d\n", ret );
        goto out;
    }
    else
        DEBUG_LOG("created cm_id %p\n", cb->cm_id );
#if defined(SERVER)
    //if (cb->server)
        krping_run_server(cb);
#elif defined(CLIENT)
    //else
        krping_run_client(cb);
#else
#error SERVER or CLIENT must be defined!
#endif


    DEBUG_LOG("RDMA destroy ID\n");
    rdma_destroy_id(cb->cm_id);
    DEBUG_LOG("RDMA ID destroed!\n");

out:
    return ret;
}


// void add_device( struct ib_device* dev )
// {
//     int i;
//     m_devs[ devices++ ] = dev;
//     if( devices > MAX_DEV )
//         --devices;
//     for( i = 0; i < devices; ++i )
//         printk( DRV "We got a new device! The devece name is: %s\n", m_devs[i]->name );
// 
// }


// void remove_device( struct ib_device* dev, void *ctx )
// {
//     printk( DRV "remove_device\n" );
// }

static void __exit client_module_exit( void )
{
//    sock_release(sock);
//    ib_unregister_event_handler(&ieh);
//    ib_unregister_client( &ib_cache );
    DEBUG_LOG( "Exit client module.\n" );
}

module_init( client_module_init );
module_exit( client_module_exit );
MODULE_LICENSE( "GPL" );
