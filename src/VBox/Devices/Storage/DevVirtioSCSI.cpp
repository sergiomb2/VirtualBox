/* $Id$ $Revision$ $Date$ $Author$ */
/** @file
 * VBox storage devices - Virtio SCSI Driver
 *
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
//#define LOG_GROUP LOG_GROUP_DRV_SCSI
#define LOG_GROUP LOG_GROUP_DEV_VIRTIO

#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmstorageifs.h>
#include <VBox/vmm/pdmcritsect.h>
#include <VBox/version.h>
#include <VBox/log.h>
#include <iprt/errcore.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include "../build/VBoxDD.h"
#include <VBox/scsi.h>
#ifdef IN_RING3
# include <iprt/alloc.h>
# include <iprt/memcache.h>
# include <iprt/semaphore.h>
# include <iprt/sg.h>
# include <iprt/param.h>
# include <iprt/uuid.h>
#endif
#include "../VirtIO/Virtio_1_0.h"

#include "VBoxSCSI.h"
#include "VBoxDD.h"


/**
 * @name VirtIO 1.0 SCSI Host feature bits (See VirtIO 1.0 specification, Section 5.6.3)
 * @{  */
#define VIRTIO_SCSI_F_INOUT                RT_BIT_64(0)           /** Request is device readable AND writeable         */
#define VIRTIO_SCSI_F_HOTPLUG              RT_BIT_64(1)           /** Host allows hotplugging SCSI LUNs & targets      */
#define VIRTIO_SCSI_F_CHANGE               RT_BIT_64(2)           /** Host LUNs chgs via VIRTIOSCSI_T_PARAM_CHANGE evt */
#define VIRTIO_SCSI_F_T10_PI               RT_BIT_64(3)           /** Add T10 port info (DIF/DIX) in SCSI req hdr      */
/** @} */


#define VIRTIOSCSI_HOST_SCSI_FEATURES_ALL \
            (VIRTIO_SCSI_F_INOUT | VIRTIO_SCSI_F_HOTPLUG | VIRTIO_SCSI_F_CHANGE | VIRTIO_SCSI_F_T10_PI)

#define VIRTIOSCSI_HOST_SCSI_FEATURES_NONE          0

#define VIRTIOSCSI_HOST_SCSI_FEATURES_OFFERED \
            VIRTIOSCSI_HOST_SCSI_FEATURES_NONE

/**
 * TEMPORARY NOTE: following parameter is set to 1 for early development. Will be increased later
 */
#define VIRTIOSCSI_REQ_QUEUE_CNT                    1            /**< Number of req queues exposed by dev.            */
#define VIRTIOSCSI_QUEUE_CNT                        VIRTIOSCSI_REQ_QUEUE_CNT + 2
#define VIRTIOSCSI_MAX_TARGETS                      1            /**< Can probably determined from higher layers      */
#define VIRTIOSCSI_MAX_LUN                          1            /* < VirtIO specification, section 5.6.4             */
#define VIRTIOSCSI_MAX_COMMANDS_PER_LUN             1            /* < T.B.D. What is a good value for this?           */
#define VIRTIOSCSI_MAX_SEG_COUNT                    1024         /* < T.B.D. What is a good value for this?           */
#define VIRTIOSCSI_MAX_SECTORS_HINT                 0x10000      /* < VirtIO specification, section 5.6.4             */
#define VIRTIOSCSI_MAX_CHANNEL_HINT                 0            /* < VirtIO specification, section 5.6.4 should be 0 */
#define VIRTIOSCSI_SAVED_STATE_MINOR_VERSION        0x01         /**< SSM version #                                   */

#define PCI_DEVICE_ID_VIRTIOSCSI_HOST               0x1048       /**< Informs guest driver of type of VirtIO device   */
#define PCI_CLASS_BASE_MASS_STORAGE                 0x01         /**< PCI Mass Storage device class                   */
#define PCI_CLASS_SUB_SCSI_STORAGE_CONTROLLER       0x00         /**< PCI SCSI Controller subclass                    */
#define PCI_CLASS_PROG_UNSPECIFIED                  0x00         /**< Programming interface. N/A.                     */
#define VIRTIOSCSI_PCI_CLASS                        0x01         /**< Base class Mass Storage?                        */


#define VIRTIOSCSI_SENSE_SIZE_DEFAULT               96          /**< VirtIO 1.0: 96 on reset, guest can change       */
#define VIRTIOSCSI_CDB_SIZE_DEFAULT                 32          /**< VirtIO 1.0: 32 on reset, guest can change       */
#define VIRTIOSCSI_PI_BYTES_IN                      1           /**< Value TBD (see section 5.6.6.1)                 */
#define VIRTIOSCSI_PI_BYTES_OUT                     1           /**< Value TBD (see section 5.6.6.1)                 */
#define VIRTIOSCSI_DATA_OUT                         512         /**< Value TBD (see section 5.6.6.1)                 */

/**
 * VirtIO SCSI Host Device device-specific queue indicies
 *
 * Virtqs (and their indices) are specified for a SCSI Host Device as described in the VirtIO 1.0 specification
 * section 5.6. Thus there is no need to explicitly indicate the number of queues needed by this device. The number
 * of req queues is variable and determined by virtio_scsi_config.num_queues. See VirtIO 1.0 spec section 5.6.4
 */
#define CONTROLQ_IDX                                0            /**< Spec-defined Index of control queue             */
#define EVENTQ_IDX                                  1            /**< Spec-defined Index of event queue               */
#define VIRTQ_REQ_BASE                              2            /**< Spec-defined base index of request queues       */

#define QUEUENAME(qIdx) (pThis->szQueueNames[qIdx])              /**< Macro to get queue name from its index          */
#define CBQUEUENAME(qIdx) RTStrNLen(QUEUENAME(qIdx), sizeof(QUEUENAME(qIdx)))

#define IS_REQ_QUEUE(qIdx) (qIdx >= VIRTQ_REQ_BASE && qIdx < VIRTIOSCSI_QUEUE_CNT)
/**
 * The following struct is the VirtIO SCSI Host Device   device-specific configuration described in section 5.6.4
 * of the VirtIO 1.0 specification. This layout maps an MMIO area shared VirtIO guest driver. The VBox VirtIO
 * this virtual controller device implementation is a client of. The frame work calls back whenever the guest driver
 * accesses any part of field in this struct
 */
typedef struct virtio_scsi_config
{
    uint32_t uNumQueues;                                         /**< num_queues       # of req q's exposed by dev    */
    uint32_t uSegMax;                                            /**< seg_max          Max # of segs allowed in cmd   */
    uint32_t uMaxSectors;                                        /**< max_sectors      Hint to guest max xfer to use  */
    uint32_t uCmdPerLun;                                         /**< cmd_per_lun      Max # of link cmd sent per lun */
    uint32_t uEventInfoSize;                                     /**< event_info_size  Fill max, evtq bufs            */
    uint32_t uSenseSize;                                         /**< sense_size       Max sense data size dev writes */
    uint32_t uCdbSize;                                           /**< cdb_size         Max CDB size driver writes     */
    uint16_t uMaxChannel;                                        /**< max_channel      Hint to guest driver           */
    uint16_t uMaxTarget;                                         /**< max_target       Hint to guest driver           */
    uint32_t uMaxLun;                                            /**< max_lun          Hint to guest driver           */
} VIRTIOSCSI_CONFIG_T, PVIRTIOSCSI_CONFIG_T;


/**
 * @name VirtIO 1.0 SCSI Host Device device specific control types
 * @{  */
#define VIRTIOSCSI_T_NO_EVENT                       0
#define VIRTIOSCSI_T_TRANSPORT_RESET                1
#define VIRTIOSCSI_T_ASYNC_NOTIFY                   2           /**< Asynchronous notification                       */
#define VIRTIOSCSI_T_PARAM_CHANGE                   3
/** @} */

/**
 * Device operation: eventq
 */
#define VIRTIOSCSI_T_EVENTS_MISSED             0x80000000
typedef struct virtio_scsi_event {
    // Device-writable part
    uint32_t uEvent;                                            /**< event:                                          */
    uint8_t  uLUN[8];                                           /**< lun                                             */
    uint32_t uReason;                                           /**< reason                                          */
} VIRTIOSCSI_EVENT_T, *PVIRTIOSCSI_EVENT_T;

/**
 * @name VirtIO 1.0 SCSI Host Device device specific event types
 * @{  */
#define VIRTIOSCSI_EVT_RESET_HARD                   0           /**<                                                 */
#define VIRTIOSCSI_EVT_RESET_RESCAN                 1           /**<                                                 */
#define VIRTIOSCSI_EVT_RESET_REMOVED                2           /**<                                                 */
/** @} */


#pragma pack(1)

/**
 * Device operation: reqestq
 */
struct REQ_CMD_HDR
{
    uint8_t  uLUN[8];                                           /**< lun                                          */
    uint64_t uId;                                               /**< id                                           */
    uint8_t  uTaskAttr;                                         /**< task_attr                                    */
    uint8_t  uPrio;                                             /**< prio                                         */
    uint8_t  uCrn;                                              /**< crn                                          */
};

struct REQ_CMD_PI
{
    uint32_t uPiBytesOut;                                       /**< pi_bytesout                                  */
    uint32_t uPiBytesIn;                                        /**< pi_bytesin                                   */
};

struct REQ_RESP_HDR
{
    uint32_t uSenseLen;                                         /**< sense_len                                    */
    uint32_t uResidual;                                         /**< residual                                     */
    uint16_t uStatusQualifier;                                  /**< status_qualifier                             */
    uint8_t  uStatus;                                           /**< status            SCSI status code           */
    uint8_t  uResponse;                                         /**< response                                     */
};

typedef struct virtio_scsi_req_cmd
{
    /* Device-readable section */

    struct REQ_CMD_HDR  cmdHdr;
    uint8_t  uCdb[1];                                           /**< cdb                                          */

    struct REQ_CMD_PI piHdr;                                    /** T10 Pi block integrity (optional feature)     */
    uint8_t  uPiOut[1];                                         /**< pi_out[]          T10 pi block integrity     */
    uint8_t  uDataOut[1];                                       /**< dataout                                      */

    /** Device writable section */

    struct REQ_RESP_HDR  respHdr;
    uint8_t  uSense[1];                                         /**< sense                                        */
    uint8_t  uPiIn[1];                                          /**< pi_in[]           T10 Pi block integrity     */
    uint8_t  uDataIn[1];                                        /**< detain;                                      */

}  VIRTIOSCSI_REQ_CMD_T, *PVIRTIOSCSI_REQ_CMD_T;
#pragma pack()

/**
 * @name VirtIO 1.0 SCSI Host Device Req command-specific response values
 * @{  */
#define VIRTIOSCSI_S_OK                             0          /**< control, command                                 */
#define VIRTIOSCSI_S_OVERRUN                        1          /**< control                                          */
#define VIRTIOSCSI_S_ABORTED                        2          /**< control                                          */
#define VIRTIOSCSI_S_BAD_TARGET                     3          /**< control, command                                 */
#define VIRTIOSCSI_S_RESET                          4          /**< control                                          */
#define VIRTIOSCSI_S_BUSY                           5          /**< control, command                                 */
#define VIRTIOSCSI_S_TRANSPORT_FAILURE              6          /**< control, command                                 */
#define VIRTIOSCSI_S_TARGET_FAILURE                 7          /**< control, command                                 */
#define VIRTIOSCSI_S_NEXUS_FAILURE                  8          /**< control, command                                 */
#define VIRTIOSCSI_S_FAILURE                        9          /**< control, command                                 */
#define VIRTIOSCSI_S_INCORRECT_LUN                  12         /**< command                                          */
/** @} */

/**
 * @name VirtIO 1.0 SCSI Host Device command-specific task_attr values
 * @{  */
#define VIRTIOSCSI_S_SIMPLE                        0           /**<                                                  */
#define VIRTIOSCSI_S_ORDERED                       1           /**<                                                  */
#define VIRTIOSCSI_S_HEAD                          2           /**<                                                  */
#define VIRTIOSCSI_S_ACA                           3           /**<                                                  */
/** @} */

/**
 * @name VirtIO 1.0 SCSI Host Device Control command before we know type (5.6.6.2)
 * @{  */
typedef struct virtio_scsi_ctrl
{
    uint32_t uType;
} VIRTIOSCSI_CTRL, *PVIRTIOSCSI_CTRL_T;

/**
 * @name VirtIO 1.0 SCSI Host Device command-specific TMF values
 * @{  */
#define VIRTIOSCSI_T_TMF                           0           /**<                                                  */
#define VIRTIOSCSI_T_TMF_ABORT_TASK                0           /**<                                                  */
#define VIRTIOSCSI_T_TMF_ABORT_TASK_SET            1           /**<                                                  */
#define VIRTIOSCSI_T_TMF_CLEAR_ACA                 2           /**<                                                  */
#define VIRTIOSCSI_T_TMF_CLEAR_TASK_SET            3           /**<                                                  */
#define VIRTIOSCSI_T_TMF_I_T_NEXUS_RESET           4           /**<                                                  */
#define VIRTIOSCSI_T_TMF_LOGICAL_UNIT_RESET        5           /**<                                                  */
#define VIRTIOSCSI_T_TMF_QUERY_TASK                6           /**<                                                  */
#define VIRTIOSCSI_T_TMF_QUERY_TASK_SET            7           /**<                                                  */
/*** @} */

#pragma pack(1)
typedef struct virtio_scsi_ctrl_tmf
{
     // Device-readable part
    uint32_t uType;                                            /** type                                              */
    uint32_t uSubtype;                                         /** subtype                                           */
    uint8_t  uLUN[8];                                          /** lun                                               */
    uint64_t uId;                                              /** id                                                */
    // Device-writable part
    uint8_t  uResponse;                                        /** response                                          */
} VIRTIOSCSI_CTRL_TMF_T, *PVIRTIOSCSI_CTRL_TMF_T;
#pragma pack(0)

/**
 * @name VirtIO 1.0 SCSI Host Device device specific tmf control response values
 * @{  */
#define VIRTIOSCSI_S_FUNCTION_COMPLETE            0           /**<                                                   */
#define VIRTIOSCSI_S_FUNCTION_SUCCEEDED           10          /**<                                                   */
#define VIRTIOSCSI_S_FUNCTION_REJECTED            11          /**<                                                   */
/** @} */

#define VIRTIOSCSI_T_AN_QUERY                     1           /** Asynchronous notification query                    */
#define VIRTIOSCSI_T_AN_SUBSCRIBE                 2           /** Asynchronous notification subscription             */

#pragma pack(1)
typedef struct virtio_scsi_ctrl_an
{
    // Device-readable part
    uint32_t  uType;                                          /** type                                               */
    uint8_t   uLUN[8];                                        /** lun                                                */
    uint32_t  uEventsRequested;                                /** event_requested                                    */
    // Device-writable part
    uint32_t  uEventActual;                                   /** event_actual                                       */
    uint8_t   uResponse;                                      /** response                                           */
}  VIRTIOSCSI_CTRL_AN, *PVIRTIOSCSI_CTRL_AN_T;
#pragma pack()

/**
 * @name VirtIO 1.0 SCSI Host Device device specific tmf control response values
 * @{  */
#define VIRTIOSCSI_EVT_ASYNC_OPERATIONAL_CHANGE  2           /**<                                                   */
#define VIRTIOSCSI_EVT_ASYNC_POWER_MGMT          4           /**<                                                   */
#define VIRTIOSCSI_EVT_ASYNC_EXTERNAL_REQUEST    8           /**<                                                   */
#define VIRTIOSCSI_EVT_ASYNC_MEDIA_CHANGE        16          /**<                                                   */
#define VIRTIOSCSI_EVT_ASYNC_MULTI_HOST          32          /**<                                                   */
#define VIRTIOSCSI_EVT_ASYNC_DEVICE_BUSY         64          /**<                                                   */
/** @} */

#define SUBSCRIBABLE_EVENTS \
              VIRTIOSCSI_EVT_ASYNC_OPERATIONAL_CHANGE \
            & VIRTIOSCSI_EVT_ASYNC_POWER_MGMT \
            & VIRTIOSCSI_EVT_ASYNC_EXTERNAL_REQUEST \
            & VIRTIOSCSI_EVT_ASYNC_MEDIA_CHANGE \
            & VIRTIOSCSI_EVT_ASYNC_MULTI_HOST \
            & VIRTIOSCSI_EVT_ASYNC_DEVICE_BUSY

/**
 * Worker thread context
 */
typedef struct WORKER
{
    R3PTRTYPE(PPDMTHREAD)           pThread;                  /**< pointer to worker thread's handle                 */
    SUPSEMEVENT                     hEvtProcess;              /**< handle of associated sleep/wake-up semaphore      */
    bool                            fSleeping;                /**< Flags whether worker  thread is sleeping or not   */
    bool                            fNotified;                /**< Flags whether worker thread notified              */
} WORKER, *PWORKER;

/**
 * State of a target attached to the VirtIO SCSI Host
 */
typedef struct VIRTIOSCSITARGET
{
    /** Pointer to PCI device that owns this target instance. - R3 pointer */
    R3PTRTYPE(struct VIRTIOSCSI *)  pVirtioScsiR3;

    /** Pointer to attached driver's base interface. */
    R3PTRTYPE(PPDMIBASE)            pDrvBase;

    /** Target LUN */
    RTUINT                          iLUN;

    /** Target LUN Description */
    char *                          pszLunName;

    /** Target base interface. */
    PDMIBASE                        IBase;

    /** Flag whether device is present. */
    bool                            fPresent;

    /** Media port interface. */
    PDMIMEDIAPORT                   IMediaPort;

    /** Pointer to the attached driver's media interface. */
    R3PTRTYPE(PPDMIMEDIA)           pDrvMedia;

    /** Extended media port interface. */
    PDMIMEDIAEXPORT                 IMediaExPort;

     /** Pointer to the attached driver's extended media interface. */
    R3PTRTYPE(PPDMIMEDIAEX)         pDrvMediaEx;

    /** Status LED interface */
    PDMILEDPORTS                    ILed;

    /** The status LED state for this device. */
    PDMLED                          led;

    /** Number of requests active */
    volatile uint32_t               cReqsInProgress;

} VIRTIOSCSITARGET, *PVIRTIOSCSITARGET;

/**
 *  PDM instance data (state) for VirtIO Host SCSI device
 *
 * @extends     PDMPCIDEV
 */
typedef struct VIRTIOSCSI
{
    /** Opaque handle to VirtIO common framework (must be first item
     *  in this struct so PDMINS_2_DATA macro's casting works) */
    VIRTIOHANDLE                    hVirtio;

    /** SCSI target instances data */
    VIRTIOSCSITARGET                aTargetInstances[VIRTIOSCSI_MAX_TARGETS];

    /** Per device-bound virtq worker-thread contexts (eventq slot unused) */
    WORKER                          aWorker[VIRTIOSCSI_QUEUE_CNT];

    bool                            fBootable;
    bool                            fRCEnabled;
    bool                            fR0Enabled;
    /** Instance name */
    const char                      szInstance[16];

    /** Device-specific spec-based VirtIO queuenames */
    const char                      szQueueNames[VIRTIOSCSI_QUEUE_CNT][VIRTIO_MAX_QUEUE_NAME_SIZE];

    /** Track which VirtIO queues we've attached to */
    bool                            fQueueAttached[VIRTIOSCSI_QUEUE_CNT];

    /** Device base interface. */
    PDMIBASE                        IBase;

    /** Pointer to the device instance. - R3 ptr. */
    PPDMDEVINSR3                    pDevInsR3;
    /** Pointer to the device instance. - R0 ptr. */
    PPDMDEVINSR0                    pDevInsR0;
    /** Pointer to the device instance. - RC ptr. */
    PPDMDEVINSRC                    pDevInsRC;

    /** Status LUN: LEDs port interface. */
    PDMILEDPORTS                    ILeds;

    /** Status LUN: Partner of ILeds. */
    R3PTRTYPE(PPDMILEDCONNECTORS)   pLedsConnector;

    /** Base address of the memory mapping. */
    RTGCPHYS                        GCPhysMMIOBase;

    /** IMediaExPort: Media ejection notification */
    R3PTRTYPE(PPDMIMEDIANOTIFY)     pMediaNotify;

    /** Queue to send tasks to R3. - HC ptr */
    R3PTRTYPE(PPDMQUEUE)            pNotifierQueueR3;

    /** The support driver session handle. */
    R3R0PTRTYPE(PSUPDRVSESSION)     pSupDrvSession;

    /** Mask of VirtIO Async Event types this device will deliver */
    uint32_t                        uAsyncEvtsEnabled;

    /** The event semaphore the processing thread waits on. */

    /** Number of ports detected */
    uint64_t                        cTargets;

    /** True if PDMDevHlpAsyncNotificationCompleted should be called when port goes idle */
    bool volatile                   fSignalIdle;

    /** Events the guest has subscribed to get notifications of */
    uint32_t                        uSubscribedEvents;

    /** Set if events missed due to lack of bufs avail on eventq */
    bool                            fEventsMissed;

    /** VirtIO Host SCSI device runtime configuration parameters */
    VIRTIOSCSI_CONFIG_T             virtioScsiConfig;

    /** True if the guest/driver and VirtIO framework are in the ready state */
    bool                            fVirtioReady;

    /** True if VIRTIO_SCSI_F_T10_PI was negotiated */
    bool                            fHasT10pi;

    /** True if VIRTIO_SCSI_F_T10_PI was negotiated */
    bool                            fHasHotplug;

    /** True if VIRTIO_SCSI_F_T10_PI was negotiated */
    bool                            fHasInOutBufs;

    /** True if VIRTIO_SCSI_F_T10_PI was negotiated */
    bool                            fHasLunChange;

} VIRTIOSCSI, *PVIRTIOSCSI;

/**
 * Request structure for IMediaEx (Associated Interfaces implemented by DrvSCSI)
 */
typedef struct VIRTIOSCSIREQ
{
    PDMMEDIAEXIOREQ                hIoReq;                   /**< Handle of I/O request                             */
    PVIRTIOSCSITARGET              pTarget;                  /**< Target                                            */
    uint16_t                       qIdx;                     /**< Index of queue this request arrived on            */
    size_t                         cbPiOut;                  /**< Size of T10 pi in buffer                          */
    uint8_t                       *pbPiOut;                  /**< Address of pi out buffer                          */
    uint8_t                       *pbDataOut;                /**< dataout                                           */
    size_t                         cbPiIn;                   /**< Size of T10 pi buffer                             */
    uint8_t                       *pbPiIn;                   /**< Address of pi in buffer                           */
    size_t                         cbDataIn;                 /**< Size of datain buffer                             */
    uint8_t                       *pbDataIn;                 /**< datain                                            */
    size_t                         cbSense;                  /**< Size of sense buffer                              */
    uint8_t                       *pbSense;                  /**< Pointer to R3 sense buffer                        */
    uint8_t                        uStatus;                  /**< SCSI status code                                  */
    PRTSGBUF                       pInSgBuf;                 /**< Buf vector to return PDM result to VirtIO Guest   */
    PVIRTIOSCSI_REQ_CMD_T          pVirtqReq;                /**< Allocated to receive translated queued req        */
} VIRTIOSCSIREQ;

#define PTARGET_FROM_LUN_BUF(lunBuf) &pThis->aTargetInstances[lunBuf[1]];

#define SET_LUN_BUF(target, lun, out) \
     out[0] = 0x01;  out[1] = target; out[2] = (lun >> 8) & 0x40;  out[3] = lun & 0xff;  *((uint16_t *)out + 4) = 0;


DECLINLINE(const char *) virtioGetTMFTypeText(uint32_t uSubType)
{
    switch (uSubType)
    {
        case VIRTIOSCSI_T_TMF_ABORT_TASK:               return "ABORT TASK";
        case VIRTIOSCSI_T_TMF_ABORT_TASK_SET:           return "ABORT TASK SET";
        case VIRTIOSCSI_T_TMF_CLEAR_ACA:                return "CLEAR ACA";
        case VIRTIOSCSI_T_TMF_CLEAR_TASK_SET:           return "CLEAR TASK SET";
        case VIRTIOSCSI_T_TMF_I_T_NEXUS_RESET:          return "I T NEXUS RESET";
        case VIRTIOSCSI_T_TMF_LOGICAL_UNIT_RESET:       return "LOGICAL UNIT RESET";
        case VIRTIOSCSI_T_TMF_QUERY_TASK:               return "QUERY TASK";
        case VIRTIOSCSI_T_TMF_QUERY_TASK_SET:           return "QUERY TASK SET";
        default:                                        return "<unknown>";
    }
}

DECLINLINE(const char *) virtioGetReqRespText(uint32_t vboxRc)
{
    switch (vboxRc)
    {
        case VIRTIOSCSI_S_OK:                           return "OK";
        case VIRTIOSCSI_S_OVERRUN:                      return "OVERRRUN";
        case VIRTIOSCSI_S_ABORTED:                      return "ABORTED";
        case VIRTIOSCSI_S_BAD_TARGET:                   return "BAD TARGET";
        case VIRTIOSCSI_S_RESET:                        return "RESET";
        case VIRTIOSCSI_S_TRANSPORT_FAILURE:            return "TRANSPORT FAILURE";
        case VIRTIOSCSI_S_TARGET_FAILURE:               return "TARGET FAILURE";
        case VIRTIOSCSI_S_NEXUS_FAILURE:                return "NEXUS FAILURE";
        case VIRTIOSCSI_S_BUSY:                         return "BUSY";
        case VIRTIOSCSI_S_FAILURE:                      return "FAILURE";
        default:                                        return "<unknown>";
    }
}

DECLINLINE(const char *) virtioGetCtrlRespText(uint32_t vboxRc)
{
    switch (vboxRc)
    {
        case VIRTIOSCSI_S_OK:                           return "OK/COMPLETE";
        case VIRTIOSCSI_S_BAD_TARGET:                   return "BAD TARGET";
        case VIRTIOSCSI_S_BUSY:                         return "BUSY";
        case VIRTIOSCSI_S_NEXUS_FAILURE:                return "NEXUS FAILURE";
        case VIRTIOSCSI_S_TRANSPORT_FAILURE:            return "TRANSPORT FAILURE";
        case VIRTIOSCSI_S_FAILURE:                      return "FAILURE";
        case VIRTIOSCSI_S_INCORRECT_LUN:                return "INCORRECT LUN";
        case VIRTIOSCSI_S_FUNCTION_SUCCEEDED:           return "FUNCTION SUCCEEDED";
        case VIRTIOSCSI_S_FUNCTION_REJECTED:            return "FUNCTION REJECTED";
        default:                                        return "<unknown>";
    }
}

DECLINLINE(void) virtioGetControlAsyncMaskText(char *pszOutput, size_t cbOutput, uint32_t uAsyncTypesMask)
{
    RTStrPrintf(pszOutput, cbOutput, "%s%s%s%s%s%s",
        (uAsyncTypesMask & VIRTIOSCSI_EVT_ASYNC_OPERATIONAL_CHANGE) ? "CHANGE_OPERATION  "   : "",
        (uAsyncTypesMask & VIRTIOSCSI_EVT_ASYNC_POWER_MGMT)         ? "POWER_MGMT  "         : "",
        (uAsyncTypesMask & VIRTIOSCSI_EVT_ASYNC_EXTERNAL_REQUEST)   ? "EXTERNAL_REQ  "       : "",
        (uAsyncTypesMask & VIRTIOSCSI_EVT_ASYNC_MEDIA_CHANGE)       ? "MEDIA_CHANGE  "       : "",
        (uAsyncTypesMask & VIRTIOSCSI_EVT_ASYNC_MULTI_HOST)         ? "MULTI_HOST  "         : "",
        (uAsyncTypesMask & VIRTIOSCSI_EVT_ASYNC_DEVICE_BUSY)        ? "DEVICE_BUSY  "        : "");
}

uint8_t virtioScsiEstimateCdbLen(uint8_t uCmd, uint8_t cbMax)
{
    if (uCmd < 0x1f)
        return 6;
    else if (uCmd >= 0x20 && uCmd < 0x60)
        return 10;
    else if (uCmd >= 0x60 && uCmd < 0x80)
        return cbMax;
    else if (uCmd >= 0x80 && uCmd < 0xa0)
        return 16;
    else if (uCmd >= 0xa0 && uCmd < 0xC0)
        return 12;
    else
        return cbMax;
}

/**
 * This macro resolves to boolean true if uOffset matches a field offset and size exactly,
 * (or if it is a 64-bit field, if it accesses either 32-bit part as a 32-bit access)
 * ASSUMED this critereon is mandated by section 4.1.3.1 of the VirtIO 1.0 specification)
 * This MACRO can be re-written to allow unaligned access to a field (within bounds).
 *
 * @param   member   - Member of VIRTIO_PCI_COMMON_CFG_T
 * @result           - true or false
 */
#define MATCH_SCSI_CONFIG(member) \
            (RT_SIZEOFMEMB(VIRTIOSCSI_CONFIG_T, member) == 8 \
             && (   uOffset == RT_UOFFSETOF(VIRTIOSCSI_CONFIG_T, member) \
                 || uOffset == RT_UOFFSETOF(VIRTIOSCSI_CONFIG_T, member) + sizeof(uint32_t)) \
             && cb == sizeof(uint32_t)) \
         || (uOffset == RT_UOFFSETOF(VIRTIOSCSI_CONFIG_T, member) \
               && cb == RT_SIZEOFMEMB(VIRTIOSCSI_CONFIG_T, member))

#define LOG_ACCESSOR(member) \
        virtioLogMappedIoValue(__FUNCTION__, #member, RT_SIZEOFMEMB(VIRTIOSCSI_CONFIG_T, member), \
            pv, cb, uIntraOffset, fWrite, false, 0);

#define SCSI_CONFIG_ACCESSOR(member) \
    { \
        uint32_t uIntraOffset = uOffset - RT_UOFFSETOF(VIRTIOSCSI_CONFIG_T, member); \
        if (fWrite) \
            memcpy(((char *)&pThis->virtioScsiConfig.member) + uIntraOffset, (const char *)pv, cb); \
        else \
            memcpy((char *)pv, (const char *)(((char *)&pThis->virtioScsiConfig.member) + uIntraOffset), cb); \
        LOG_ACCESSOR(member); \
    }

#define SCSI_CONFIG_ACCESSOR_READONLY(member) \
    { \
        uint32_t uIntraOffset = uOffset - RT_UOFFSETOF(VIRTIOSCSI_CONFIG_T, member); \
        if (fWrite) \
            LogFunc(("Guest attempted to write readonly virtio_pci_common_cfg.%s\n", #member)); \
        else \
        { \
            memcpy((char *)pv, (const char *)(((char *)&pThis->virtioScsiConfig.member) + uIntraOffset), cb); \
            LOG_ACCESSOR(member); \
        } \
    }



typedef struct VIRTIOSCSIREQ *PVIRTIOSCSIREQ;

#ifdef BOOTABLE_SUPPORT_TBD
/** @callback_method_impl{FNIOMIOPORTIN} */
static DECLCALLBACK(int) virtioScsiR3BiosIoPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT uPort, uint8_t *pbDst,
                                                    uint32_t *pcTransfers, unsigned cb);
{
}
/** @callback_method_impl{FNIOMIOPORTOUT} */
static DECLCALLBACK(int) virtioScsiR3BiosIoPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT uPort, uint32_t u32, unsigned cb);
{
}
/** @callback_method_impl{FNIOMIOPORTOUTSTRING} */
static DECLCALLBACK(int) virtioScsiR3BiosIoPortWriteStr(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT uPort, const uint8_t *pbSrc,
                                                        uint32_t *pcTransfers, unsigned cb);
{
}
/** @callback_method_impl{FNIOMIOPORTINSTRING} */
static DECLCALLBACK(int) virtioScsiR3BiosIoPortReadStr(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT uPort, uint8_t *pbDst,
                                                       uint32_t *pcTransfers, unsigned cb);
{
}
#endif

/**
 * @interface_method_impl{PDMIMEDIAEXPORT,pfnIoReqStateChanged}
 */
static DECLCALLBACK(void) virtioScsiR3IoReqStateChanged(PPDMIMEDIAEXPORT pInterface, PDMMEDIAEXIOREQ hIoReq,
                                                      void *pvIoReqAlloc, PDMMEDIAEXIOREQSTATE enmState)
{

    RT_NOREF4(pInterface, hIoReq, pvIoReqAlloc, enmState);
    PVIRTIOSCSITARGET pTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, IMediaExPort);

    switch (enmState)
    {
        case PDMMEDIAEXIOREQSTATE_SUSPENDED:
        {
            /* Make sure the request is not accounted for so the VM can suspend successfully. */
            uint32_t cTasksActive = ASMAtomicDecU32(&pTarget->cReqsInProgress);
            if (!cTasksActive && pTarget->CTX_SUFF(pVirtioScsi)->fSignalIdle)
                PDMDevHlpAsyncNotificationCompleted(pTarget->CTX_SUFF(pVirtioScsi)->pDevInsR3);
            break;
        }
        case PDMMEDIAEXIOREQSTATE_ACTIVE:
            /* Make sure the request is accounted for so the VM suspends only when the request is complete. */
            ASMAtomicIncU32(&pTarget->cReqsInProgress);
            break;
        default:
            AssertMsgFailed(("Invalid request state given %u\n", enmState));
    }
}

/**
 * @interface_method_impl{PDMIMEDIAEXPORT,pfnIoReqCopyFromBuf}
 */
static DECLCALLBACK(int) virtioScsiR3IoReqCopyFromBuf(PPDMIMEDIAEXPORT pInterface, PDMMEDIAEXIOREQ hIoReq,
                                                    void *pvIoReqAlloc, uint32_t offDst, PRTSGBUF pSgBuf,
                                                    size_t cbCopy)
{
    RT_NOREF(hIoReq);
    RT_NOREF(pInterface);

    PVIRTIOSCSIREQ pReq = (PVIRTIOSCSIREQ)pvIoReqAlloc;

    /** DrvSCSI.cpp, that issues this callback, just sticks one segment in the buffer */
    memcpy(pReq->pbDataIn + offDst, pSgBuf->paSegs[0].pvSeg, cbCopy);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIMEDIAEXPORT,pfnIoReqCopyToBuf}
 */
static DECLCALLBACK(int) virtioScsiR3IoReqCopyToBuf(PPDMIMEDIAEXPORT pInterface, PDMMEDIAEXIOREQ hIoReq,
                                                  void *pvIoReqAlloc, uint32_t offSrc, PRTSGBUF pSgBuf,
                                                  size_t cbCopy)
{
    RT_NOREF(hIoReq);
    RT_NOREF(pInterface);

    PVIRTIOSCSIREQ pReq = (PVIRTIOSCSIREQ)pvIoReqAlloc;
    /** DrvSCSI.cpp, that issues this callback, just sticks one segment in the buffer */

    memcpy(pSgBuf->paSegs[0].pvSeg, pReq->pbDataOut + offSrc, cbCopy);

    return VINF_SUCCESS;
}

static int virtioScsiSendEvent(PVIRTIOSCSI pThis, uint16_t uTarget, uint32_t uEventType, uint32_t uReason)
{
    PVIRTIOSCSITARGET pTarget = &pThis->aTargetInstances[uTarget];

    VIRTIOSCSI_EVENT_T event = { uEventType, { 0 }, uReason };
    SET_LUN_BUF(pTarget->iLUN, 0, event.uLUN);

    switch(uEventType)
    {
        case VIRTIOSCSI_T_NO_EVENT:
            if (uEventType & VIRTIOSCSI_T_EVENTS_MISSED)
                LogFunc(("LUN: %s Warning driver that events were missed\n", event.uLUN));
            else
                LogFunc(("LUN: %s Warning driver event info it queued is shorter than configured\n", event.uLUN));
            break;
        case VIRTIOSCSI_T_TRANSPORT_RESET:
            switch(uReason)
            {
                case VIRTIOSCSI_EVT_RESET_REMOVED:
                    LogFunc(("LUN: %s Target or LUN removed\n", event.uLUN));
                    break;
                case VIRTIOSCSI_EVT_RESET_RESCAN:
                    LogFunc(("LUN: %s Target or LUN added\n", event.uLUN));
                    break;
                case VIRTIOSCSI_EVT_RESET_HARD:
                    LogFunc(("LUN: %s Target was reset\n", event.uLUN));
                    break;
            }
            break;
        case VIRTIOSCSI_T_ASYNC_NOTIFY:
            char szTypeText[128];
            virtioGetControlAsyncMaskText(szTypeText, sizeof(szTypeText), uReason);
            LogFunc(("LUN: %s Delivering subscribed async notification %s\n", event.uLUN, szTypeText));
            break;
        case VIRTIOSCSI_T_PARAM_CHANGE:
            LogFunc(("LUN: %s PARAM_CHANGE sense code: 0x%x sense qualifier: 0x%x\n",
                        event.uLUN, uReason & 0xff, (uReason >> 8) & 0xff));
            break;
        default:
            LogFunc(("LUN: %s Unknown event type: %d, ignoring\n", event.uLUN, uEventType));
            return VINF_SUCCESS;
    }

    if (virtioQueueIsEmpty(pThis->hVirtio, EVENTQ_IDX))
    {
        LogFunc(("eventq is empty, events missed!\n"));
        ASMAtomicWriteBool(&pThis->fEventsMissed, true);
        return VINF_SUCCESS;
    }

    int rc = virtioQueueGet(pThis->hVirtio, EVENTQ_IDX, true, NULL, NULL);
    AssertRC(rc);

    RTSGBUF reqSegBuf;
    RTSGSEG aReqSegs[] = { { &event, sizeof(event) } };
    RTSgBufInit(&reqSegBuf, aReqSegs, sizeof(aReqSegs) / sizeof(RTSGSEG));

    rc = virtioQueuePut (pThis->hVirtio, EVENTQ_IDX, &reqSegBuf, true);
    AssertRC(rc);

    rc = virtioQueueSync(pThis->hVirtio, EVENTQ_IDX);
    AssertRC(rc);

    return VINF_SUCCESS;

}
/** TBD: VirtIO 1.0 spec 5.6.6.1.1 requires some request actions on reset that are
 *       not implemented.  Specifically either canceling outstanding I/O or
 *       returning VIRTIOSCSI_S_FAILURE for those requests.  Since there's no
 *       way to cancel I/O on VSCSI at this time the only the only other
 *       possibility is to wait for the outstanding request count to drop
 *       and return the failure code for any-and-all until that's done before
 *       allowing a reset to continue.
 *
 *       In the absence of active I/O farmed out to VSCSI
 *       the device handles a guest driver unload/reload gracefully and has
 *       been tested.
 */
static int virtioScsiReqFinish(PVIRTIOSCSI pThis, PVIRTIOSCSIREQ pReq, int rcReq)
{
    PVIRTIOSCSITARGET pTarget = pReq->pTarget;
    PPDMIMEDIAEX pIMediaEx = pTarget->pDrvMediaEx;

    ASMAtomicDecU32(&pTarget->cReqsInProgress);

    size_t cbResidual = 0, cbXfer = 0;
    int rc = pIMediaEx->pfnIoReqQueryResidual(pIMediaEx, pReq->hIoReq, &cbResidual);
    AssertRC(rc);

    rc = pIMediaEx->pfnIoReqQueryXferSize(pIMediaEx, pReq->hIoReq, &cbXfer);
    AssertRC(rc);

    #define SHOULD_FIX_VSCSI_TO_RETURN_SENSE_LEN 32
    struct REQ_RESP_HDR respHdr = { 0 };
    respHdr.uSenseLen = pReq->uStatus == SCSI_STATUS_CHECK_CONDITION ? SHOULD_FIX_VSCSI_TO_RETURN_SENSE_LEN : 0;
    respHdr.uResidual = cbResidual;
    respHdr.uStatus   = pReq->uStatus;

    /** VirtIO 1.0 spec 5.6.6.1.1 says device MUST return a VirtIO response byte value.
     *  Some are returned during the submit phase, and a few are not mapped at all,
     *  wherein anything that can't map specifically gets mapped to VIRTIOSCSI_S_FAILURE */
    switch(rcReq)
    {
        case SCSI_STATUS_OK:
            if (pReq->uStatus != SCSI_STATUS_CHECK_CONDITION)
            {
                respHdr.uResponse = VIRTIOSCSI_S_OK;
                break;
            }
            /* fallthrough */
        case SCSI_STATUS_BUSY:
            respHdr.uResponse = VIRTIOSCSI_S_BUSY;
            break;
        case SCSI_STATUS_DATA_UNDEROVER_RUN:
            respHdr.uResponse = VIRTIOSCSI_S_OVERRUN;
            break;
        case SCSI_STATUS_TASK_ABORTED:
            respHdr.uResponse = VIRTIOSCSI_S_ABORTED;
            break;
        case SCSI_STATUS_CHECK_CONDITION:
            {
                uint8_t uSenseKey = pReq->pbSense[2];
                switch (uSenseKey)
                {
                    case SCSI_SENSE_ABORTED_COMMAND:
                        respHdr.uResponse = VIRTIOSCSI_S_ABORTED;
                        break;
                    case SCSI_SENSE_COPY_ABORTED:
                        respHdr.uResponse = VIRTIOSCSI_S_ABORTED;
                        break;
                    case SCSI_SENSE_UNIT_ATTENTION:
                        respHdr.uResponse = VIRTIOSCSI_S_TARGET_FAILURE;
                        break;
                    case SCSI_SENSE_HARDWARE_ERROR:
                        respHdr.uResponse = VIRTIOSCSI_S_TARGET_FAILURE;
                        break;
                    case SCSI_SENSE_NOT_READY:
                        respHdr.uResponse = VIRTIOSCSI_S_BUSY; /* e.g. try again */
                        break;
                    default:
                        respHdr.uResponse = VIRTIOSCSI_S_FAILURE;
                        break;
                }
            }
            break;

        default:
            respHdr.uResponse = VIRTIOSCSI_S_FAILURE;
            break;
    }

    Log2Func(("status: %s,   response: (%x) %s\n",
              SCSIStatusText(pReq->uStatus), respHdr.uResponse, virtioGetReqRespText(respHdr.uResponse)));

    if (RT_FAILURE(rcReq))
        Log2Func(("rcReq:  %s\n", RTErrGetDefine(rcReq)));

    Log3Func(("status:%02x/resp:%02x, xfer=%d, residual: %u, sense (len=%d, alloc=%d)\n",
              pReq->uStatus, respHdr.uResponse, cbXfer, cbResidual,
              respHdr.uSenseLen, pThis->virtioScsiConfig.uSenseSize));

    if (respHdr.uSenseLen && LogIs2Enabled())
    {
        Log2Func(("Sense: %s\n", SCSISenseText(pReq->pbSense[2])));
        Log2Func(("Sense Ext3: %s\n", SCSISenseExtText(pReq->pbSense[12], pReq->pbSense[13])));
    }

    int cSegs = 0;


//    Assert(pReq->cbDataIn >= cbXfer);
    Assert(pReq->pbSense != NULL);

    RTSGSEG aReqSegs[4];
    aReqSegs[cSegs].pvSeg   = &respHdr;
    aReqSegs[cSegs++].cbSeg = sizeof(respHdr);

    aReqSegs[cSegs].pvSeg   = pReq->pbSense;
    aReqSegs[cSegs++].cbSeg = pReq->cbSense; /* VirtIO 1.0 spec 5.6.4/5.6.6.1 */

    if (pReq->cbPiIn)
    {
        aReqSegs[cSegs].pvSeg   = pReq->pbPiIn;
        aReqSegs[cSegs++].cbSeg = pReq->cbPiIn;
    }
    if (pReq->cbDataIn)
    {
        aReqSegs[cSegs].pvSeg   = pReq->pbDataIn;
        aReqSegs[cSegs++].cbSeg = cbXfer;
    }
    RTSGBUF reqSegBuf;
    RTSgBufInit(&reqSegBuf, aReqSegs, cSegs);

    /**
     * Fill in the request queue current descriptor chain's IN queue entry/entries
     * (phys. memory) with the Req response data in virtual memory.
     */
    size_t cbReqSgBuf = RTSgBufCalcTotalLength(&reqSegBuf);
    size_t cbInSgBuf  = RTSgBufCalcTotalLength(pReq->pInSgBuf);
    AssertMsgReturn(cbReqSgBuf <= cbInSgBuf,
                   ("Guest expected less req data (space needed: %d, avail: %d)\n", cbReqSgBuf, cbInSgBuf),
                   VERR_BUFFER_OVERFLOW);

    /**
     * Following doesn't put up memory barrier (fence).
     * VirtIO 1.0 Spec requires mem. barrier for ctrl cmds
     * but doesn't mention fences in regard to requests. */
    virtioQueuePut(pThis->hVirtio, pReq->qIdx, &reqSegBuf, true /* fFence TBD */);
    virtioQueueSync(pThis->hVirtio, pReq->qIdx);

    Log(("-----------------------------------------------------------------------------------------\n"));

    RTMemFree(pReq->pbSense);
    RTMemFree(pReq->pbDataIn);
    RTMemFree(pReq->pbPiIn);
    RTMemFree(pReq->pVirtqReq);

    pIMediaEx->pfnIoReqFree(pIMediaEx, pReq->hIoReq);

    if (pTarget->cReqsInProgress == 0 && pThis->fSignalIdle)
        PDMDevHlpAsyncNotificationCompleted(pThis->pDevInsR3);

    return VINF_SUCCESS;
}

/**
 * This is called to complete a request immediately
 *
 * @param pThis     - PDM driver instance state
 * @param qIdx      - Queue index
 * @param pRespHdr  - Response header
 * @param pbSense   - Pointer to sense buffer or NULL if none.
 *
 * @returns virtual box status code
 */
static int virtioScsiReqFinish(PVIRTIOSCSI pThis, uint16_t qIdx, struct REQ_RESP_HDR *pRespHdr, uint8_t *pbSense)
{
    uint8_t *abSenseBuf = (uint8_t *)RTMemAllocZ(pThis->virtioScsiConfig.uSenseSize);
    AssertReturn(abSenseBuf, VERR_NO_MEMORY);

    Log2Func(("status: %s    response: %s\n",
              SCSIStatusText(pRespHdr->uStatus),  virtioGetReqRespText(pRespHdr->uResponse)));

    RTSGSEG aReqSegs[2];
    aReqSegs[0].cbSeg = sizeof(pRespHdr);
    aReqSegs[0].pvSeg = pRespHdr;
    aReqSegs[1].cbSeg = pThis->virtioScsiConfig.uSenseSize;
    aReqSegs[1].pvSeg = abSenseBuf;

    if (pbSense && pRespHdr->uSenseLen)
        memcpy(abSenseBuf, pbSense, pRespHdr->uSenseLen);
    else
        pRespHdr->uSenseLen = 0;

    RTSGBUF reqSegBuf;
    RTSgBufInit(&reqSegBuf, aReqSegs, RT_ELEMENTS(aReqSegs));

    virtioQueuePut(pThis->hVirtio, qIdx, &reqSegBuf, true /* fFence */);
    virtioQueueSync(pThis->hVirtio, qIdx);

    RTMemFree(abSenseBuf);

    Log(("---------------------------------------------------------------------------------\n"));

    return VINF_SUCCESS;
}

static int virtioScsiReqSubmit(PVIRTIOSCSI pThis, uint16_t qIdx, PRTSGBUF pInSgBuf, PRTSGBUF pOutSgBuf)
{
    AssertMsgReturn(pOutSgBuf->cSegs, ("Req. has no OUT data, unexpected/TBD\n"), VERR_INVALID_PARAMETER);

    size_t cbOut = RTSgBufCalcTotalLength(pOutSgBuf);
    size_t cbIn  = RTSgBufCalcTotalLength(pInSgBuf);

    AssertMsgReturn(cbOut >= sizeof(REQ_CMD_HDR) + pThis->virtioScsiConfig.uCdbSize,
                        ("Req too short"), VERR_BUFFER_UNDERFLOW);

    PVIRTIOSCSI_REQ_CMD_T pVirtqReq = (PVIRTIOSCSI_REQ_CMD_T)RTMemAlloc(cbOut);
    AssertReturn(pVirtqReq, VERR_NO_MEMORY);

    off_t cbOff = 0;
    size_t cbCopy = cbOut;
    while (cbCopy)
    {
        size_t cbSeg = cbCopy;
        RTGCPHYS GCPhys = (RTGCPHYS)RTSgBufGetNextSegment(pOutSgBuf, &cbSeg);
        PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), GCPhys, ((uint8_t *)pVirtqReq) + cbOff, cbSeg);
        cbCopy -= cbSeg;
        cbOff += cbSeg;
    }

    uint8_t  uTarget =  pVirtqReq->cmdHdr.uLUN[1];
    uint32_t uLUN    = (pVirtqReq->cmdHdr.uLUN[2] << 8 | pVirtqReq->cmdHdr.uLUN[3]) & 0x3fff;

    LogFunc(("[%s] (Target: %d LUN: %d)  CDB: %.*Rhxs\n",
        SCSICmdText(pVirtqReq->uCdb[0]), uTarget, uLUN,
        virtioScsiEstimateCdbLen(pVirtqReq->uCdb[0], pThis->virtioScsiConfig.uCdbSize), pVirtqReq->uCdb));

    Log3Func(("   id: %RX64, attr: %x, prio: %d, crn: %x\n",
        pVirtqReq->cmdHdr.uId, pVirtqReq->cmdHdr.uTaskAttr, pVirtqReq->cmdHdr.uPrio, pVirtqReq->cmdHdr.uCrn));

    /**
     * Calculate request offsets
     */
    size_t  cbPiIn = 0, cbPiOut = 0, cbPiHdr = 0;
    off_t   piOutOff = 0;

    if (pThis->fHasT10pi)
    {
        cbPiIn   = pVirtqReq->piHdr.uPiBytesOut;
        cbPiOut  = pVirtqReq->piHdr.uPiBytesIn;
        cbPiHdr  = sizeof(REQ_CMD_PI) + cbPiOut;
        piOutOff = sizeof(REQ_CMD_HDR) + pThis->virtioScsiConfig.uCdbSize + sizeof(REQ_CMD_PI);
    }

    off_t   uDataOutOff = sizeof(REQ_CMD_HDR)  + pThis->virtioScsiConfig.uCdbSize + cbPiHdr;
    off_t   uDataInOff  = sizeof(REQ_RESP_HDR) + pThis->virtioScsiConfig.uSenseSize + cbPiIn;
    uint8_t *pbDataOut = (uint8_t *)((uint64_t)pVirtqReq + uDataOutOff);
    size_t  cbDataOut = cbOut - uDataOutOff;
    size_t  cbDataIn  = cbIn  - uDataInOff;

    if (uTarget >= pThis->cTargets || uLUN != 0)
    {
        uint8_t abSense[] = { RT_BIT(7) | SCSI_SENSE_RESPONSE_CODE_CURR_FIXED,
                              0, SCSI_SENSE_ILLEGAL_REQUEST,
                              0, 0, 0, 0, 10, SCSI_ASC_LOGICAL_UNIT_NOT_SUPPORTED, 0, 0 };
        struct REQ_RESP_HDR respHdr = { 0 };
        respHdr.uSenseLen = sizeof(abSense);
        respHdr.uStatus   = SCSI_STATUS_CHECK_CONDITION;
        respHdr.uResponse = (uTarget > pThis->cTargets) ? VIRTIOSCSI_S_BAD_TARGET : VIRTIOSCSI_S_OK;
        respHdr.uResidual = cbDataOut + cbDataIn;
        virtioScsiReqFinish(pThis, qIdx, &respHdr, abSense);
        return VINF_SUCCESS;
    }

    PVIRTIOSCSITARGET pTarget = &pThis->aTargetInstances[uTarget];

    if (RT_UNLIKELY(!pTarget->fPresent))
    {
        Log2Func(("Error submitting request, target not present!!\n"));
        uint8_t abSense[] = { RT_BIT(7) | SCSI_SENSE_RESPONSE_CODE_CURR_FIXED,
                              0, SCSI_SENSE_NOT_READY, 0, 0, 0, 0, 10, 0, 0, 0 };
        struct REQ_RESP_HDR respHdr = { 0 };
        respHdr.uSenseLen = sizeof(abSense);
        respHdr.uStatus   = SCSI_STATUS_CHECK_CONDITION;
        respHdr.uResponse = VIRTIOSCSI_S_TARGET_FAILURE;
        respHdr.uResidual = cbDataIn + cbDataOut;
        virtioScsiReqFinish(pThis, qIdx, &respHdr, abSense);
        RTMemFree(pVirtqReq);
        return VINF_SUCCESS;
    }

    PDMMEDIAEXIOREQ   hIoReq = NULL;
    PVIRTIOSCSIREQ    pReq;
    PPDMIMEDIAEX      pIMediaEx = pTarget->pDrvMediaEx;

    int rc = pIMediaEx->pfnIoReqAlloc(pIMediaEx, &hIoReq, (void **)&pReq, 0 /* uIoReqId */,
                                  PDMIMEDIAEX_F_SUSPEND_ON_RECOVERABLE_ERR);

    AssertMsgRCReturn(rc, ("Failed to allocate I/O request, rc=%Rrc\n", rc), rc);

    if (pThis->fHasT10pi)
    {
        pReq->cbPiOut   = cbPiOut;
        pReq->pbPiOut   = (uint8_t *)((uint64_t)pVirtqReq + piOutOff);
        pReq->cbPiIn    = cbPiIn;
        pReq->pbPiIn    = (uint8_t *)RTMemAllocZ(cbPiIn);
        AssertMsgReturn(pReq->pbPiIn, ("Out of memory allocating pi_in buffer"),  VERR_NO_MEMORY);
    }

    pReq->hIoReq    = hIoReq;
    pReq->pTarget   = pTarget;
    pReq->qIdx      = qIdx;
    pReq->pbDataOut = pbDataOut;
    pReq->pVirtqReq = pVirtqReq;
    pReq->pInSgBuf  = pInSgBuf;
    pReq->cbSense   = pThis->virtioScsiConfig.uSenseSize;
    pReq->pbSense   = (uint8_t *)RTMemAllocZ(pReq->cbSense);
    AssertMsgReturn(pReq->pbSense,  ("Out of memory allocating sense buffer"),  VERR_NO_MEMORY);

    if (cbDataIn)
    {
        pReq->cbDataIn  = cbDataIn;
        pReq->pbDataIn  = (uint8_t *)RTMemAllocZ(cbDataIn);
        AssertMsgReturn(pReq->pbDataIn, ("Out of memory allocating datain buffer"), VERR_NO_MEMORY);
    }

    ASMAtomicIncU32(&pTarget->cReqsInProgress);

    rc = pIMediaEx->pfnIoReqSendScsiCmd(pIMediaEx, pReq->hIoReq, uLUN,
                                        pVirtqReq->uCdb, pThis->virtioScsiConfig.uCdbSize,
                                        PDMMEDIAEXIOREQSCSITXDIR_UNKNOWN, NULL, cbDataIn,
                                        pReq->pbSense, pReq->cbSense, NULL,
                                        &pReq->uStatus, 30 * RT_MS_1SEC);

    if (rc != VINF_PDM_MEDIAEX_IOREQ_IN_PROGRESS)
    {
        /**
         * This rc value from DrvSCSI/send SCSI cmd means the request failed early
         * (no mem, buf copy callback, request buffer creation, or req enqueue),
         * and not submitted to lower layers, error it out.
         */
        Log2Func(("Error submitting request!\n"));
        size_t cbResidual;
        pIMediaEx->pfnIoReqQueryResidual(pIMediaEx, pReq->hIoReq, &cbResidual);
        uint8_t uASC, uASCQ = 0;
        switch (rc)
        {
            case VERR_NO_MEMORY:
                uASC = SCSI_ASC_SYSTEM_RESOURCE_FAILURE;
                break;
            default:
                uASC = SCSI_ASC_INTERNAL_TARGET_FAILURE;
                break;
        }
        uint8_t abSense[] = { RT_BIT(7) | SCSI_SENSE_RESPONSE_CODE_CURR_FIXED,
                              0, SCSI_SENSE_VENDOR_SPECIFIC,
                              0, 0, 0, 0, 10, uASC, uASCQ, 0 };
        struct REQ_RESP_HDR respHdr = { 0 };
        respHdr.uSenseLen = sizeof(abSense);
        respHdr.uStatus   = SCSI_STATUS_CHECK_CONDITION;
        respHdr.uResponse = VIRTIOSCSI_S_FAILURE;
        respHdr.uResidual = cbDataIn + cbDataOut;
        virtioScsiReqFinish(pThis, qIdx, &respHdr, abSense);
        RTMemFree(pReq->pbSense);
        RTMemFree(pReq->pbDataIn);
        RTMemFree(pReq->pbPiIn);
        RTMemFree(pVirtqReq);
        pIMediaEx->pfnIoReqFree(pIMediaEx, pReq->hIoReq);
        return VINF_SUCCESS;
    }

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIMEDIAEXPORT,pfnIoReqCompleteNotify}
 */
static DECLCALLBACK(int) virtioScsiR3IoReqCompleteNotify(PPDMIMEDIAEXPORT pInterface, PDMMEDIAEXIOREQ hIoReq,
                                                       void *pvIoReqAlloc, int rcReq)
{
    RT_NOREF(hIoReq);
    PVIRTIOSCSITARGET pTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, IMediaExPort);
    virtioScsiReqFinish(pTarget->CTX_SUFF(pVirtioScsi), (PVIRTIOSCSIREQ)pvIoReqAlloc, rcReq);
    return VINF_SUCCESS;
}

static int virtioScsiCtrl(PVIRTIOSCSI pThis, uint16_t qIdx, PRTSGBUF pInSgBuf, PRTSGBUF pOutSgBuf)
{
    RT_NOREF(pThis);
    RT_NOREF(qIdx);
    RT_NOREF(pInSgBuf);

    /**
     * According to the VirtIO 1.0 SCSI Host device, spec, section 5.6.6.2, control packets are
     * extremely small, so more than one segment is highly unlikely but not a bug. Get the
     * the controlq sg buffer into virtual memory. */

    size_t cbOut = RTSgBufCalcTotalLength(pOutSgBuf);

    PVIRTIOSCSI_CTRL_T pScsiCtrl = (PVIRTIOSCSI_CTRL_T)RTMemAllocZ(cbOut);
    AssertMsgReturn(pScsiCtrl, ("Out of memory"), VERR_NO_MEMORY);

     /**
      * Get control command into virtual memory
      */
    off_t cbOff = 0;
    size_t cbSeg = 0;
    while (cbOut)
    {
        RTGCPHYS pvSeg = (RTGCPHYS)RTSgBufGetNextSegment(pOutSgBuf, &cbSeg);
        PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), pvSeg, pScsiCtrl + cbOff, cbSeg);
        cbOut -= cbSeg;
        cbOff += cbSeg;
    }

    uint8_t  uResponse = VIRTIOSCSI_S_OK;

    /**
     * Mask of events to tell guest driver this device supports
     * See VirtIO 1.0 specification section 5.6.6.2
     */
    uint32_t uSubscribedEvents  =
                            VIRTIOSCSI_EVT_ASYNC_POWER_MGMT
                          | VIRTIOSCSI_EVT_ASYNC_EXTERNAL_REQUEST
                          | VIRTIOSCSI_EVT_ASYNC_MEDIA_CHANGE
                          | VIRTIOSCSI_EVT_ASYNC_DEVICE_BUSY;

    RTSGBUF reqSegBuf;

    switch(pScsiCtrl->uType)
    {
        case VIRTIOSCSI_T_TMF: /* Task Management Functions */
        {
            PVIRTIOSCSI_CTRL_TMF_T pScsiCtrlTmf = (PVIRTIOSCSI_CTRL_TMF_T)pScsiCtrl;
            LogFunc(("%s, VirtIO LUN: %.8Rhxs\n%*sTask Mgt Function: %s (not yet implemented)\n",
                QUEUENAME(qIdx), pScsiCtrlTmf->uLUN,
                CBQUEUENAME(qIdx) + 18, "", virtioGetTMFTypeText(pScsiCtrlTmf->uSubtype)));

            switch(pScsiCtrlTmf->uSubtype)
            {
                case VIRTIOSCSI_T_TMF_ABORT_TASK:
                    uResponse = VIRTIOSCSI_S_FUNCTION_SUCCEEDED;
                    break;
                case VIRTIOSCSI_T_TMF_ABORT_TASK_SET:
                    uResponse = VIRTIOSCSI_S_FUNCTION_SUCCEEDED;
                    break;
                case VIRTIOSCSI_T_TMF_CLEAR_ACA:
                    uResponse = VIRTIOSCSI_S_FUNCTION_SUCCEEDED;
                    break;
                case VIRTIOSCSI_T_TMF_CLEAR_TASK_SET:
                    uResponse = VIRTIOSCSI_S_FUNCTION_SUCCEEDED;
                    break;
                case VIRTIOSCSI_T_TMF_I_T_NEXUS_RESET:
                    uResponse = VIRTIOSCSI_S_FUNCTION_SUCCEEDED;
                    break;
                case VIRTIOSCSI_T_TMF_LOGICAL_UNIT_RESET:
                    uResponse = VIRTIOSCSI_S_FUNCTION_SUCCEEDED;
                    break;
                case VIRTIOSCSI_T_TMF_QUERY_TASK:
                    uResponse = VIRTIOSCSI_S_FUNCTION_SUCCEEDED;
                    break;
                case VIRTIOSCSI_T_TMF_QUERY_TASK_SET:
                    uResponse = VIRTIOSCSI_S_FUNCTION_SUCCEEDED;
                    break;
                default:
                    LogFunc(("Unknown TMF type\n"));
                    uResponse = VIRTIOSCSI_S_FAILURE;
            }

            RTSGSEG aReqSegs[] = { { &uResponse,  sizeof(uResponse) } };
            RTSgBufInit(&reqSegBuf, aReqSegs, sizeof(aReqSegs) / sizeof(RTSGSEG));

            break;
        }
        case VIRTIOSCSI_T_AN_QUERY: /** Guest SCSI driver is querying supported async event notifications */
        {
            PVIRTIOSCSI_CTRL_AN_T pScsiCtrlAnQuery = (PVIRTIOSCSI_CTRL_AN_T)pScsiCtrl;

            char szTypeText[128];
            virtioGetControlAsyncMaskText(szTypeText, sizeof(szTypeText), pScsiCtrlAnQuery->uEventsRequested);

            Log3Func(("%s, VirtIO LUN: %.8Rhxs\n%*sAsync Query, types: %s\n",
                QUEUENAME(qIdx), pScsiCtrlAnQuery->uLUN, CBQUEUENAME(qIdx) + 30, "", szTypeText));

            uSubscribedEvents &= pScsiCtrlAnQuery->uEventsRequested;
            uResponse = VIRTIOSCSI_S_FUNCTION_COMPLETE;

            RTSGSEG aReqSegs[] = { { &uSubscribedEvents, sizeof(uSubscribedEvents) },  { &uResponse, sizeof(uResponse)  } };
            RTSgBufInit(&reqSegBuf, aReqSegs, sizeof(aReqSegs) / sizeof(RTSGSEG));

            break;
        }
        case VIRTIOSCSI_T_AN_SUBSCRIBE: /** Guest SCSI driver is subscribing to async event notification(s) */
        {
            PVIRTIOSCSI_CTRL_AN_T pScsiCtrlAnSubscribe = (PVIRTIOSCSI_CTRL_AN_T)pScsiCtrl;

            if (pScsiCtrlAnSubscribe->uEventsRequested & ~SUBSCRIBABLE_EVENTS)
                LogFunc(("Unsupported bits in event subscription event mask: 0x%x\n", pScsiCtrlAnSubscribe->uEventsRequested));

            char szTypeText[128];
            virtioGetControlAsyncMaskText(szTypeText, sizeof(szTypeText), pScsiCtrlAnSubscribe->uEventsRequested);

            Log3Func(("%s, VirtIO LUN: %.8Rhxs\n%*sAsync Subscribe, types: %s\n",
                QUEUENAME(qIdx), pScsiCtrlAnSubscribe->uLUN, CBQUEUENAME(qIdx) + 30, "", szTypeText));

            uSubscribedEvents &= pScsiCtrlAnSubscribe->uEventsRequested;
            pThis->uAsyncEvtsEnabled = uSubscribedEvents;

            /**
             * TBD: Verify correct status code if request mask is only partially fulfillable
             *      and confirm when to use 'complete' vs. 'succeeded' See VirtIO 1.0 spec section 5.6.6.2 and read SAM docs*/
            if (uSubscribedEvents == pScsiCtrlAnSubscribe->uEventsRequested)
                uResponse = VIRTIOSCSI_S_FUNCTION_SUCCEEDED;
            else
                uResponse = VIRTIOSCSI_S_FUNCTION_COMPLETE;

            RTSGSEG aReqSegs[] = { { &uSubscribedEvents, sizeof(uSubscribedEvents) },  { &uResponse, sizeof(uResponse)  } };
            RTSgBufInit(&reqSegBuf, aReqSegs, sizeof(aReqSegs) / sizeof(RTSGSEG));

            break;
        }
        default:
            LogFunc(("Unknown control type extracted from %s: %d\n", QUEUENAME(qIdx), pScsiCtrl->uType));

            uResponse = VIRTIOSCSI_S_FAILURE;

            RTSGSEG aReqSegs[] = { { &uResponse,  sizeof(uResponse) } };
            RTSgBufInit(&reqSegBuf, aReqSegs, sizeof(aReqSegs) / sizeof(RTSGSEG));
    }

    LogFunc(("Response code: %s\n", virtioGetCtrlRespText(uResponse)));
    virtioQueuePut (pThis->hVirtio, qIdx, &reqSegBuf, true);
    virtioQueueSync(pThis->hVirtio, qIdx);

    return VINF_SUCCESS;
}

/*
 * Unblock the worker thread so it can respond to a state change.
 *
 * @returns VBox status code.
 * @param   pDevIns     The pcnet device instance.
 * @param   pThread     The send thread.
 */
static DECLCALLBACK(int) virtioScsiR3WorkerWakeUp(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    RT_NOREF(pThread);
    uint16_t qIdx = ((uint64_t)pThread->pvUser) & 0xffff;
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    return SUPSemEventSignal(pThis->pSupDrvSession, pThis->aWorker[qIdx].hEvtProcess);
}

static int virtioScsiWorker(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{

    int rc;
    uint16_t qIdx = ((uint64_t)pThread->pvUser) & 0xffff;
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    PWORKER pWorker = &pThis->aWorker[qIdx];
    PRTSGBUF pInSgBuf;
    PRTSGBUF pOutSgBuf;

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        if (virtioQueueIsEmpty(pThis->hVirtio, qIdx))
        {
            /** Interlocks avoid missing alarm while going to sleep & notifier doesn't wake the awoken */
            ASMAtomicWriteBool(&pWorker->fSleeping, true);
            bool fNotificationSent = ASMAtomicXchgBool(&pWorker->fNotified, false);
            if (!fNotificationSent)
            {
                Log3Func(("%s worker sleeping...\n", QUEUENAME(qIdx)));
                Assert(ASMAtomicReadBool(&pWorker->fSleeping));
                rc = SUPSemEventWaitNoResume(pThis->pSupDrvSession, pWorker->hEvtProcess, RT_INDEFINITE_WAIT);
                AssertLogRelMsgReturn(RT_SUCCESS(rc) || rc == VERR_INTERRUPTED, ("%Rrc\n", rc), rc);
                if (RT_UNLIKELY(pThread->enmState != PDMTHREADSTATE_RUNNING))
                    break;
                Log3Func(("%s worker woken\n", QUEUENAME(qIdx)));
                ASMAtomicWriteBool(&pWorker->fNotified, false);
            }
            ASMAtomicWriteBool(&pWorker->fSleeping, false);
        }
        Log3Func(("fetching next descriptor chain from %s\n", QUEUENAME(qIdx)));
        rc = virtioQueueGet(pThis->hVirtio, qIdx, true, &pInSgBuf, &pOutSgBuf);
        if (rc == VERR_NOT_AVAILABLE)
        {
            Log3Func(("Nothing found in %s\n", QUEUENAME(qIdx)));
            continue;
        }

        AssertRC(rc);
        if (qIdx == CONTROLQ_IDX)
            virtioScsiCtrl(pThis, qIdx, pInSgBuf, pOutSgBuf);
        else
        {
            rc = virtioScsiReqSubmit(pThis, qIdx, pInSgBuf, pOutSgBuf);
            if (RT_FAILURE(rc))
            {
                LogRel(("Fatal error submitting req packet, resetting %Rrc", rc));
                /** TBD: MUST AT LEAD RETURN VIRTIO_SCSI_S_FAILURE for all pending I/O, Aborting is an option! */
                virtioResetAll(pThis->hVirtio);
            }
        }
    }
    return VINF_SUCCESS;
}


/*static void virtioScsiEventToClient(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{ } */


/**
 * Implementation invokes this to reset the VirtIO device
 */
static void virtioScsiDeviceReset(PVIRTIOSCSI pThis)
{
    pThis->virtioScsiConfig.uSenseSize = VIRTIOSCSI_SENSE_SIZE_DEFAULT;
    pThis->virtioScsiConfig.uCdbSize = VIRTIOSCSI_CDB_SIZE_DEFAULT;
    virtioResetAll(pThis->hVirtio);
}

static int virtioScsiR3CfgAccessed(PVIRTIOSCSI pThis, uint32_t uOffset,
                                    const void *pv, size_t cb, uint8_t fWrite)
{
    int rc = VINF_SUCCESS;
    if (MATCH_SCSI_CONFIG(uNumQueues))
    {
        SCSI_CONFIG_ACCESSOR_READONLY(uNumQueues);
    }
    else
    if (MATCH_SCSI_CONFIG(uSegMax))
    {
        SCSI_CONFIG_ACCESSOR_READONLY(uSegMax);
    }
    else
    if (MATCH_SCSI_CONFIG(uMaxSectors))
    {
        SCSI_CONFIG_ACCESSOR_READONLY(uMaxSectors);
    }
    else
    if (MATCH_SCSI_CONFIG(uCmdPerLun))
    {
        SCSI_CONFIG_ACCESSOR_READONLY(uCmdPerLun);
    }
    else
    if (MATCH_SCSI_CONFIG(uEventInfoSize))
    {
        SCSI_CONFIG_ACCESSOR_READONLY(uEventInfoSize);
    }
    else
    if (MATCH_SCSI_CONFIG(uSenseSize))
    {
        SCSI_CONFIG_ACCESSOR(uSenseSize);
    }
    else
    if (MATCH_SCSI_CONFIG(uCdbSize))
    {
        SCSI_CONFIG_ACCESSOR(uCdbSize);
    }
    else
    if (MATCH_SCSI_CONFIG(uMaxChannel))
    {
        SCSI_CONFIG_ACCESSOR_READONLY(uMaxChannel);
    }
    else
    if (MATCH_SCSI_CONFIG(uMaxTarget))
    {
        SCSI_CONFIG_ACCESSOR_READONLY(uMaxTarget);
    }
    else
    if (MATCH_SCSI_CONFIG(uMaxLun))
    {
        SCSI_CONFIG_ACCESSOR_READONLY(uMaxLun);
    }
    else
    {
        LogFunc(("Bad access by guest to virtio_scsi_config: uoff=%d, cb=%d\n", uOffset, cb));
        rc = VERR_ACCESS_DENIED;
    }
    return rc;
}



/**
 * virtio-scsi VirtIO Device-specific capabilities read callback
 * (other VirtIO capabilities and features are handled in VirtIO implementation)
 *
 * @param   pDevIns     The device instance.
 * @param   uOffset     Offset within device specific capabilities struct
 * @param   pv          Buffer in which to save read data
 * @param   cb          Number of bytes to read
 */
static DECLCALLBACK(int) virtioScsiR3DevCapRead(PPDMDEVINS pDevIns, uint32_t uOffset, const void *pv, size_t cb)
{
    int rc = VINF_SUCCESS;
    PVIRTIOSCSI  pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

    rc = virtioScsiR3CfgAccessed(pThis, uOffset, pv, cb, false);

    return rc;
}

/**
 * virtio-scsi VirtIO Device-specific capabilities read callback
 * (other VirtIO capabilities and features are handled in VirtIO implementation)
 *
 * @param   pDevIns     The device instance.
 * @param   uOffset     Offset within device specific capabilities struct
 * @param   pv          Buffer in which to save read data
 * @param   cb          Number of bytes to write
 */
static DECLCALLBACK(int) virtioScsiR3DevCapWrite(PPDMDEVINS pDevIns, uint32_t uOffset, const void *pv, size_t cb)
{
    int rc = VINF_SUCCESS;
    PVIRTIOSCSI  pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

    rc = virtioScsiR3CfgAccessed(pThis, uOffset, pv, cb, true);

    return rc;
}

DECLINLINE(void) virtioScsiReportEventsMissed(PVIRTIOSCSI pThis, uint16_t uTarget)
{
    virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_NO_EVENT | VIRTIOSCSI_T_EVENTS_MISSED, 0);
}

DECLINLINE(void) virtioScsiReportTargetRemoved(PVIRTIOSCSI pThis, uint16_t uTarget)
{
    if (pThis->fHasHotplug)
        virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_TRANSPORT_RESET,
                VIRTIOSCSI_EVT_RESET_REMOVED);
}

DECLINLINE(void) virtioScsiReportTargetAdded(PVIRTIOSCSI pThis, uint16_t uTarget)
{
    if (pThis->fHasHotplug)
        virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_TRANSPORT_RESET,
                VIRTIOSCSI_EVT_RESET_RESCAN);
}

DECLINLINE(void) virtioScsiReportTargetReset(PVIRTIOSCSI pThis, uint16_t uTarget)
{
    virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_TRANSPORT_RESET,
            VIRTIOSCSI_EVT_RESET_HARD);
}

DECLINLINE(void) virtioScsiReportOperChange(PVIRTIOSCSI pThis, uint16_t uTarget)
{
    if (pThis->uSubscribedEvents & VIRTIOSCSI_EVT_ASYNC_OPERATIONAL_CHANGE)
        virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_ASYNC_NOTIFY,
                VIRTIOSCSI_EVT_ASYNC_OPERATIONAL_CHANGE);
}

DECLINLINE(void) virtioScsiReportPowerMsg(PVIRTIOSCSI pThis, uint16_t uTarget)
{
    if (pThis->uSubscribedEvents & VIRTIOSCSI_EVT_ASYNC_POWER_MGMT)
        virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_ASYNC_NOTIFY,
                VIRTIOSCSI_EVT_ASYNC_POWER_MGMT);
}

DECLINLINE(void) virtioScsiReportExtReq(PVIRTIOSCSI pThis, uint16_t uTarget)
{
    if (pThis->uSubscribedEvents & VIRTIOSCSI_EVT_ASYNC_EXTERNAL_REQUEST)
        virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_ASYNC_NOTIFY,
                VIRTIOSCSI_EVT_ASYNC_EXTERNAL_REQUEST);
}

DECLINLINE(void) virtioScsiReportMediaChange(PVIRTIOSCSI pThis, uint16_t uTarget)
{
    if (pThis->uSubscribedEvents & VIRTIOSCSI_EVT_ASYNC_MEDIA_CHANGE)
        virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_ASYNC_NOTIFY,
                VIRTIOSCSI_EVT_ASYNC_MEDIA_CHANGE);
}

DECLINLINE(void) virtioScsiReportMultiHost(PVIRTIOSCSI pThis, uint16_t uTarget)
{
    if (pThis->uSubscribedEvents & VIRTIOSCSI_EVT_ASYNC_MULTI_HOST)
        virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_ASYNC_NOTIFY,
                VIRTIOSCSI_EVT_ASYNC_MULTI_HOST);
}

DECLINLINE(void) virtioScsiReportDeviceBusy(PVIRTIOSCSI pThis, uint16_t uTarget)
{
    if (pThis->uSubscribedEvents & VIRTIOSCSI_EVT_ASYNC_DEVICE_BUSY)
        virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_ASYNC_NOTIFY,
                VIRTIOSCSI_EVT_ASYNC_DEVICE_BUSY);
}


DECLINLINE(void) virtioScsiReportParamChange(PVIRTIOSCSI pThis, uint16_t uTarget, uint32_t uSenseCode, uint32_t uSenseQualifier)
{
    uint32_t uReason = uSenseQualifier << 8 | uSenseCode;
    virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_PARAM_CHANGE, uReason);

}

static DECLCALLBACK(void) virtioScsiNotified(VIRTIOHANDLE hVirtio, void *pClient, uint16_t qIdx)
{
    RT_NOREF(hVirtio);

    AssertReturnVoid(qIdx < VIRTIOSCSI_QUEUE_CNT);
    PVIRTIOSCSI pThis = (PVIRTIOSCSI)pClient;
    PWORKER pWorker = &pThis->aWorker[qIdx];


    RTLogFlush(RTLogDefaultInstanceEx(RT_MAKE_U32(0, UINT16_MAX)));

    if (qIdx == CONTROLQ_IDX || IS_REQ_QUEUE(qIdx))
    {
        Log3Func(("%s has available data\n", QUEUENAME(qIdx)));
        /** Wake queue's worker thread up if sleeping */
        if (!ASMAtomicXchgBool(&pWorker->fNotified, true))
        {
            if (ASMAtomicReadBool(&pWorker->fSleeping))
            {
                Log3Func(("waking %s worker.\n", QUEUENAME(qIdx)));
                int rc = SUPSemEventSignal(pThis->pSupDrvSession, pWorker->hEvtProcess);
                AssertRC(rc);
            }
        }
    }
    else if (qIdx == EVENTQ_IDX)
    {
        Log3Func(("Driver queued buffer(s) to %s\n"));
        if (ASMAtomicXchgBool(&pThis->fEventsMissed, false))
            virtioScsiReportEventsMissed(pThis, 0);
    }
    else
        LogFunc(("Unexpected queue idx (ignoring): %d\n", qIdx));
}

static DECLCALLBACK(void) virtioScsiStatusChanged(VIRTIOHANDLE hVirtio, void *pClient,  bool fVirtioReady)
{
    RT_NOREF(hVirtio);
    PVIRTIOSCSI pThis = (PVIRTIOSCSI)pClient;
    pThis->fVirtioReady = fVirtioReady;
    if (fVirtioReady)
    {
        LogFunc(("VirtIO ready\n-----------------------------------------------------------------------------------------\n"));
        uint64_t features = virtioGetNegotiatedFeatures(hVirtio);
        pThis->fHasT10pi     = features & VIRTIO_SCSI_F_T10_PI;
        pThis->fHasHotplug   = features & VIRTIO_SCSI_F_HOTPLUG;
        pThis->fHasInOutBufs = features & VIRTIO_SCSI_F_INOUT;
        pThis->fHasLunChange = features & VIRTIO_SCSI_F_CHANGE;
    }
    else
    {
        LogFunc(("VirtIO is resetting\n"));
        for (int i = 0; i < VIRTIOSCSI_QUEUE_CNT; i++)
            pThis->fQueueAttached[i] = false;
    }
}

/**
 * Turns on/off the write status LED.
 *
 * @param   pTarget         Pointer to the target device
 * @param   fOn             New LED state.
 */
void virtioScsiSetWriteLed(PVIRTIOSCSITARGET pTarget, bool fOn)
{
    LogFlow(("%s virtioSetWriteLed: %s\n", pTarget->pszLunName, fOn ? "on" : "off"));
    if (fOn)
        pTarget->led.Asserted.s.fWriting = pTarget->led.Actual.s.fWriting = 1;
    else
        pTarget->led.Actual.s.fWriting = fOn;
}

/**
 * Turns on/off the read status LED.
 *
 * @param   pTarget         Pointer to the device state structure.
 * @param   fOn             New LED state.
 */
void virtioScsiSetReadLed(PVIRTIOSCSITARGET pTarget, bool fOn)
{
    LogFlow(("%s virtioSetReadLed: %s\n", pTarget->pszLunName, fOn ? "on" : "off"));
    if (fOn)
        pTarget->led.Asserted.s.fReading = pTarget->led.Actual.s.fReading = 1;
    else
        pTarget->led.Actual.s.fReading = fOn;
}

/**
 * virtio-scsi debugger info callback.
 *
 * @param   pDevIns     The device instance.
 * @param   pHlp        The output helpers.
 * @param   pszArgs     The arguments.
 */
static DECLCALLBACK(void) virtioScsiR3Info(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    bool fVerbose = false;

    /* Parse arguments. */
    if (pszArgs)
        fVerbose = strstr(pszArgs, "verbose") != NULL;

    /* Show basic information. */
    pHlp->pfnPrintf(pHlp, "%s#%d: virtio-scsci ",
                    pDevIns->pReg->szName,
                    pDevIns->iInstance);
    pHlp->pfnPrintf(pHlp, "numTargets=%lu", pThis->cTargets);
}

/**
 * @interface_method_impl{PDMIMEDIAEXPORT,pfnMediumEjected}
 */
static DECLCALLBACK(void) virtioScsiR3MediumEjected(PPDMIMEDIAEXPORT pInterface)
{
    PVIRTIOSCSITARGET pTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, IMediaExPort);
    PVIRTIOSCSI pThis = pTarget->CTX_SUFF(pVirtioScsi);
    LogFunc(("LUN %d Ejected!\n", pTarget->iLUN));
    if (pThis->pMediaNotify)
        virtioScsiSetWriteLed(pTarget, false);
}

/** @callback_method_impl{FNSSMDEVLIVEEXEC}  */
static DECLCALLBACK(int) virtioScsiR3LiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    LogFunc(("callback"));
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    RT_NOREF(pThis);
    RT_NOREF(uPass);
    RT_NOREF(pSSM);
    return VINF_SSM_DONT_CALL_AGAIN;
}

/** @callback_method_impl{FNSSMDEVLOADEXEC}  */
static DECLCALLBACK(int) virtioScsiR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    LogFunc(("callback"));
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    RT_NOREF(pThis);
    RT_NOREF(uPass);
    RT_NOREF(pSSM);
    RT_NOREF(uVersion);
    return VINF_SSM_DONT_CALL_AGAIN;
}

/** @callback_method_impl{FNSSMDEVSAVEEXEC}  */
static DECLCALLBACK(int) virtioScsiR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    LogFunc(("callback"));
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

    RT_NOREF(pThis);
    RT_NOREF(pSSM);
    return VINF_SUCCESS;
}

/** @callback_method_impl{FNSSMDEVLOADDONE}  */
static DECLCALLBACK(int) virtioScsiR3LoadDone(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    LogFunc(("callback"));
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    RT_NOREF(pThis);
    RT_NOREF(pSSM);
    return VINF_SUCCESS;
}


/**
 * Callback employed by virtioScsiR3PDMReset.
 *
 * @returns true if we've quiesced, false if we're still working.
 * @param   pDevIns     The device instance.
 */
static DECLCALLBACK(bool) virtioScsiR3IsAsyncResetDone(PPDMDEVINS pDevIns)
{
    RT_NOREF(pDevIns);
    Log(("\n"));

    return true;
}

/**
 * @copydoc FNPDMDEVRESET
 */
static DECLCALLBACK(void) virtioScsiR3PDMReset(PPDMDEVINS pDevIns)
{
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    ASMAtomicWriteBool(&pThis->fSignalIdle, true);

    bool fIoInProgress = false;
    for (uint32_t i = 0; i < RT_ELEMENTS(pThis->aTargetInstances); i++)
    {
          PVIRTIOSCSITARGET pTarget = &pThis->aTargetInstances[i];
          if (pTarget->cReqsInProgress != 0)
          {
              fIoInProgress = true;
              break;
          }
    }
    if (!fIoInProgress)
        PDMDevHlpSetAsyncNotification(pDevIns, virtioScsiR3IsAsyncResetDone);
    else
        ASMAtomicWriteBool(&pThis->fSignalIdle, false);

    virtioScsiDeviceReset(pThis);
}

/**
 * Device relocation callback.
 *
 * When this callback is called the device instance data, and if the
 * device have a GC component, is being relocated, or/and the selectors
 * have been changed. The device must use the chance to perform the
 * necessary pointer relocations and data updates.
 *
 * Before the GC code is executed the first time, this function will be
 * called with a 0 delta so GC pointer calculations can be one in one place.
 *
 * @param   pDevIns     Pointer to the device instance.
 * @param   offDelta    The relocation delta relative to the old location.
 *
 * @remark  A relocation CANNOT fail.
 */
static DECLCALLBACK(void) virtioScsiR3Relocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    LogFunc(("Relocating virtio-scsi"));
    RT_NOREF(offDelta);
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

    pThis->pDevInsR3 = pDevIns;

    for (uint32_t i = 0; i < VIRTIOSCSI_MAX_TARGETS; i++)
    {
        PVIRTIOSCSITARGET pTarget = &pThis->aTargetInstances[i];
        pTarget->pVirtioScsiR3 = pThis;;
    }

    /**
     * Important: Forward to virtio framework!
     */
    virtioRelocate(pDevIns, offDelta);

}

static DECLCALLBACK(int) virtioScsiR3QueryDeviceLocation(PPDMIMEDIAPORT pInterface, const char **ppcszController,
                                                       uint32_t *piInstance, uint32_t *piLUN)
{
    PVIRTIOSCSITARGET pThisTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, IMediaPort);
    PPDMDEVINS pDevIns = pThisTarget->CTX_SUFF(pVirtioScsi)->CTX_SUFF(pDevIns);

    AssertPtrReturn(ppcszController, VERR_INVALID_POINTER);
    AssertPtrReturn(piInstance, VERR_INVALID_POINTER);
    AssertPtrReturn(piLUN, VERR_INVALID_POINTER);

    *ppcszController = pDevIns->pReg->szName;
    *piInstance = pDevIns->iInstance;
    *piLUN = pThisTarget->iLUN;

    return VINF_SUCCESS;
}

/**
 * Gets the pointer to the status LED of a unit.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   iLUN            The unit which status LED we desire.
 * @param   ppLed           Where to store the LED pointer.
 */
static DECLCALLBACK(int) virtioScsiR3TargetQueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    PVIRTIOSCSITARGET pTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, ILed);
    if (iLUN == 0)
    {
        *ppLed = &pTarget->led;
        Assert((*ppLed)->u32Magic == PDMLED_MAGIC);
        return VINF_SUCCESS;
    }
    return VERR_PDM_LUN_NOT_FOUND;
  }


/**
 * Gets the pointer to the status LED of a unit.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   iLUN            The unit which status LED we desire.
 * @param   ppLed           Where to store the LED pointer.
 */
static DECLCALLBACK(int) virtioScsiR3DeviceQueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    PVIRTIOSCSI pThis = RT_FROM_MEMBER(pInterface, VIRTIOSCSI, ILeds);
    if (iLUN < pThis->cTargets)
    {
        *ppLed = &pThis->aTargetInstances[iLUN].led;
        Assert((*ppLed)->u32Magic == PDMLED_MAGIC);
        return VINF_SUCCESS;
    }
    return VERR_PDM_LUN_NOT_FOUND;
}


/**
 * Memory mapped I/O Handler for read operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   GCPhysAddr  Physical address (in GC) where the read starts.
 * @param   pv          Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int) virtioScsiMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    RT_NOREF_PV(pDevIns); RT_NOREF_PV(pvUser); RT_NOREF_PV(GCPhysAddr); RT_NOREF_PV(pv); RT_NOREF_PV(cb);
    LogFunc(("Read from MMIO area\n"));
    return VINF_SUCCESS;
}

/**
 * Memory mapped I/O Handler for write operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   GCPhysAddr  Physical address (in GC) where the read starts.
 * @param   pv          Where to fetch the result.
 * @param   cb          Number of bytes to write.
 */
PDMBOTHCBDECL(int) virtioScsiMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{
    RT_NOREF_PV(pDevIns); RT_NOREF_PV(pvUser); RT_NOREF_PV(GCPhysAddr); RT_NOREF_PV(pv); RT_NOREF_PV(cb);
    LogFunc(("Write to MMIO area\n"));
    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{FNPCIIOREGIONMAP}
 */
static DECLCALLBACK(int) virtioScsiR3Map(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, uint32_t iRegion,
                                           RTGCPHYS GCPhysAddress, RTGCPHYS cb, PCIADDRESSSPACE enmType)
{
    RT_NOREF(pPciDev, iRegion);
    PVIRTIOSCSI  pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    int rc = VINF_SUCCESS;

    Assert(cb >= 32);

    switch (iRegion)
    {
        case 0:
            LogFunc(("virtio-scsi MMIO mapped at GCPhysAddr=%RGp cb=%RGp\n", GCPhysAddress, cb));

            /* We use the assigned size here, because we currently only support page aligned MMIO ranges. */
            rc = PDMDevHlpMMIORegister(pDevIns, GCPhysAddress, cb, NULL /*pvUser*/,
                                   IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                                   virtioScsiMMIOWrite, virtioScsiMMIORead,
                                   "virtio-scsi MMIO");
            pThis->GCPhysMMIOBase = RT_SUCCESS(rc) ? GCPhysAddress : 0;
            return rc;
        case 1:
            /* VirtIO 1.0 doesn't uses Port I/O (Virtio 0.95 e.g. "legacy", does) */
            AssertMsgFailed(("virtio-scsi: Port I/O not supported by this Host SCSI device\n"));
        default:
            AssertMsgFailed(("Invalid enmType=%d\n", enmType));
    }
    return VERR_GENERAL_FAILURE; /* Should never get here */
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) virtioScsiR3TargetQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
     PVIRTIOSCSITARGET pTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, IBase);
     PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE,        &pTarget->IBase);
     PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIAPORT,   &pTarget->IMediaPort);
     PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIAEXPORT, &pTarget->IMediaExPort);
     PDMIBASE_RETURN_INTERFACE(pszIID, PDMILEDPORTS,    &pTarget->ILed);
     return NULL;
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) virtioScsiR3DeviceQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PVIRTIOSCSI pThis = RT_FROM_MEMBER(pInterface, VIRTIOSCSI, IBase);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE,         &pThis->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMILEDPORTS,     &pThis->ILeds);

    return NULL;
}

/**
 * Detach notification.
 *
 * One harddisk at one port has been unplugged.
 * The VM is suspended at this point.
 *
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static DECLCALLBACK(void) virtioScsiR3Detach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    RT_NOREF(fFlags);
    PVIRTIOSCSI       pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    PVIRTIOSCSITARGET pTarget = &pThis->aTargetInstances[iLUN];

    LogFunc((""));

    AssertMsg(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
              ("virtio-scsi: Device does not support hotplugging\n"));

    /*
     * Zero some important members.
     */
    pTarget->fPresent    = false;
    pTarget->pDrvBase    = NULL;
}

/**
 * Attach command.
 *
 * This is called when we change block driver.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static DECLCALLBACK(int)  virtioScsiR3Attach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PVIRTIOSCSI       pThis   = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    PVIRTIOSCSITARGET pTarget = &pThis->aTargetInstances[iLUN];
    int rc;

    pThis->pDevInsR3    = pDevIns;
    pThis->pDevInsR0    = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC    = PDMDEVINS_2_RCPTR(pDevIns);

    AssertMsgReturn(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
                    ("virtio-scsi: Device does not support hotplugging\n"),
                    VERR_INVALID_PARAMETER);

    /* the usual paranoia */
    AssertRelease(!pTarget->pDrvBase);
    Assert(pTarget->iLUN == iLUN);

    /*
     * Try attach the SCSI driver and get the interfaces,
     * required as well as optional.
     */
    rc = PDMDevHlpDriverAttach(pDevIns, pTarget->iLUN, &pDevIns->IBase,
                               &pTarget->pDrvBase, (const char *)&pTarget->pszLunName);
    if (RT_SUCCESS(rc))
        pTarget->fPresent = true;
    else
        AssertMsgFailed(("Failed to attach %s. rc=%Rrc\n", pTarget->pszLunName, rc));

    if (RT_FAILURE(rc))
    {
        pTarget->fPresent = false;
        pTarget->pDrvBase = NULL;
    }
    return rc;
}

static DECLCALLBACK(int) virtioScsiDestruct(PPDMDEVINS pDevIns)
{
    /*
     * Check the versions here as well since the destructor is *always* called.
     */

    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

    PVIRTIOSCSI  pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

    for (int qIdx = 0; qIdx < VIRTQ_MAX_CNT; qIdx++)
    {
        PWORKER pWorker = &pThis->aWorker[qIdx];
        if (pWorker->hEvtProcess != NIL_SUPSEMEVENT)
        {
            SUPSemEventClose(pThis->pSupDrvSession, pWorker->hEvtProcess);
            pWorker->hEvtProcess = NIL_SUPSEMEVENT;
        }
    }
     return VINF_SUCCESS;
}

static DECLCALLBACK(int) virtioScsiConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg){

    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

    PVIRTIOSCSI  pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    int  rc = VINF_SUCCESS;

    pThis->pDevInsR3 = pDevIns;
    pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    pThis->pSupDrvSession = PDMDevHlpGetSupDrvSession(pDevIns);

    LogFunc(("PDM device instance: %d\n", iInstance));
    RTStrPrintf((char *)pThis->szInstance, sizeof(pThis->szInstance), "VIRTIOSCSI%d", iInstance);

    /*
     * Validate and read configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg,"NumTargets\0"
                                   "Bootable\0"
                                /* "GCEnabled\0"    TBD */
                                /* "R0Enabled\0"    TBD */
    ))
    return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("virtio-scsi configuration error: unknown option specified"));

    rc = CFGMR3QueryIntegerDef(pCfg, "NumTargets", &pThis->cTargets, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("virtio-scsi configuration error: failed to read NumTargets as integer"));
    LogFunc(("NumTargets=%d\n", pThis->cTargets));

    rc = CFGMR3QueryBoolDef(pCfg, "Bootable", &pThis->fBootable, true);
    if (RT_FAILURE(rc))
         return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("virtio-scsi configuration error: failed to read Bootable as boolean"));
    LogFunc(("Bootable=%RTbool (unimplemented)\n", pThis->fBootable));

    rc = CFGMR3QueryBoolDef(pCfg, "R0Enabled", &pThis->fR0Enabled, false);
    if (RT_FAILURE(rc))
         return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("virtio-scsi configuration error: failed to read R0Enabled as boolean"));

    rc = CFGMR3QueryBoolDef(pCfg, "RCEnabled", &pThis->fRCEnabled, false);
    if (RT_FAILURE(rc))
         return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("virtio-scsi configuration error: failed to read RCEnabled as boolean"));

    VIRTIOPCIPARAMS virtioPciParams, *pVirtioPciParams = &virtioPciParams;
    pVirtioPciParams->uDeviceId      = PCI_DEVICE_ID_VIRTIOSCSI_HOST;
    pVirtioPciParams->uClassBase     = PCI_CLASS_BASE_MASS_STORAGE;
    pVirtioPciParams->uClassSub      = PCI_CLASS_SUB_SCSI_STORAGE_CONTROLLER;
    pVirtioPciParams->uClassProg     = PCI_CLASS_PROG_UNSPECIFIED;
    pVirtioPciParams->uSubsystemId   = PCI_DEVICE_ID_VIRTIOSCSI_HOST;  /* Virtio 1.0 spec allows PCI Device ID here */
    pVirtioPciParams->uInterruptLine = 0x00;
    pVirtioPciParams->uInterruptPin  = 0x01;

    pThis->IBase.pfnQueryInterface = virtioScsiR3DeviceQueryInterface;

    /* Configure virtio_scsi_config that transacts via VirtIO implementation's Dev. Specific Cap callbacks */
    pThis->virtioScsiConfig.uNumQueues      = VIRTIOSCSI_REQ_QUEUE_CNT;
    pThis->virtioScsiConfig.uSegMax         = VIRTIOSCSI_MAX_SEG_COUNT;
    pThis->virtioScsiConfig.uMaxSectors     = VIRTIOSCSI_MAX_SECTORS_HINT;
    pThis->virtioScsiConfig.uCmdPerLun      = VIRTIOSCSI_MAX_COMMANDS_PER_LUN;
    pThis->virtioScsiConfig.uEventInfoSize  = sizeof(VIRTIOSCSI_EVENT_T); /* Spec says at least this size! */
    pThis->virtioScsiConfig.uSenseSize      = VIRTIOSCSI_SENSE_SIZE_DEFAULT;
    pThis->virtioScsiConfig.uCdbSize        = VIRTIOSCSI_CDB_SIZE_DEFAULT;
    pThis->virtioScsiConfig.uMaxChannel     = VIRTIOSCSI_MAX_CHANNEL_HINT;
    pThis->virtioScsiConfig.uMaxTarget      = pThis->cTargets;
    pThis->virtioScsiConfig.uMaxLun         = VIRTIOSCSI_MAX_LUN;

    rc = virtioConstruct(pDevIns, pThis, &pThis->hVirtio, pVirtioPciParams, pThis->szInstance,
                         VIRTIOSCSI_HOST_SCSI_FEATURES_OFFERED,
                         virtioScsiR3DevCapRead,
                         virtioScsiR3DevCapWrite,
                         virtioScsiStatusChanged,
                         virtioScsiNotified,
                         virtioScsiR3LiveExec,
                         virtioScsiR3SaveExec,
                         virtioScsiR3LoadExec,
                         virtioScsiR3LoadDone,
                         sizeof(VIRTIOSCSI_CONFIG_T) /* cbDevSpecificCap */,
                         (void *)&pThis->virtioScsiConfig /* pDevSpecificCap */);

    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi: failed to initialize VirtIO"));


    RTStrCopy((char *)pThis->szQueueNames[CONTROLQ_IDX], VIRTIO_MAX_QUEUE_NAME_SIZE, "controlq");
    RTStrCopy((char *)pThis->szQueueNames[EVENTQ_IDX],   VIRTIO_MAX_QUEUE_NAME_SIZE, "eventq");
    for (uint16_t qIdx = VIRTQ_REQ_BASE; qIdx < VIRTQ_REQ_BASE + VIRTIOSCSI_REQ_QUEUE_CNT; qIdx++)
        RTStrPrintf((char *)pThis->szQueueNames[qIdx], VIRTIO_MAX_QUEUE_NAME_SIZE,
            "requestq<%d>", qIdx - VIRTQ_REQ_BASE);

    /**
     * Create one worker per incoming-work-related queue (eventq is outgoing status to guest,
     * wherein guest is supposed to keep the queue loaded-up with buffer vectors the host
     * can quickly fill-in send back). Should be light-duty and fast enough to be handled on
     * requestq or controlq thread.  The Linux virtio_scsi driver limits the number of request
     * queues to MIN(<# Guest CPUs>, <Device's req queue max>), so queue count is ultimately
     * constrained from host side at negotiation time and initialization and later through
     * bounds-checking.
     */
    for (uint16_t qIdx = 0; qIdx < VIRTIOSCSI_QUEUE_CNT; qIdx++)
    {
        rc = virtioQueueAttach(pThis->hVirtio, qIdx, QUEUENAME(qIdx));
        AssertMsgReturn(rc == VINF_SUCCESS, ("Failed to attach queue %s\n", QUEUENAME(qIdx)), rc);
        pThis->fQueueAttached[qIdx] = (rc == VINF_SUCCESS);

        if (qIdx == CONTROLQ_IDX || IS_REQ_QUEUE(qIdx))
        {
            rc = PDMDevHlpThreadCreate(pDevIns, &pThis->aWorker[qIdx].pThread,
                                       (void *)(uint64_t)qIdx, virtioScsiWorker,
                                       virtioScsiR3WorkerWakeUp, 0, RTTHREADTYPE_IO, QUEUENAME(qIdx));
            if (rc != VINF_SUCCESS)
            {
                LogRel(("Error creating thread for Virtual Queue %s\n", QUEUENAME(qIdx)));
                return rc;
            }

            rc = SUPSemEventCreate(pThis->pSupDrvSession, &pThis->aWorker[qIdx].hEvtProcess);
            if (RT_FAILURE(rc))
                return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                     N_("LsiLogic: Failed to create SUP event semaphore"));
         }
    }

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, VIRTIOSCSI_REGION_MEM_IO, 32,
                                      PCI_ADDRESS_SPACE_MEM, virtioScsiR3Map);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi: cannot register PCI mmio address space"));

#ifdef BOOTABLE_SUPPORT_TBD
    if (fBootable)
    {
        /* Register I/O port space for BIOS access. */
        rc = PDMDevHlpIOPortRegister(pDevIns, VIRTIOSCSI_BIOS_IO_PORT, 4, NULL,
                                     virtioScsiR3BiosIoPortWrite, virtioScsiR3BiosIoPortRead,
                                     virtioScsiR3BiosIoPortWriteStr, virtioScsiR3BiosIoPortReadStr,
                                     "virtio-scsi BIOS");
        if (RT_FAILURE(rc))
            return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi cannot register BIOS I/O handlers"));
    }
#endif

    /* Initialize per device instance. */
    for (RTUINT iLUN = 0; iLUN < VIRTIOSCSI_MAX_TARGETS; iLUN++)
    {
        PVIRTIOSCSITARGET pTarget = &pThis->aTargetInstances[iLUN];

        if (RTStrAPrintf(&pTarget->pszLunName, "VSCSILUN%u", iLUN) < 0)
            AssertLogRelFailedReturn(VERR_NO_MEMORY);

        /* Initialize static parts of the device. */
        pTarget->iLUN = iLUN;
        pTarget->pVirtioScsiR3 = pThis;

        pTarget->IBase.pfnQueryInterface                 = virtioScsiR3TargetQueryInterface;

        /* IMediaPort and IMediaExPort interfaces provide callbacks for VD media and downstream driver access */
        pTarget->IMediaPort.pfnQueryDeviceLocation       = virtioScsiR3QueryDeviceLocation;
        pTarget->IMediaExPort.pfnIoReqCompleteNotify     = virtioScsiR3IoReqCompleteNotify;
        pTarget->IMediaExPort.pfnIoReqCopyFromBuf        = virtioScsiR3IoReqCopyFromBuf;
        pTarget->IMediaExPort.pfnIoReqCopyToBuf          = virtioScsiR3IoReqCopyToBuf;
        pTarget->IMediaExPort.pfnIoReqStateChanged       = virtioScsiR3IoReqStateChanged;
        pTarget->IMediaExPort.pfnMediumEjected           = virtioScsiR3MediumEjected;
        pTarget->IMediaExPort.pfnIoReqQueryBuf           = NULL; /* When used avoids copyFromBuf CopyToBuf*/
        pTarget->IMediaExPort.pfnIoReqQueryDiscardRanges = NULL;


        pTarget->IBase.pfnQueryInterface                 = virtioScsiR3TargetQueryInterface;
        pTarget->ILed.pfnQueryStatusLed                  = virtioScsiR3TargetQueryStatusLed;
        pThis->ILeds.pfnQueryStatusLed                   = virtioScsiR3DeviceQueryStatusLed;
        pTarget->led.u32Magic                            = PDMLED_MAGIC;

        LogFunc(("Attaching LUN: %s\n", pTarget->pszLunName));

        AssertReturn(iLUN < RT_ELEMENTS(pThis->aTargetInstances), VERR_PDM_NO_SUCH_LUN);
        rc = PDMDevHlpDriverAttach(pDevIns, iLUN, &pTarget->IBase, &pTarget->pDrvBase, (const char *)&pTarget->pszLunName);
        if (RT_SUCCESS(rc))
        {
            pTarget->fPresent = true;

            /* DrvSCSI.cpp currently implements the IMedia and IMediaEx interfaces, so those
             * are the interfaces that will be used to pass SCSI requests down to the
             * DrvSCSI driver to eventually make it down to the VD layer */
            pTarget->pDrvMedia = PDMIBASE_QUERY_INTERFACE(pTarget->pDrvBase, PDMIMEDIA);
            AssertMsgReturn(VALID_PTR(pTarget->pDrvMedia),
                         ("virtio-scsi configuration error: LUN#%d missing basic media interface!\n", pTarget->iLUN),
                         VERR_PDM_MISSING_INTERFACE);

            /* Get the extended media interface. */
            pTarget->pDrvMediaEx = PDMIBASE_QUERY_INTERFACE(pTarget->pDrvBase, PDMIMEDIAEX);
            AssertMsgReturn(VALID_PTR(pTarget->pDrvMediaEx),
                         ("virtio-scsi configuration error: LUN#%d missing extended media interface!\n", pTarget->iLUN),
                         VERR_PDM_MISSING_INTERFACE);

            rc = pTarget->pDrvMediaEx->pfnIoReqAllocSizeSet(pTarget->pDrvMediaEx, sizeof(VIRTIOSCSIREQ));
            AssertMsgReturn(VALID_PTR(pTarget->pDrvMediaEx),
                         ("virtio-scsi configuration error: LUN#%u: Failed to set I/O request size!\n", pTarget->iLUN),
                         rc);

        }
        else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        {
            pTarget->fPresent = false;
            pTarget->pDrvBase = NULL;
            rc = VINF_SUCCESS;
            Log(("virtio-scsi: no driver attached to device %s\n", pTarget->pszLunName));
        }
        else
        {
            AssertLogRelMsgFailed(("virtio-scsi: Failed to attach %s\n", pTarget->pszLunName));
            return rc;
        }
    }

    /* Status driver */
    PPDMIBASE pUpBase;
    rc = PDMDevHlpDriverAttach(pDevIns, PDM_STATUS_LUN, &pThis->IBase, &pUpBase, "Status Port");
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to attach the status LUN"));

    /*
     * Register the debugger info callback.
     */
    char szTmp[128];
    RTStrPrintf(szTmp, sizeof(szTmp), "%s%d", pDevIns->pReg->szName, pDevIns->iInstance);
    PDMDevHlpDBGFInfoRegister(pDevIns, szTmp, "virtio-scsi info", virtioScsiR3Info);

    return rc;
}

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceVirtioSCSI =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "virtio-scsi",
#ifdef VIRTIOSCSI_GC_SUPPORT
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
#else
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS,
#endif
    /* .fClass = */                 PDM_DEVREG_CLASS_MISC,
    /* .cMaxInstances = */          ~0U,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(VIRTIOSCSI),
    /* .cbInstanceCC = */           0,
    /* .cbInstanceRC = */           0,
    /* .uReserved1 = */             0,
    /* .pszDescription = */         "Virtio Host SCSI.\n",
#if defined(IN_RING3)
    /* .pszRCMod = */               "VBoxDDRC.rc",
    /* .pszR0Mod = */               "VBoxDDR0.r0",
    /* .pfnConstruct = */           virtioScsiConstruct,
    /* .pfnDestruct = */            virtioScsiDestruct,
    /* .pfnRelocate = */            virtioScsiR3Relocate,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               virtioScsiR3PDMReset,
    /* .pfnSuspend = */             NULL,
    /* .pfnResume = */              NULL,
    /* .pfnAttach = */              virtioScsiR3Attach,
    /* .pfnDetach = */              virtioScsiR3Detach,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        NULL,
    /* .pfnPowerOff = */            NULL,
    /* .pfnSoftReset = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RING0)
    /* .pfnEarlyConstruct = */      NULL,
    /* .pfnConstruct = */           NULL,
    /* .pfnDestruct = */            NULL,
    /* .pfnFinalDestruct = */       NULL,
    /* .pfnRequest = */             NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RC)
    /* .pfnConstruct = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#else
# error "Not in IN_RING3, IN_RING0 or IN_RC!"
#endif
    /* .u32VersionEnd = */          PDM_DEVREG_VERSION
};

