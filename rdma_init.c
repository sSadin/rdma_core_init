#include <linux/module.h>
//#include <rdma/ib_verbs.h>
//#include <rdma/ib_cm.h>
#include <linux/inet.h>

#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>


#define PORT    4444
#define IP      "192.168.1.33"


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

    enum test_state state;           // used for cond/signalling
    wait_queue_head_t sem;

    struct rdma_cm_id *cm_id;       // connection on client side?
    struct rdma_cm_id *child_cm_id; // connection on server side?
};

static int krping_cma_event_handler(struct rdma_cm_id *cma_id,
				   struct rdma_cm_event *event)
{
DEBUG_LOG( "krping_cma_event_handler\n");
return 0;
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


static int krping_bind_server(struct cache_cb *cb)
{
    struct  sockaddr_storage sin;
    struct  sockaddr_in      *sin4   = (struct sockaddr_in *)&sin;
            int              ret     = 0;

    in4_pton( IP, -1, cb->addr, -1, NULL );
    cb->port = htons( PORT );

    sin4->sin_family = AF_INET;
    memcpy((void *)&sin4->sin_addr.s_addr, cb->addr, 4);
    sin4->sin_port = cb->port;

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
       return;
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
    //if (cb->server)
        krping_run_server(cb);
    //else
    //    krping_run_client(cb);


    DEBUG_LOG("RDMA destroy ID\n");
    rdma_destroy_id(cb->cm_id);
    DEBUG_LOG("RDMA ID destroed!\n");

out:
    return ret;
}

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
