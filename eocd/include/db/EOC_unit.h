/*
 * EOC_db.h:
 *	EOC channel configuration & status storage
 */
 
#ifndef EOC_UNIT_H
#define EOC_UNIT_H

#include <generic/EOC_generic.h>
#include <generic/EOC_responses.h>
#include <db/EOC_side.h>
#define EOC_SIDES_NUM 2

class EOC_unit{
public:
    typedef enum { net_side=0,cust_side } Sides;
    typedef enum { span = 0, local } power_t;
protected:
    unit u;
    resp_inventory inv_info;
    power_t power;
    EOC_side *side[EOC_SIDES_NUM];
public:
    EOC_unit(unit u_in,resp_inventory *resp,int loops){
	inv_info = *resp;
	for( int i=0; i<EOC_SIDES_NUM;i++)
	    side[i] = NULL;
	u = u_in;
	switch( u ){
	case stu_c:
	    side[net_side] = new EOC_side(loops); 
	    break;
	case stu_r:
	    side[cust_side] = new EOC_side(loops);
	    break;
	default:
	    side[cust_side] = new EOC_side(loops); 
	    side[net_side] = new EOC_side(loops);
	    break;
	}
    }
    
    EOC_side *nside(){ return side[net_side]; }
    EOC_side *cside(){ return side[cust_side]; }

    int integrity(resp_inventory *resp){
	if( memcmp(resp,&inv_info,sizeof(inv_info) ) )
	    return -1;
	return 0;
    }
    resp_inventory inventory_info(){ return inv_info; }
    
};

#endif
 
