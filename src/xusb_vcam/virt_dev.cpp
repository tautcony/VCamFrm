#include <WinSock2.h>
#include <InitGuid.h>
#include <WinIoCtl.h>
#include <cstdio>
//#include "usb_info.h"
#include <SetupAPI.h>
#pragma comment(lib,"setupapi")
#include <cfgmgr32.h>
#include "ioctl.h"

#include "virt_dev.h"
#include <string>
#include <Tchar.h>

struct virt_usb_t
{
    HANDLE hFile;

    HANDLE hSemaphore;
    HANDLE hRemoveEvent;
    int bus_addr;
    BOOL bReplug;

    std::string dev_id;
    std::string hw_ids;
    std::string comp_ids;

    int buf_size;
};

static WCHAR* find_virt_usb_path(WCHAR* dev_path)
{
    DWORD index = 0;
    SP_DEVINFO_DATA devInfo = {sizeof(SP_DEVINFO_DATA)};
    SP_DEVICE_INTERFACE_DATA devInter = {sizeof(SP_DEVICE_INTERFACE_DATA)};


    const HDEVINFO devList = SetupDiGetClassDevs(&GUID_XUSB_VIRT_INTERFACE, nullptr, nullptr,
                                           DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (devList == reinterpret_cast<HANDLE>(-1))
    {
        printf("SetupDiGetClassDev: [%s] error=%d\n", "USB", GetLastError());
        return nullptr;
    }

    while (SetupDiEnumDeviceInfo(devList, index++, &devInfo))
    {
        char buf[4096];
        const auto devDetail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(buf);
        devDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        if (!SetupDiEnumDeviceInterfaces(devList, nullptr, &GUID_XUSB_VIRT_INTERFACE, index - 1, &devInter))
        {
            printf("SetupDiEnumDeviceInterfaces err\n");
            continue;
        }

        if (!SetupDiGetDeviceInterfaceDetail(devList, &devInter, devDetail, sizeof(buf), nullptr, nullptr))
        {
            printf("SetupDiGetDeviceInterfaceDetail err\n");
            continue;
        }

        wcscpy_s(dev_path, 260, devDetail->DevicePath);
    }
    SetupDiDestroyDeviceInfoList(devList);

    if (dev_path[0] == 0) return nullptr;

    return dev_path;
}

void* virt_usb_open()
{
    WCHAR dev_path[260] = {0};
    if (!find_virt_usb_path(dev_path))
    {
        printf("*** not found virtual usb path.\n");
        return nullptr;
    }

    const auto hFile = CreateFile(dev_path, GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        printf("open usb virtual device err=%d\n", GetLastError());

        return nullptr;
    }

    const auto dev = new virt_usb_t;
    dev->hFile = hFile;
    dev->hSemaphore = nullptr;
    dev->hRemoveEvent = nullptr;
    dev->bus_addr = 0;
    dev->bReplug = FALSE;
    dev->buf_size = 1024 * 64;

    return dev;
}

void virt_usb_close(void* handle)
{
    const auto dev = static_cast<virt_usb_t*>(handle);
    if (!dev) return;

    if (dev->hSemaphore)
    {
        CloseHandle(static_cast<HANDLE>(dev->hSemaphore));
        dev->hSemaphore = nullptr;
    }

    CloseHandle(dev->hFile);

    delete dev;
}

int virt_usb_plugin(void* handle, const char* dev_id, const char* hw_ids, const char* comp_ids)
{
    auto dev = static_cast<virt_usb_t*>(handle);
    if (!dev) return -1;

    auto bus_addr = 0;
    ioctl_pdo_create_t cp{};
    memset(&cp, 0, sizeof(cp));

    auto devid = _T("usb\\vid_05ac&pid_12a8");
    // devid = L"usb\\vid_05ca&pid_18c6";
    // devid = L"usb\\vid_0ac8&pid_3420"; // 0ac8, 3420
    // devid = L"usb\\vid_0781&pid_5580"; // U store

    // wcscpy(cp.hardware_ids, devid);
    // wcscpy(cp.compatible_ids, L"USB\\COMPOSITE");


    if (!hw_ids || strlen(hw_ids) == 0)
    {
        return -1;
    }
    wchar_t t[HW_IDS_COUNT];
    int i;
    memset(t, 0, sizeof(t));
    MultiByteToWideChar(CP_ACP, 0, hw_ids, -1, t, 259);
    t[258] = 0;
    t[259] = 0; //两个零结尾
    for (i = 0; i < 260; ++i)
    {
        if (t[i] == L'\n') t[i] = L'\0';
    }
    memcpy(cp.hardware_ids, t, sizeof(t));

    if (dev_id && strlen(dev_id) > 0)
    {
        memset(t, 0, sizeof(t));
        MultiByteToWideChar(CP_ACP, 0, dev_id, -1, t, 259);
        t[259] = 0;
        memcpy(cp.device_id, t, sizeof(t));
    }
    else
    {
        memcpy(cp.device_id, t, sizeof(t));
    }

    if (comp_ids && strlen(comp_ids) > 0)
    {
        memset(t, 0, sizeof(t));
        MultiByteToWideChar(CP_ACP, 0, comp_ids, -1, t, 259);
        t[258] = 0;
        t[259] = 0; //两个0结尾
        for (i = 0; i < 260; ++i)
        {
            if (t[i] == _T('\n')) t[i] = _T('\0');
        }
        memcpy(cp.compatible_ids, t, sizeof(t));
    }

    if (dev_id)dev->dev_id = dev_id;
    dev->hw_ids = hw_ids;
    if (comp_ids)dev->comp_ids = comp_ids;


    auto hsem = CreateSemaphore(nullptr, 0, MAXLONG, nullptr);
    cp.hSemaphore = reinterpret_cast<ULONGLONG>(hsem);
    dev->hSemaphore = hsem;

    auto hevt = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    cp.hRemoveEvent = reinterpret_cast<ULONGLONG>(hevt);
    dev->hRemoveEvent = hevt;
    if (dev->bReplug) cp.BusAddress = dev->bus_addr;

    DWORD bytes;
    const auto bRet = DeviceIoControl(dev->hFile, IOCTL_PDO_ADD, &cp, sizeof(cp), &bus_addr, sizeof(int), &bytes, nullptr);
    if (bRet) dev->bus_addr = bus_addr;

    printf("IOCTL_PDO_ADD: ret=%d,err=%d\n", bRet, GetLastError());

    return bRet ? 0 : -1;
}

int virt_usb_unplug(void* handle)
{
    const auto dev = static_cast<virt_usb_t*>(handle);
    if (!dev) return -1;

    DWORD bytes;
    const auto bRet = DeviceIoControl(dev->hFile, IOCTL_PDO_REMOVE, nullptr, 0, nullptr, 0, &bytes, nullptr);
    if (bRet)
    {
        if (dev->hSemaphore)
        {
            CloseHandle(static_cast<HANDLE>(dev->hSemaphore));
            dev->hSemaphore = nullptr;
        }
        if (dev->hRemoveEvent)
        {
            ///等待3秒，直到设备被移除
            const LONG timeout = 5 * 1000; //
            const auto ret = WaitForSingleObject(static_cast<HANDLE>(dev->hRemoveEvent), timeout);
            if (ret != 0)
            {
                printf("*** Not Wait For Device Remove.\n");
            }

            CloseHandle(static_cast<HANDLE>(dev->hRemoveEvent));
            dev->hRemoveEvent = nullptr;
        }
    }

    return bRet ? 0 : -1;
}

int virt_usb_replug(void* handle)
{
    const auto dev = static_cast<virt_usb_t*>(handle);
    if (!dev) return -1;
    if (dev->hw_ids.length() < 2)return -1; //// HWID empty

    printf("*** --- simulate unplug and replug \n");

    auto r = virt_usb_unplug(handle);

    dev->bReplug = TRUE;
    r = virt_usb_plugin(handle, dev->dev_id.c_str(), dev->hw_ids.c_str(), dev->comp_ids.c_str());
    dev->bReplug = FALSE;

    return r;
}

static DWORD CALLBACK replug_thread(void* _p)
{
    printf("*** --- simulate unplug and replug \n");
    const auto dev = static_cast<virt_usb_t*>(_p);
    // Sleep(140);
    // virt_usb_replug(dev);
    virt_usb_unplug(dev);
    // Sleep(150);
    dev->bReplug = 1;
    virt_usb_plugin(dev, dev->dev_id.c_str(), dev->hw_ids.c_str(), dev->comp_ids.c_str());
    dev->bReplug = 0;
    return 0;
}

usbtx_header_t* virt_usb_begin(void* handle)
{
    const auto dev = static_cast<virt_usb_t*>(handle);
    if (!dev) return nullptr;

    if (WaitForSingleObject(dev->hSemaphore, 10 * 1000) == WAIT_TIMEOUT)
        return nullptr;

    auto buf_size = dev->buf_size;

    auto hdr = static_cast<ioctl_usbtx_header_t*>(malloc(buf_size));

    DWORD bytes = 0;
    memset(hdr, 0, buf_size);
    auto bRet = DeviceIoControl(dev->hFile, IOCTL_BEGIN_TRANS_USB_DATA, nullptr, 0, hdr, buf_size, &bytes, nullptr);
    if (!bRet)
    {
        if (GetLastError() == ERROR_MORE_DATA)
        {
            // -> STATUS_BUFFER_OVERFLOW
            printf("buffer too small.\n");
            if (bytes != sizeof(ioctl_usbtx_header_t))
            {
                free(hdr);
                printf("buffer overflow: ret not valid.\n");
                return nullptr;
            }
            buf_size = hdr->data_length + sizeof(ioctl_usbtx_header_t) + 4096;

            dev->buf_size = buf_size;

            free(hdr);
            hdr = static_cast<ioctl_usbtx_header_t*>(malloc(buf_size));

            bytes = 0;
            bRet = DeviceIoControl(dev->hFile, IOCTL_BEGIN_TRANS_USB_DATA, nullptr, 0, hdr, buf_size, &bytes, nullptr);
        }
    }
    if (!bRet)
    {
        printf("IOCTL begin: err=%d\n", GetLastError());
        free(hdr);
        return nullptr;
    }

    if (hdr->data_length + sizeof(ioctl_usbtx_header_t) > buf_size)
    {
        printf("---- buffer not large enough.\n");
        buf_size = hdr->data_length + sizeof(ioctl_usbtx_header_t) + 4096;

        hdr = static_cast<ioctl_usbtx_header_t*>(realloc(hdr, buf_size));

        dev->buf_size = buf_size;
    }

    auto ret = reinterpret_cast<usbtx_header_t*>(reinterpret_cast<char*>(hdr) + 24);
    ret->data_length = hdr->data_length;
    ret->result = static_cast<int>(bytes) - sizeof(ioctl_usbtx_header_t);


    if (ret->type == 4 && ret->reset.type == 2)
    {
        //模拟设备replug

        ret->result = 0; // success,
        ret->data_length = 0;

        virt_usb_end(handle, ret);

        DWORD tid;
        const auto thread = CreateThread(nullptr, 0, replug_thread, dev, 0, &tid);
        if (thread != nullptr)
            CloseHandle(thread);

        return nullptr;
    }

    return ret;
}


int virt_usb_end(void* handle, usbtx_header_t* header)
{
    const auto dev = static_cast<virt_usb_t*>(handle);
    if (!dev) return -1;

    const auto hdr = reinterpret_cast<ioctl_usbtx_header_t*>(reinterpret_cast<char*>(header) - 24);

    if (hdr->data_length > header->data_length && hdr->type == 3 && !hdr->transfer.is_read)
    {
        //
        printf("type=3, subtype=%d, is_read=%d, len=%ld, actual_len=%d\n", hdr->transfer.type, hdr->transfer.is_read,
               hdr->data_length, header->data_length);
    }
    hdr->result = header->result;
    hdr->data_length = header->data_length;

    DWORD bytes;
    auto bRet = DeviceIoControl(dev->hFile, IOCTL_END_TRANS_USB_DATA, hdr,
                                sizeof(ioctl_usbtx_header_t) + hdr->data_length, nullptr, 0, &bytes, nullptr);
    // printf("virt_usb_end: bRet=%d, err=%d\n", bRet, GetLastError());

    free(hdr);

    return 0;
}


#if 0
#include "usb_dev.h"

void cbk(void* header, void* data, int ret, void* param)
{
    usbtx_header_t* hdr = (usbtx_header_t*)header;
    if (ret >= 0) {
        hdr->result = 0;
        hdr->data_length = ret;
    }
    else {
        hdr->result = -1;
    }

    // printf("**TRANS** <--end IOCTL: type=%d, trans_type2=%d, Ret=%d\n\n", hdr->type, hdr->descriptor.type, ret );

    virt_usb_end(param, hdr);
}

void test()
{
    list<usb_info_t> usb_infos;
    enum_usb_devices(&usb_infos);
    const char* dev_id;
    void* usb = 0;

    dev_id = "usb\\vid_05ac&pid_12a8";
    // dev_id = "usb\\vid_05ca&pid_18c6";
    // dev_id = "usb\\vid_0ac8&pid_3420";
    // dev_id = "usb\\vid_0781&pid_5580"; // U 盘
    for (list<usb_info_t>::iterator it = usb_infos.begin(); it != usb_infos.end(); ++it) {
        if (strnicmp(it->dev_id, dev_id, strlen(dev_id)) == 0) {
            printf("**** [%s] [%s] [%s] \n", it->hw_id, it->dev_id, it->dev_desc);

            usb = xusb_open(&(*it));
            // break;
        }
    }
    // xusb_close(usb); exit(0);

    void* h = virt_usb_open();
    virt_usb_add(h);

    while (1) {

        usbtx_header_t* hdr = virt_usb_begin(h);
        if (!hdr)continue;

        // printf("-->begin IOCTL: type=%d, type2=%d, dataLen=%d ", hdr->type, hdr->descriptor.type, hdr->data_length );
        // if (hdr->type == 3) printf(", bRead=%d, ep_address=0x%X\n", hdr->transfer.is_read, hdr->transfer.ep_address); else printf("\n");

        int r;
        r = xusb_ioctl(usb, hdr, hdr->data, hdr->data_length, cbk, h);

        //  printf("<--end IOCTL: type=%d, type2=%d, Ret=%d\n\n", hdr->type,hdr->descriptor.type, r );
    }

    // xusb_close(usb);
}

int main(int argc, char** argv)
{
    // void* h = virt_usb_open();
    // virt_usb_add(h);
    int sz = sizeof(usbtx_header_t);

    test();
    return 0;
}
#endif
