#ifndef __CPL_RESOURCE_H
#define __CPL_RESOURCE_H

/* metrics */
#define PROPSHEETWIDTH  246
#define PROPSHEETHEIGHT 228
#define PROPSHEETPADDING(x)  (x+x+x+x+x+x)
#define SYSTEM_COLUMN   (18*PROPSHEETPADDING)
#define LABELLINE(x)  (x+x+x+2+x+x+x+x+x+x)


#define ICONSIZE        16

/* ids */
#define IDI_CPLACCESS		110

#define IDD_PROPPAGEKEYBOARD	100
#define IDD_PROPPAGESOUND	101
#define IDD_PROPPAGEDISPLAY	102
#define IDD_PROPPAGEMOUSE	103
#define IDD_PROPPAGEGENERAL	104

#define IDD_STICKYKEYSOPTIONS	105

#define IDS_CPLSYSTEMNAME	1001
#define IDS_CPLSYSTEMDESCRIPTION	2001

#define IDS_SENTRY_NONE		1501
#define IDS_SENTRY_TITLE	1502
#define IDS_SENTRY_WINDOW	1503
#define IDS_SENTRY_DISPLAY	1504


/* controls */
#define IDC_STICKY_BOX		200
#define IDC_STICKY_BUTTON	201
#define IDC_FILTER_BOX		202
#define IDC_FILTER_BUTTON	203
#define IDC_TOGGLE_BOX		204
#define IDC_TOGGLE_BUTTON	205

#define IDC_SENTRY_BOX		206
#define IDC_SENTRY_TEXT		207
#define IDC_SENTRY_COMBO	208
#define IDC_SSHOW_BOX		209

#define IDC_CONTRAST_BOX	211
#define IDC_CONTRAST_BUTTON	212
#define IDC_MOUSE_BOX		213
#define IDC_MOUSE_BUTTON	214
#define IDC_RESET_BOX		215
#define IDC_RESET_COMBO		216
#define IDC_NOTIFICATION_MESSAGE	217
#define IDC_NOTIFICATION_SOUND	218
#define IDC_SERIAL_BOX		219
#define IDC_SERIAL_BUTTON	220
#define IDC_ADMIN_LOGON_BOX	221
#define IDC_ADMIN_USERS_BOX	222

#define IDC_STICKY_ACTVATE_CHECK	221
#define IDC_STICKY_LOCK_CHECK		222
#define IDC_STICKY_UNLOCK_CHECK		223
#define IDC_STICKY_SIGNAL_CHECK		224
#define IDC_STICKY_STATUS_CHECK		225

#endif /* __CPL_RESOURCE_H */

/* EOF */
