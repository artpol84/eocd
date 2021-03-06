#include <syslog.h>

#define EOC_DEBUG
#include <eoc_debug.h>

#include <generic/EOC_generic.h>
#include <generic/EOC_msg.h>
#include <generic/EOC_requests.h>
#include <generic/EOC_responses.h>

#include <engine/EOC_router.h>
#include <engine/EOC_responder.h>
#include <engine/EOC_poller.h>
#include <engine/EOC_engine_act.h>

#include <app-if/err_codes.h>

#include <handlers/EOC_poller_req.h>

#include <conf_profile.h>
#include <shdsl/config.h>

int EOC_engine_act::register_handlers() {
	if(poll){
		poll->register_request(REQ_DISCOVERY, _req_discovery);
		poll->register_request(REQ_INVENTORY, _req_inventory);
		poll->register_request(REQ_CONFIGURE, _req_configure);
		poll->register_request(REQ_STATUS, _req_status);
		poll->register_request(15, _req_test);
	}
}

//--------------------------------------------------------------------------------------
// Terminal constructor

EOC_engine_act::EOC_engine_act(EOC_dev_terminal *d1, EOC_config *c,
	EOC_dev::dev_del_func df, u16 ticks_p_min, u16 rmax) :
	EOC_engine(d1, c, master, df, rmax) {
	ASSERT(d1);
	recv_max = rmax;
	poll = new EOC_poller(cfg, ticks_p_min, rtr->loops());
	register_handlers();
}

//----------------------------------------------------------------------
// setup_state - setups poller link status if i really changes
// on device

int EOC_engine_act::setup_state_act() {
	EOC_dev *dev;

	if(setup_state())
		return -1;

	switch(type){
	case repeater:
		return 0;
	case master:
		if(!(dev = rtr->csdev()))
			return -1;
		break;
	case slave:
		if(!(dev = rtr->nsdev()))
			return -1;
		break;
	}

	if(dev->link_state()==EOC_dev::OFFLINE){
		if(poll->link_state()==EOC_dev::ONLINE)
			poll->link_state(EOC_dev::OFFLINE);
	}else if(poll->link_state()==EOC_dev::OFFLINE){
		poll->link_state(EOC_dev::ONLINE);
	}
	return 0;
}

//----------------------------------------------------------------------
// schedule - call it to take control to engine for processing
// incoming and outgoing messages
int EOC_engine_act::schedule(char *ch_name) {
	EOC_msg *m, **ret;
	ASSERT(rtr&&resp); // Constructor failed

	if(setup_state_act())
		return -1;
	int i = 0;
	int cnt;

	PDEBUG(DFULL, "schedule -------------------------------------------------");
//	PDEBUG(DFULL, "Receiving");
	while((m = rtr->receive())&&i<recv_max){
		m->set_chname(ch_name);
		if(m->is_request()){
			if(resp->request(m, ret, cnt)){
				PDEBUG(DERR, "error in resp->request(m, ret, cnt)");
				delete m;
				break;
			}
			if(!ret){
				PDEBUG(DFULL,"Have one-message response");
				// only one message to respond
				if(rtr->send(m)){
					PDEBUG(DERR, "error in rtr->send(m)");
					delete m;
					break;
				}
			}else if(ret){
				// several messages to respond
				PDEBUG(DFULL,"Have %d-message response",cnt);
				for(i = 0;i<cnt;i++){
					if(rtr->send(ret[i])){
						PDEBUG(DERR, "error in rtr->send(ret[%d])",i);
						for(int j = 0;j<cnt;j++){
							delete ret[j];
						}
						delete[] ret;
						delete m;
						break;
					}
				}
				// BUG fix (03.07.08): missing of this loop
				// leads to memory leack
				// PDEBUG(DFULL,"Cleaning all ret-related memory");
				for(int j = 0;j<cnt;j++){
					delete ret[j];
				}
				delete[] ret;
			}
		}else if(m->is_response()){
			// PDEBUG(DFULL, "m->is_response()");
			if(poll->process_msg(m)){
				PDEBUG(DERR, "error processing msg; src=%d,dst=%d,id=%d",
					m->src(), m->dst(), m->type());
				delete m;
				break;
			}
		}
		delete m;
		i++;
	}

	//PDEBUG(DFULL, "Sending");
	// Send one EOC request
	while(m = poll->gen_request()){
    PDEBUG(DINFO,"REQUEST: src(%d) dst(%d) id(%d)",
      m->src(), m->dst(), m->type() );
		if(rtr->send(m)){
			PDEBUG(DFULL,"error in rtr->send(m)");
			delete m;
			continue;
		}
		delete m;
	}
	poll->finish_poll();
	//PDEBUG(DFULL, "exit");
	return 0;
}

int EOC_engine_act::app_request(app_frame *fr) {

	switch(fr->id()){
	case APP_SPAN_PARAMS: {
		span_params_payload *p = (span_params_payload *)fr->payload_ptr();
		p->units = poll->unit_quan(1);
		p->loops = rtr->loops();
		p->link_establ = poll->link_established();
		return 0;
	}
	case APP_SPAN_STATUS: {
		span_status_payload *p = (span_status_payload*)fr->payload_ptr();
		p->nreps = poll->reg_quan(1);
		p->nreps_avail = poll->reg_quan(0);
		if(!rtr->csdev()){
			fr->negative(ERUNEXP);
			break;
		}
		span_conf_profile_t cfg;
		int mode, tcpam;
		if(((EOC_dev_terminal*)rtr->csdev())->cur_config(cfg, mode)){
			PDEBUG(DERR, "Error requesting configuration");
			fr->negative(ERUNEXP);
			break;
		}

		p->max_lrate = cfg.rate;
		p->act_lrate = cfg.rate;
		switch(cfg.annex){
		case annex_a:
			p->region0 = 1;
			break;
		case annex_b:
			p->region1 = 1;
			break;
		}
		p->max_prate = cfg.rate;
		p->act_prate = cfg.rate;
		p->tcpam = cfg.tcpam;
		break;
	}
	default: {
		PDEBUG(DERR, "Poller request");
		poll->app_request(fr);
		return 0;
	}
	}
	return 0;
}

