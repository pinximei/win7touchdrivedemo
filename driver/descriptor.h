// descriptor.h -- Win7 multi-touch HID descriptor.
//
// Critical: USAGE(Finger) is declared ONCE at the Application Collection
// level, then 10 separate Logical Collections follow without their own
// USAGE(Finger). win32k requires this exact shape for multi-touch dispatch;
// putting USAGE(Finger) inside each Logical Collection prevents WM_TOUCH.
//
// PHYSICAL_MAX must equal LOGICAL_MAX (32767) — win32k refuses to subscribe
// to COL01 input reports when PHYSICAL_MAX=0, observed as Service=(empty)
// on the Touch Screen TLC and frames=0 in WM_TOUCH receivers.
#pragma once

#define HIDTOUCH_REPORT_ID_INPUT      0x01
#define HIDTOUCH_REPORT_ID_MAXCOUNT   0x02
#define HIDTOUCH_REPORT_ID_CONFIG     0x03

#pragma pack(push, 1)

// 8 bytes per contact: Status + ContactId + X + Y + Width + Height
typedef struct _HIDTOUCH_CONTACT_REPORT {
    UCHAR  Status;        // bit0=Tip, bit1=InRange, bit2=Confidence
    UCHAR  ContactId;
    USHORT X;
    USHORT Y;
    USHORT Width;
    USHORT Height;
} HIDTOUCH_CONTACT_REPORT;

typedef struct _HIDTOUCH_INPUT_REPORT {
    UCHAR                   ReportId;
    HIDTOUCH_CONTACT_REPORT Contacts[10];
    UCHAR                   ContactCount;
} HIDTOUCH_INPUT_REPORT;

typedef struct _HIDTOUCH_MAXCOUNT_REPORT {
    UCHAR ReportId;
    UCHAR MaxCount;
} HIDTOUCH_MAXCOUNT_REPORT;

typedef struct _HIDTOUCH_CONFIG_REPORT {
    UCHAR ReportId;
    UCHAR DeviceMode;
    UCHAR DeviceIdentifier;
} HIDTOUCH_CONFIG_REPORT;

#pragma pack(pop)

// Per-contact Logical Collection. NOTE: no USAGE(Finger) inside; the parent
// Application Collection has one USAGE(Finger) declared once.
#define HIDTOUCH_CONTACT_BLOCK \
    0xA1, 0x02,        /*    COLLECTION (Logical) */                 \
    0x09, 0x42,        /*      USAGE (Tip Switch) */                 \
    0x15, 0x00,        /*      LOGICAL_MIN (0) */                    \
    0x25, 0x01,        /*      LOGICAL_MAX (1) */                    \
    0x75, 0x01,        /*      REPORT_SIZE (1) */                    \
    0x95, 0x01,        /*      REPORT_COUNT (1) */                   \
    0x81, 0x02,        /*      INPUT (Data,Var,Abs) */               \
    0x09, 0x32,        /*      USAGE (In Range) */                   \
    0x81, 0x02,        /*      INPUT (Data,Var,Abs) */               \
    0x09, 0x47,        /*      USAGE (Confidence) */                 \
    0x81, 0x02,        /*      INPUT (Data,Var,Abs) */               \
    0x95, 0x05,        /*      REPORT_COUNT (5) */                   \
    0x81, 0x03,        /*      INPUT (Cnst,Ary,Abs) padding */       \
    0x75, 0x08,        /*      REPORT_SIZE (8) */                    \
    0x09, 0x51,        /*      USAGE (Contact Identifier) */         \
    0x95, 0x01,        /*      REPORT_COUNT (1) */                   \
    0x81, 0x02,        /*      INPUT (Data,Var,Abs) */               \
    0x05, 0x01,        /*      USAGE_PAGE (Generic Desktop) */       \
    0x26, 0xFF, 0x7F,  /*      LOGICAL_MAX (32767) */                \
    0x75, 0x10,        /*      REPORT_SIZE (16) */                   \
    0x55, 0x00,        /*      UNIT_EXPONENT (0) */                  \
    0x65, 0x00,        /*      UNIT (None) */                        \
    0x35, 0x00,        /*      PHYSICAL_MIN (0) */                   \
    0x46, 0xFF, 0x7F,  /*      PHYSICAL_MAX (32767) */               \
    0x09, 0x30,        /*      USAGE (X) */                          \
    0x81, 0x02,        /*      INPUT (Data,Var,Abs) */               \
    0x09, 0x31,        /*      USAGE (Y) */                          \
    0x81, 0x02,        /*      INPUT (Data,Var,Abs) */               \
    0x05, 0x0D,        /*      USAGE_PAGE (Digitizers) */            \
    0x09, 0x48,        /*      USAGE (Width) */                      \
    0x81, 0x02,        /*      INPUT (Data,Var,Abs) */               \
    0x09, 0x49,        /*      USAGE (Height) */                     \
    0x81, 0x02,        /*      INPUT (Data,Var,Abs) */               \
    0xC0               /*    END_COLLECTION */

static const UCHAR g_HidTouchReportDescriptor[] = {
    // -------- Touch Screen Application TLC --------
    0x05, 0x0D,                         // USAGE_PAGE (Digitizers)
    0x09, 0x04,                         // USAGE (Touch Screen)
    0xA1, 0x01,                         // COLLECTION (Application)
    0x85, HIDTOUCH_REPORT_ID_INPUT,     //   REPORT_ID 1
    0x09, 0x22,                         //   USAGE (Finger)  -- ONCE, here

    HIDTOUCH_CONTACT_BLOCK,
    HIDTOUCH_CONTACT_BLOCK,
    HIDTOUCH_CONTACT_BLOCK,
    HIDTOUCH_CONTACT_BLOCK,
    HIDTOUCH_CONTACT_BLOCK,
    HIDTOUCH_CONTACT_BLOCK,
    HIDTOUCH_CONTACT_BLOCK,
    HIDTOUCH_CONTACT_BLOCK,
    HIDTOUCH_CONTACT_BLOCK,
    HIDTOUCH_CONTACT_BLOCK,

    // Contact Count
    0x05, 0x0D,                         //   USAGE_PAGE (Digitizers)
    0x09, 0x54,                         //   USAGE (Contact Count)
    0x95, 0x01,                         //   REPORT_COUNT (1)
    0x75, 0x08,                         //   REPORT_SIZE (8)
    0x15, 0x00,                         //   LOGICAL_MIN (0)
    0x25, 0x0A,                         //   LOGICAL_MAX (10)
    0x81, 0x02,                         //   INPUT (Data,Var,Abs)

    // Maximum Contact Count (feature). vmulti style: implicitly inherits
    // size/count/min/max from Contact Count above.
    0x85, HIDTOUCH_REPORT_ID_MAXCOUNT,  //   REPORT_ID 2
    0x09, 0x55,                         //   USAGE (Contact Count Maximum)
    0xB1, 0x02,                         //   FEATURE (Data,Var,Abs)
    0xC0,                               // END_COLLECTION

    // -------- Device Configuration TLC --------
    // vmulti has this *inside* the previous TLC, but Microsoft's sample
    // puts it as a separate top-level. We put it separate -- mtconfig
    // accepts both, and we already saw SetFeatureCalls=1 with this layout.
    0x05, 0x0D,                         // USAGE_PAGE (Digitizers)
    0x09, 0x0E,                         // USAGE (Device Configuration)
    0xA1, 0x01,                         // COLLECTION (Application)
    0x85, HIDTOUCH_REPORT_ID_CONFIG,    //   REPORT_ID 3
    0x09, 0x23,                         //   USAGE (Device Settings)
    0xA1, 0x02,                         //   COLLECTION (Logical)
    0x09, 0x52,                         //     USAGE (Device Mode)
    0x09, 0x53,                         //     USAGE (Device Identifier)
    0x15, 0x00,                         //     LOGICAL_MIN (0)
    0x25, 0x0A,                         //     LOGICAL_MAX (10)
    0x75, 0x08,                         //     REPORT_SIZE (8)
    0x95, 0x02,                         //     REPORT_COUNT (2)
    0xB1, 0x02,                         //     FEATURE (Data,Var,Abs)
    0xC0,                               //   END_COLLECTION
    0xC0                                // END_COLLECTION
};

#define HIDTOUCH_REPORT_DESCRIPTOR_SIZE sizeof(g_HidTouchReportDescriptor)

static const HID_DESCRIPTOR g_HidTouchHidDescriptor = {
    0x09,       // bLength
    0x21,       // bDescriptorType (HID)
    0x0100,     // bcdHID
    0x00,       // bCountryCode
    0x01,       // bNumDescriptors
    {
        { 0x22, (USHORT)HIDTOUCH_REPORT_DESCRIPTOR_SIZE }
    }
};
