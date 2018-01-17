#include "sisdevicemgr.h"

/*=============================================================================*/
/* hid-example.c */
/*
* Hidraw Userspace Example
*
* Copyright (c) 2010 Alan Ott <alan@signal11.us>
* Copyright (c) 2010 Signal 11 Software
*
* The code may be used by anyone for any purpose,
* and can serve as a starting point for developing
* applications using hidraw.
*/
/* Linux */
#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>

/*
 * Ugly hack to work around failing compilation on systems that don't
 * yet populate new version of hidraw.h to userspace.
 *
 * If you need this, please have your distro update the kernel headers.
 */
#ifndef HIDIOCSFEATURE
#define HIDIOCSFEATURE(len)    _IOC(_IOC_WRITE|_IOC_READ, 'H', 0x06, len)
#define HIDIOCGFEATURE(len)    _IOC(_IOC_WRITE|_IOC_READ, 'H', 0x07, len)
#endif

/* Unix */
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
/*=============================================================================*/

#include "sislog.h"
#include "shellcommand/shellcommand.h"

using namespace SiS;

#define TAG "SiSDeviceMgr"

SiSDeviceMgr::SiSDeviceMgr() :
    m_openedIndex(0) // default open index 0
{

}

SiSDeviceMgr::~SiSDeviceMgr()
{
    clearDeviceAttributeList();
}

void 
SiSDeviceMgr::clearDeviceAttributeList()
{
    /* delete m_sisDeviceAttributeList */
    for (SiSDeviceAttributeList::iterator it = m_sisDeviceAttributeList.begin(); it != m_sisDeviceAttributeList.end();)  
    {  
        //printf("delete m_sisDeviceAttributeList\n");
        m_sisDeviceAttributeList.erase(it++);  
    }
}

SiSDeviceAttribute*
SiSDeviceMgr::getOpened()
{
    return getElement(m_sisDeviceAttributeList, m_openedIndex);
}

SiSDeviceAttribute*
SiSDeviceMgr::getElement(SiSDeviceAttributeList _list, int _i)
{
    int maxIndex = _list.size() - 1;
    if( _i > maxIndex )
    {
        throw SiSDeviceException("out of range of SiSDeviceAttributeList", -1);
    }

    SiSDeviceAttributeList::iterator it = _list.begin();
    for(int i=0; i<_i; i++)
    {
        ++it;
    }
    return *it;
}

bool
SiSDeviceMgr::detectByHidrawName(std::string hidrawName)
{
    SIS_LOG_I(SiSLog::getOwnerSiS(), TAG, "detectByHidrawName : %s", hidrawName.c_str());

    SiSDeviceAttribute* newSiSDeviceAttribute = new SiSDeviceAttribute();

    if( hidrawName.find("/dev/hidraw") == 0 && getHidInfo( hidrawName.c_str(), newSiSDeviceAttribute) )
    {
        if( newSiSDeviceAttribute->getVID() != 0x0457 )
        {
            delete newSiSDeviceAttribute;
            SIS_LOG_E(SiSLog::getOwnerSiS(), TAG, "Not sis touch (VID is not 0457) \n");
            return false;
        }

        /* find hidrawNum */
        std::string hidrawNum = hidrawName;
        hidrawNum.replace(0, 5, ""); 
        //SIS_LOG_D(SiSLog::getOwnerSiS(), TAG, "hidrawNum : %s", hidrawNum.c_str() );

        /* find ls_sys_class_hidraw_hidrawN */
        std::string cmd_ls_sys_class_hidraw_hidrawN = "ls -l /sys/class/hidraw/"; // " ls -l /sys/class/hidraw/hidraw* "
        cmd_ls_sys_class_hidraw_hidrawN.append(hidrawNum);
        SIS_LOG_D(SiSLog::getOwnerSiS(), TAG, "cmd : %s", cmd_ls_sys_class_hidraw_hidrawN.c_str() );
        std::string ls_sys_class_hidraw_hidrawN = ShellCommand::exec( cmd_ls_sys_class_hidraw_hidrawN.c_str() );
        SIS_LOG_D(SiSLog::getOwnerSiS(), TAG, "output : %s", ls_sys_class_hidraw_hidrawN.c_str() );

        /* judge what interface */
        if( ls_sys_class_hidraw_hidrawN.find("/usb") != std::string::npos )
        {
        	newSiSDeviceAttribute->setConnectType( SiSDeviceAttribute::CON_819_USB_HID	);
        }
        else if( ls_sys_class_hidraw_hidrawN.find("/i2c") != std::string::npos	)
        {
        	newSiSDeviceAttribute->setConnectType( SiSDeviceAttribute::CON_819_HID_OVER_I2C  );
        }
        else if( ls_sys_class_hidraw_hidrawN.find("usb") != std::string::npos )
        {
        	newSiSDeviceAttribute->setConnectType( SiSDeviceAttribute::CON_819_USB_HID	);
        }
        else if( ls_sys_class_hidraw_hidrawN.find("i2c") != std::string::npos	)
        {
        	newSiSDeviceAttribute->setConnectType( SiSDeviceAttribute::CON_819_HID_OVER_I2C  );
        }
        else // default : force to set CON_819_HID_OVER_I2C
        {
            SIS_LOG_W(SiSLog::getOwnerSiS(), TAG, "! Default : CON_819_HID_OVER_I2C" );
        	newSiSDeviceAttribute->setConnectType( SiSDeviceAttribute::CON_819_HID_OVER_I2C  );
        }

        m_sisDeviceAttributeList.push_back(newSiSDeviceAttribute);
        SIS_LOG_I(SiSLog::getOwnerSiS(), TAG, "Available sis touch\n");
        return true;
    }
    else
    {
        delete newSiSDeviceAttribute;
        SIS_LOG_I(SiSLog::getOwnerSiS(), TAG, "Not (available) sis touch\n");
        return false;
    }
}

bool
SiSDeviceMgr::getHidInfo(const char* deviceName, SiSDeviceAttribute* sisDeviceAttribute)
{
    int fd;
    int i, res, desc_size = 0;
    char buf[256];
    struct hidraw_report_descriptor rpt_desc;
    struct hidraw_devinfo info;

    SIS_LOG_D(SiSLog::getOwnerSiS(), TAG, "getHidInfo: %s", deviceName);

    /* Open the Device with non-blocking reads. In real life,
        don't use a hard coded path; use libudev instead. */
    fd = open(deviceName, O_RDWR|O_NONBLOCK);

    if (fd < 0) 
    {
        SIS_LOG_E(SiSLog::getOwnerSiS(), TAG, "Unable to open device, errno=%d (%s)", errno, strerror(errno));
        //perror("Unable to open device");
        return false;
    }
    else
    {
        sisDeviceAttribute->setNodeName(deviceName);
    }

    memset(&rpt_desc, 0x0, sizeof(rpt_desc));
    memset(&info, 0x0, sizeof(info));
    memset(buf, 0x0, sizeof(buf));

    /* Get Report Descriptor Size */
    /*
    res = ioctl(fd, HIDIOCGRDESCSIZE, &desc_size);
    if (res < 0)
    {
        perror("HIDIOCGRDESCSIZE");
    }
    else
    {
        printf("Report Descriptor Size: %d\n", desc_size);
    }
    */

    /* Get Report Descriptor */
    /*
    rpt_desc.size = desc_size;
    res = ioctl(fd, HIDIOCGRDESC, &rpt_desc);
    if (res < 0)
    {
        SIS_LOG_E(SiSLog::getOwnerSiS(), TAG, "HIDIOCGRDESC, errno=%d", deviceName, errno);
        perror("HIDIOCGRDESC");
    }
    else
    {
        SIS_LOG_I(SiSLog::getOwnerSiS(), TAG, "Report Descriptor:");
        for (i = 0; i < rpt_desc.size; i++)
        {
            printf("%hhx ", rpt_desc.value[i]);
        }
        SIS_LOG_I(SiSLog::getOwnerSiS(), TAG, "");
    }
    */

    /* Get Raw Name */
    res = ioctl(fd, HIDIOCGRAWNAME(256), buf);
    if (res < 0)
    {
        SIS_LOG_E(SiSLog::getOwnerSiS(), TAG, "HIDIOCGRAWNAME, errno=%d (%s)", errno, strerror(errno));
        //perror("HIDIOCGRAWNAME");
    }
    else 
    {
        SIS_LOG_D(SiSLog::getOwnerSiS(), TAG, "Raw Name: %s", buf);

        /* save raw name */
        std::string rawName(buf);
        sisDeviceAttribute->setRawName(rawName);
    }

    /* Get Physical Location */
    /*
    res = ioctl(fd, HIDIOCGRAWPHYS(256), buf);
    if (res < 0)
    {
        perror("HIDIOCGRAWPHYS");
    }
    else
    {
        printf("Raw Phys: %s\n", buf);
    }
    */

    /* Get Raw Info */
    res = ioctl(fd, HIDIOCGRAWINFO, &info);
    if (res < 0)
    {
        SIS_LOG_E(SiSLog::getOwnerSiS(), TAG, "HIDIOCGRAWINFO, errno=%d (%s)", errno, strerror(errno));
        //perror("HIDIOCGRAWINFO");
    }
    else
    {
        SIS_LOG_D(SiSLog::getOwnerSiS(), TAG, "Raw Info:");
        SIS_LOG_D(SiSLog::getOwnerSiS(), TAG, "\tbustype: %d (%s)", info.bustype, bus_str(info.bustype));
        SIS_LOG_D(SiSLog::getOwnerSiS(), TAG, "\tvendor: 0x%04hx", info.vendor);
        SIS_LOG_D(SiSLog::getOwnerSiS(), TAG, "\tproduct: 0x%04hx", info.product);

        /* save vid/pid */
        sisDeviceAttribute->setVID(info.vendor);
        sisDeviceAttribute->setPID(info.product);
    }

    /* Set Feature */
    /*
    buf[0] = 0x9; // Report Number
    buf[1] = 0xff;
    buf[2] = 0xff;
    buf[3] = 0xff;
    res = ioctl(fd, HIDIOCSFEATURE(4), buf);
    if (res < 0)
    {
        STF_DEBUG perror("HIDIOCSFEATURE");
    }
    else
    {
        STF_DEBUG printf("ioctl HIDIOCGFEATURE returned: %d\n", res);
    }
    */

    /* Get Feature */
    /*
    buf[0] = 0x9; // Report Number
    res = ioctl(fd, HIDIOCGFEATURE(256), buf);
    if (res < 0)
    {
        STF_DEBUG perror("HIDIOCGFEATURE");
    }
    else
    {
        STF_DEBUG printf("ioctl HIDIOCGFEATURE returned: %d\n", res);
        STF_DEBUG printf("Report data (not containing the report number):\n\t");
        for (i = 0; i < res; i++)
        {
            printf("%hhx ", buf[i]);
        }
        STF_DEBUG puts("\n");
    }
    */

    close(fd);
    return true;
}

const char *
SiSDeviceMgr::bus_str(int bus)
{
    switch (bus) 
    {
    case BUS_USB:		
        return "USB";	
        break;
    case BUS_HIL:		
        return "HIL";	
        break;	
    case BUS_BLUETOOTH:		
        return "Bluetooth";
        break;
    /*
    case BUS_VIRTUAL:		
        return "Virtual";
        break;
    */
    default:	
        return "Other";
        break;	
    }
}