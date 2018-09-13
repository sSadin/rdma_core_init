#include <linux/module.h>
//#include <rdma/ib_verbs.h>
#include <rdma/ib_cm.h>
//#include </usr/src/mlnx-ofed-kernel-4.3/include/rdma/ib_verbs.h>

#define DRV KBUILD_MODNAME ": "

struct ib_client ib_cache;

struct ibv_context  *context;

#define MAX_DEV 2
/*???*/struct ib_device *m_devs[MAX_DEV];
int devices;

void add_device( struct ib_device* dev )
{
    int i;
    m_devs[ devices++ ] = dev;
    if( devices > MAX_DEV )
        --devices;
    for( i = 0; i < devices; ++i )
        printk( DRV "We got a new device! The devece name is: %s\n", m_devs[i]->name );

    context = ibv_open_device(dev);
    if( context == 0 )
    {
        printk( DRV "Can't open device: \n", dev->name );
    }
}


void remove_device( struct ib_device* dev, void *ctx )
{
    printk( DRV "remove_device\n" );
}


static int __init client_module_init( void )
{
    int ret;
    devices = 0;
    ib_cache.name      = "DISAG_MEM";
    ib_cache.add       = add_device;
    ib_cache.remove    = remove_device;

    ret = ib_register_client( &ib_cache );
    if (ret)
    {
        printk( KERN_ERR "Failed to register IB client\n" );
        return ret;
    }

//    while(1);
    return 0;
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
