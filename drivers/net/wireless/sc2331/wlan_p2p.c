#include "wlan_common.h"

void if_to_dev(wlan_vif_t *vif, unsigned char *frame, unsigned short len)
{
    unsigned char *dev,  *sub_if;
    unsigned char *source_addr;
    
    dev     = &(vif->ndev->dev_addr[0]);
    sub_if  = &(vif->sub_if[0].ndev->dev_addr[0]);
    source_addr = &(frame[6]);
    
    if(0 == memcmp(source_addr, sub_if, 6) )
        memcpy(source_addr, dev, 6);
	return;
}

void dev_to_if(wlan_vif_t *vif, unsigned char *frame, unsigned short len)
{
    unsigned char *dev,  *sub_if;
    unsigned char *dest_addr;
	
    dev     = &(vif->ndev->dev_addr[0]);
    sub_if  = &(vif->sub_if[0].ndev->dev_addr[0]);
    dest_addr = &(frame[0]);
    
    if(0 == memcmp(dest_addr, dev, 6) )
        memcpy(dest_addr, sub_if, 6);
	return;
}

void dhcp_rx(wlan_vif_t *vif, unsigned char *frame, unsigned short len)
{
    unsigned char *dev_addr, *if_addr;
    unsigned char *pos = frame;
    int i;
	
    dev_addr = vif->ndev->dev_addr;
    if_addr  = vif->sub_if[0].ndev->dev_addr;
    if (len <= 300)
        return;
    if(memcmp(pos + 70, dev_addr, 6) == 0)
	{
        memcpy(pos + 70, if_addr, 6);  
        memset(pos + 40, 0, 2);
    }
	return;
}

void arp_rx(wlan_vif_t *vif, char *frame, int len)
{
	unsigned char *dev,  *sub_if;
	unsigned char *dst,  *target;
	
	if( (len < 42) || (0x08 != frame[12]) || (0x06 != frame[13]) )
		return;
	dev     = &(vif->ndev->dev_addr[0]);
	sub_if  = &(vif->sub_if[0].ndev->dev_addr[0]);
	target  = &(frame[32]);
	if(0 == memcmp(target, dev, 6) )
		memcpy(target, sub_if, 6);      
	return;
}

void dhcp_tx(wlan_vif_t *vif, unsigned char *frame, unsigned short len)
{
    unsigned char *dev_addr, *if_addr;
    unsigned char *pos = frame;
    int i;

    dev_addr = vif->ndev->dev_addr;
    if_addr  = vif->sub_if[0].ndev->dev_addr;
    if (len <= 300)
        return;
    if(memcmp(pos + 70, if_addr, 6) == 0)
	{
        memcpy(pos + 70, dev_addr, 6); 
        memset(pos + 40, 0, 2);
    }
}

void arp_tx(wlan_vif_t *vif, char *frame, int len)
{
	unsigned char *dev,  *sub_if;
	unsigned char *src,  *sender;
	
	if( (NULL == frame) || (len < 42) || (0x08 != frame[12]) || (0x06 != frame[13]) )
		return;
	dev     = &(vif->ndev->dev_addr[0]);
	sub_if  = &(vif->sub_if[0].ndev->dev_addr[0]);         
	sender  = &(frame[22]);
	if(0 == memcmp(sender, sub_if, 6) )
		memcpy(sender, dev, 6);
	return;
}

void p2p_tx(unsigned char vif_id, char *frame, int len)
{
	wlan_vif_t *vif;
	if(1 != vif_id)
		return;
	vif = &(g_wlan.netif[vif_id]);
	if( (1 != vif->sub_if[0].status) || (NULL == vif->sub_if[0].ndev) || (NULL == frame) || (len < 14) )
		return;
	if_to_dev(vif, frame, len);
	dhcp_tx(vif, frame, len);
	arp_tx(vif, frame, len);
	
	return;
}

void p2p_rx(unsigned char vif_id, char *frame, int len)
{
	wlan_vif_t *vif;
	if(1 != vif_id)
		return;
	vif = &(g_wlan.netif[vif_id]);
	if( (1 != vif->sub_if[0].status) || (NULL == vif->sub_if[0].ndev) || (NULL == frame) || (len < 14) )
		return;
	dev_to_if(vif, frame, len);
	dhcp_rx(vif, frame, len);
	arp_rx(vif, frame, len);
	return;
}


