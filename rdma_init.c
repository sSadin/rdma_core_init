#include <linux/module.h>
//#include <rdma/ib_verbs.h>
#include <rdma/ib_cm.h>
//#include </usr/src/mlnx-ofed-kernel-4.3/include/rdma/ib_verbs.h>

#define DRV KBUILD_MODNAME ": "

struct ib_client nvdimm_client;


void add_device(struct ib_device* dev)
{
    printk(DRV "We got a new device!\n ");
}


void remove_device(struct ib_device* dev, void *ctx)
{
    printk(DRV "remove_device\n ");
}


static int __init client_module_init(void)
{
    nvdimm_client.name = "DISAG_MEM";
    nvdimm_client.add = add_device;
    nvdimm_client.remove = remove_device;

    ib_register_client(&nvdimm_client);

//    while(1);
    return 0;
}

static void __exit client_module_exit( void )
{
//    sock_release(sock);
//    ib_unregister_event_handler(&ieh);
    ib_unregister_client(&nvdimm_client);
    printk(DRV "Exit client module.\n");
}

module_init( client_module_init );
module_exit( client_module_exit );
MODULE_LICENSE("GPL");
