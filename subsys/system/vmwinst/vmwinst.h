#ifndef __VMWINST_H
#define __VMWINST_H

#ifndef PSCB_BUTTONPRESSED
#define PSCB_BUTTONPRESSED	(3)
#endif

#ifndef PBS_MARQUEE
#define PBS_MARQUEE	(8)
#endif

/* metrics */
#define PROPSHEETWIDTH  	250
#define PROPSHEETHEIGHT 	120
#define PROPSHEETPADDING        6
#define SYSTEM_COLUMN   	(18 * PROPSHEETPADDING)
#define LABELLINE(x)    	(((PROPSHEETPADDING + 2) * x) + (x + 2))
#define ICONSIZE        	16

/* Resource IDs */

#define IDS_WIZARD_NAME			100
#define IDS_FAILEDTOLOCATEDRIVERS	101
#define IDS_FAILEDTOCOPYFILES		102
#define IDS_FAILEDTOACTIVATEDRIVER	103
#define IDS_FAILEDTOSELVGADRIVER	104
#define IDS_FAILEDTOSELVBEDRIVER	105
#define IDS_UNINSTNOTICE		106

#define IDS_SEARCHINGFORCDROM	201
#define IDS_COPYINGFILES	202
#define IDS_ENABLINGDRIVER	203

#define IDD_WELCOMEPAGE			100
#define IDD_INSERT_VMWARE_TOOLS		101
#define IDD_INSTALLING_VMWARE_TOOLS     102
#define IDD_CONFIG			103
#define IDD_CHOOSEACTION		104
#define IDD_SELECTDRIVER		105
#define IDD_INSTALLATION_FAILED		106
#define IDD_DOUNINSTALL			107

#define IDC_COLORQUALITY	200
#define IDC_CONFIGSETTINGS	201
#define IDC_USEOTHERDRIVER	202
#define IDC_UNINSTALL		203
#define IDC_VGA			204
#define IDC_VBE			205
#define IDC_INSTALLINGSTATUS	206
#define IDC_INSTALLINGPROGRESS	207

#define IDB_WATERMARK		100
#define IDB_HEADER		101

#define IDD_INSERT_VMWARE_TOOLSTITLE		301
#define IDD_INSERT_VMWARE_TOOLSSUBTITLE		302
#define IDD_INSTALLING_VMWARE_TOOLSTITLE	311
#define IDD_INSTALLING_VMWARE_TOOLSSUBTITLE	312
#define IDD_CONFIGTITLE				321
#define IDD_CONFIGSUBTITLE			322
#define IDD_INSTALLATION_FAILEDTITLE		331
#define IDD_INSTALLATION_FAILEDSUBTITLE		332
#define IDD_CHOOSEACTIONTITLE			341
#define IDD_CHOOSEACTIONSUBTITLE		342
#define IDD_SELECTDRIVERTITLE			351
#define IDD_SELECTDRIVERSUBTITLE		352
#define IDD_DOUNINSTALLTITLE			361
#define IDD_DOUNINSTALLSUBTITLE			362

#endif /* __VMWINST_H */
