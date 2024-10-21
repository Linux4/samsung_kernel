#ifndef TP_NOTIFIER_H
#define TP_NOTIFIER_H

#define USB_PLUG_IN 1
#define USB_PLUG_OUT 0

//+S98901AA1,zhangjian.wt,modify,2024/07/31,Optimize charging mode
//static BLOCKING_NOTIFIER_HEAD(usb_tp_notifier_list);
//-S98901AA1,zhangjian.wt,modify,2024/07/31,Optimize charging mode

int ft3419u_usb_register_notifier_chain_for_tp(struct notifier_block *nb);
int usb_notifier_call_chain_for_tp(unsigned long val, void *v);

#endif