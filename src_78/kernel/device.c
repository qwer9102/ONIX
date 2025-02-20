#include <onix/device.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/string.h>
#include <onix/task.h>
#include <onix/arena.h>

#define DEVICE_NR 64  //设备数量

static device_t devices[DEVICE_NR];   //设备数组

static device_t* get_null_device()
{
    for(size_t i = 0; i < DEVICE_NR; i++)
    {
        device_t *device = &devices[i];
        if(device->type == DEV_NULL)
        {
            return device;
        }
    }
    panic("no more devices !!!");
}

// 安装设备,并返回设备号device->dev
dev_t device_install(
    int type, int subtype,
    void *ptr, char *name, dev_t parent,
    void *ioctl, void *read, void *write)
{
    device_t *device = get_null_device();
    device->ptr = ptr;
    device->parent = parent;
    device->type = type;
    device->subtype = subtype;
    strncpy(device->name, name, NAMELEN);
    device->ioctl = ioctl;
    device->read = read;
    device->write = write;
    return device->dev;
}

// 根据子类型查找设备
device_t *device_find(int subtype, idx_t idx)
{
    idx_t nr = 0;
    for(size_t i = 0; i < DEVICE_NR; i++)
    {
        device_t *device = &devices[i];
        if(device->subtype != subtype)
            continue;
        // device->subtype == subtype
        if(nr == idx) 
            return device;
        nr++;
    }
    return NULL;
}

// 根据设备号查找设备
device_t *device_get(dev_t dev)
{
    assert(dev < DEVICE_NR);
    device_t *device = &devices[dev];
    assert(device->type != DEV_NULL);
    return device;
}

// 控制设备
int device_ioctl(dev_t dev, int cmd, void *args, int flags)
{
    device_t *device = device_get(dev);
    if(device->ioctl)
    {
        return device->ioctl(device->ptr,cmd,args,flags);
    }
    LOGK("ioctl of device %d not implemented !!!",dev);
    return EOF;
}

// 读设备
int device_read(dev_t dev, void *buf, size_t count, idx_t idx, int flags)
{
    device_t *device = device_get(dev);
    if(device->read != NULL)
    {
        return device->read(device->ptr,buf,count,idx,flags);
    }
    LOGK("read of device %d not implemented !!!",dev);
    return EOF;
}

// 写设备
int device_write(dev_t dev, void *buf, size_t count, idx_t idx, int flags)
{
    device_t *device = device_get(dev);
    if(device->write != NULL)
    {
        return device->write(device->ptr,buf,count,idx,flags);
    }
    LOGK("write of device %d not implemented !!!",dev);
    return EOF;
}


void device_init()
{
    for(size_t i = 0; i < DEVICE_NR; i++)
    {
        device_t *device = &devices[i];
        strcpy((char *)device->name,"null");
        device->type = DEV_NULL;
        device->subtype = DEV_NULL;
        device->dev = i;
        device->parent = 0;
        device->ioctl = NULL;
        device->read  = NULL;
        device->write = NULL;
    }
}