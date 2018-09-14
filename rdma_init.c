#include <linux/module.h>
//#include <rdma/ib_verbs.h>
//#include <rdma/ib_cm.h>
#include <linux/inet.h>

#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>

static int debug = 1;
module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "Debug level (0=none, 1=all)");
#define DEBUG_LOG if (debug) printk
#define DRV KBUILD_MODNAME ": "

struct ib_client ib_cache;

struct ibv_context  *context;

//------------------------------------------------------


struct ib_client nvdimm_client;
#define MAX_DEV 2
/*???*/struct ib_device *m_devs[MAX_DEV];
int devices;


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

    enum test_state state;           // used for cond/signalling
    wait_queue_head_t sem;

    struct rdma_cm_id *cm_id;       // connection on client side?
    struct rdma_cm_id *child_cm_id; // connection on server side?
};

static int krping_cma_event_handler(struct rdma_cm_id *cma_id,
				   struct rdma_cm_event *event)
{
printk(DRV "krping_cma_event_handler\n");
return 0;
}


void add_device( struct ib_device* dev )
{
    int i;
    m_devs[ devices++ ] = dev;
    if( devices > MAX_DEV )
        --devices;
    for( i = 0; i < devices; ++i )
        printk( DRV "We got a new device! The devece name is: %s\n", m_devs[i]->name );

}


void remove_device( struct ib_device* dev, void *ctx )
{
    printk( DRV "remove_device\n" );
}


static int krping_bind_server(struct cache_cb *cb)
{
    struct  sockaddr_storage sin;
    struct  sockaddr_in      *sin4   = (struct sockaddr_in *)&sin;
            int              ret     = 0;
            char             *ip_str = "192.168.1.34";
            uint16_t         port    = 4444;

    in4_pton( ip_str, -1, cb->addr, -1, NULL );
    cb->port = htons( port );

    sin4->sin_family = AF_INET;
    memcpy((void *)&sin4->sin_addr.s_addr, cb->addr, 4);
    sin4->sin_port = cb->port;

    ret = rdma_bind_addr(cb->cm_id, (struct sockaddr *)&sin);
    if( ret )
    {
        printk( KERN_ERR DRV "rdma_bind_addr error %d\n", ret );
        return ret;
    }
    DEBUG_LOG("rdma_bind_addr successful\n");

    DEBUG_LOG("rdma_listen\n");
    ret = rdma_listen( cb->cm_id, 3 );
    if( ret )
    {
        printk( KERN_ERR DRV "rdma_listen failed: %d\n", ret );
        return ret;
    }
    DEBUG_LOG("rdma_listen successful\n");

    wait_event_interruptible(cb->sem, cb->state >= CONNECT_REQUEST);
//     if( cb->state != CONNECT_REQUEST )
//     {
//         printk( KERN_ERR DRV "wait for CONNECT_REQUEST state %d\n", cb->state );
//         return -1;
//     }
//
//     if( !reg_supported( cb->child_cm_id->device ) )
//         return -EINVAL;

    return 0;
}




static void krping_run_server(struct cache_cb *cb)
{
    int ret;

    init_waitqueue_head(&cb->sem);
    ret = krping_bind_server(cb);
    if (ret)
       return;
}


static int __init client_module_init(void)
{
    struct  cache_cb    *cb;
            int         ret             = 0;
                        devices         = 0;
                        ib_cache.name   = "DISAG_MEM";
                        ib_cache.add    = add_device;
                        ib_cache.remove = remove_device;
//Not need - we use CM
//     ret = ib_register_client( &ib_cache );
//     if (ret)
//     {
//         printk( KERN_ERR "Failed to register IB client\n" );
//         return ret;
//     }


    cb = kzalloc(sizeof(*cb), GFP_KERNEL);
    if (!cb)
        return -ENOMEM;
    cb->cm_id = rdma_create_id(&init_net, krping_cma_event_handler, cb, RDMA_PS_TCP, IB_QPT_RC);
    if ( IS_ERR( cb->cm_id ) ) {
        ret = PTR_ERR( cb->cm_id );
        printk( KERN_ERR DRV "rdma_create_id error %d\n", ret );
        goto out;
    }
    else
        printk( DRV "created cm_id %p\n", cb->cm_id );
    //if (cb->server)
        krping_run_server(cb);
    //else
    //    krping_run_client(cb);




//    while(1);
out:
    return ret;
}

static void __exit client_module_exit( void )
{
//    sock_release(sock);
//    ib_unregister_event_handler(&ieh);
    ib_unregister_client( &ib_cache );
    printk( DRV "Exit client module.\n" );
}

module_init( client_module_init );
module_exit( client_module_exit );
MODULE_LICENSE( "GPL" );
