// system includes
#include <sys/types.h>
#include <syslog.h>
#include <dirent.h>

// lib includes
#include <iostream>
#include <libconfig.h++>

// local includes
#include <generic/EOC_generic.h>
#include <snmp/snmp-generic.h>
#include <utils/hash_table.h>
#include <config/EOC_config.h>

#include <devs/EOC_dev.h>
#include <devs/EOC_dev_terminal.h>
#include <devs/EOC_mr17h.h>
#include <devs/EOC_mr16h.h>

#include <EOC_main.h>
#include <conf_profile.h>
#include <channel.h>

#include <app-if/err_codes.h>

using namespace libconfig;
using namespace std;
//---------------------------------------------------------------------//
EOC_dev_terminal *init_dev(char *name);


#ifndef VIRTUAL_DEVS
EOC_dev_terminal *
init_dev(char *name)
{
    char *path=OS_IF_PATH;
    DIR *dir;
    struct dirent *ent;
    EOC_dev_terminal *dev = NULL;
    
    if( !(dir = opendir(path) ) ){
		printf("Cannot open %s\n",path);
		return NULL;
    }

    while( (ent = readdir(dir)) ){
		if( !strcmp(ent->d_name,name) ){
			printf("IF %s is phisicaly present\n",name);
			char cfg_dir[PATH_SIZE];
			DIR *dir1;
			mr17h_conf_dir(ent->d_name,cfg_dir,PATH_SIZE);
			if( dir1 = opendir(cfg_dir) ){
				printf("Seems it mr17h\n");
				dev = (EOC_dev_terminal*)new EOC_mr17h(name);
				closedir(dir1);
				side_perf s;
				if( !dev->init_ok() ){
					printf("Error initialising %s\n",name);		    
					delete dev;
					return NULL;
				}
				//		dev->statistics(0,s); // Not need by now
				//	( (EOC_mr17h*)dev )->dbg_last_msg();
				printf("Dev %s successfully initialised\n",name);		    
				return dev; 
			}
			/*	    else { 
					snprintf(cfg_dir,256,"/sys/bus/pci/drivers/sg16lan/%s",ent->d_name);
					if( dir1 = opendir(cfg_dir) ){
					dev = new EOC_mr16h(name);
					closedir(dir1);
					if( !dev.init_ok() ){
					delete dev;
					return NULL;
					}
					return dev; 
					}
					}
			*/
		}
    }
    closedir(dir);
    return NULL;
}
#endif // #ifndef VIRTUAL_DEVS

int EOC_main::
read_config()
{
    Config cfg;
    char *name;
    int i;
    tick_per_min = TICK_PER_MINUTE_DEF;
	hash_elem *el;

    //----------- Open config file ------------------//
    PDEBUG(DINFO,"Open config file: %s",config_file);
    try	{
		cfg.readFile((const char *)config_file);
	}catch(ParseException& ex){
		//eoc_debug("Error in %s (%d): %s",file,ex.getLine(),ex.getError());
		syslog(LOG_ERR,"Error in %s (%d): %s",config_file,ex.getLine(),ex.getError());
		PDEBUG(DERR,"Error in %s (%d): %s",config_file,ex.getLine(),ex.getError());
		return -1;
    }catch(FileIOException &fex){
		// eoc_debug("Cannot open configuration file: %s",file);
		syslog(LOG_ERR,"Cannot open configuration file: %s",config_file);
		PDEBUG(DERR,"Cannot open configuration file: %s",config_file);
		return -1;
    }catch(...){
		syslog(LOG_ERR,"Error openning config file: %s",config_file);
		PDEBUG(DERR,"Error openning config file: %s",config_file);
		return -1;
    }
    PDEBUG(DINFO,"Config file opened successfull");

    //----------- read tick per minute value ------------------//
    // 1. Check that conf_profile group exist
    try{
		tick_per_min = cfg.lookup("act_per_minute");
		if( tick_per_min < 0 ){
			syslog(LOG_ERR,"(%s): value of \"act_per_minute\" must be greater than zero",
				   config_file);
			PDEBUG(DERR,"(%s): value of \"act_per_minute\" must be greater than zero",
				   config_file);
			return -1;
		}else if( tick_per_min > TICK_PER_MINUTE_MAX ){
			syslog(LOG_ERR,"(%s): exceed max value of \"act_per_minute\": %d, must be %d",
				   config_file,tick_per_min,TICK_PER_MINUTE_MAX);
			PDEBUG(DERR,"(%s): exceed max value of \"act_per_minute\": %d, must be %d",
				   config_file,tick_per_min,TICK_PER_MINUTE_MAX);
			return -1;
		}	
    }catch(SettingNotFoundException &snfex){
		// nothing to do - use default value
    }catch(SettingTypeException &tex){
		// wrong type - configuration parsing error
		syslog(LOG_ERR,"(%s): wrong data type of \"act_per_minute\" setting\n",
			   config_file);
		PDEBUG(DERR,"(%s): wrong data type of \"act_per_minute\" setting",
			   config_file);
		return -1;
    }catch(...){
		syslog(LOG_ERR,"Unexpected error \"act_per_minute\" setting in %s",
			   config_file);
		PDEBUG(DERR,"Unexpected error \"act_per_minute\" setting in %s",
			   config_file);
		return -1;
    }
    PDEBUG(DINFO,"tick_p_min = %d",tick_per_min);

    //----------- read span configuration profiles ------------------//
    // 1. Check that conf_profile group exist
    try{
		Setting &s = cfg.lookup("span_profiles");
    }catch(...){
        // eoc_log("(%s): cannot found \"conf_profiles\" section",config_file);
        syslog(LOG_ERR,"(%s): cannot found \"span_profiles\" section",config_file);
        PDEBUG(DERR,"(%s): cannot found \"span_profiles\" section",config_file);
        return -1;
    }

    // 2. read all elements of conf_profiles
    for(i=0;;i++){
		name = NULL;
		try{
    	    Setting &s = cfg.lookup("span_profiles");
			const char *str = s[i]["name"];
			if( !(name = strndup(str,SNMP_ADMIN_LEN)) ){
				syslog(LOG_ERR,"Not enougth memory");
				PDEBUG(DERR,"Not enougth memory");
				return -1;
			}
			int wires = s[i]["wires"];
			int rate = s[i]["rate"];
			int annex = s[i]["annex"];
			int power = s[i]["powerSource"];
			int ref_clock = s[i]["refClock"];
			int line_probe = s[i]["lineProbe"];
			int cur_marg_down = s[i]["currCondMargDown"];
			int worst_marg_down = s[i]["worstCaseMargDown"];
			int cur_marg_up = s[i]["currCondMargUp"];
			int worst_marg_up = s[i]["worstCaseMargUp"];

			int use_cur_down;
			int use_worst_down;
			int use_cur_up;
			int use_worst_up;

			try{ use_cur_down = s[i]["useCurrCondMargDown"];
			}catch(...){ use_cur_down = 0; }

			try{ use_worst_down = s[i]["useWorseCaseMargDown"];
			}catch(...){ use_worst_down = 0; }

			try{ use_cur_up = s[i]["useCurrCondMargUp"];
			}catch(...){ use_cur_up = 0; }

			try{ use_worst_up = s[i]["useWorseCaseMargUp"];
			}catch(...){ use_worst_up = 0; }

	    
			// Check input values
			if( wires > 4 || wires <= 0 ){
				syslog(LOG_ERR,"(%s): wrong \"wires\" value in %s profile: %d , may be 1-4",
					   config_file,name,wires);
				PDEBUG(DERR,"(%s): wrong \"wires\" value in %s profile: %d , may be 1-4",
					   config_file,name,wires);
				return -1;
			}
			if( annex > 2 || annex <= 0){
				syslog(LOG_ERR,"(%s): wrong \"wires\" value in %s profile: %d , may be 1-4",
					   config_file,name,annex);
				PDEBUG(DERR,"(%s): wrong \"wires\" value in %s profile: %d , may be 1-4",
					   config_file,name,annex);
				return -1;
			}
	    	
			if( power > 3 && power <= 0){
				syslog(LOG_ERR,"(%s) wrong \"power\" value in %s profile: %d , may be 0,1",
					   config_file,name,power);
				PDEBUG(DERR,"(%s) wrong \"power\" value in %s profile: %d , may be 0,1",
					   config_file,name,power);
				return -1;
			}

			if( ref_clock > 4 || ref_clock <= 0){
				syslog(LOG_ERR,"(%s): wrong \"ref_clock\" value in %s profile: %d , may be 1-4",
					   config_file,name,ref_clock);
				PDEBUG(DERR,"(%s): wrong \"ref_clock\" value in %s profile: %d , may be 1-4",
					   config_file,name,ref_clock);
				return -1;
			}
	    	
			if( line_probe != 1 && line_probe != 2){
				syslog(LOG_ERR,"(%s): wrong \"line_probe\" value in %s profile: %d , may be 1,2",
					   config_file,name,line_probe);
				PDEBUG(DERR,"(%s): wrong \"line_probe\" value in %s profile: %d , may be 1,2",
					   config_file,name,line_probe);
				return -1;
			}

			conf_profile *nprof = new conf_profile;
			memset(nprof,0,sizeof(conf_profile));
			nprof->name = name;
			nprof->nsize = strlen(name);

			nprof->conf.annex = (annex_t)annex;
			nprof->conf.wires = (wires_t)wires;
			nprof->conf.power = (power_t)power;
			// ????	    nprof->conf.psd = 0;
			nprof->conf.clk = (clk_t)ref_clock;
			nprof->conf.line_probe = (line_probe_t)line_probe;
			// ????	    nprof->conf.remote_cfg = (remote_cfg_t)rem_cfg; 
			nprof->conf.rate = rate;

			nprof->conf.use_cur_down = use_cur_down;
			nprof->conf.use_worst_down = use_worst_down;
			nprof->conf.use_cur_up = use_cur_up;
			nprof->conf.use_worst_up = use_worst_up;
	    
			nprof->conf.cur_marg_down = cur_marg_down;
			nprof->conf.worst_marg_down = worst_marg_down;
			nprof->conf.cur_marg_up = cur_marg_up;
			nprof->conf.worst_marg_up = worst_marg_up;

			PDEBUG(DERR,"Save profile %s",nprof->name);
			conf_profile *cprof = (conf_profile *)conf_profs.find(nprof->name,nprof->nsize);
			if( !cprof ){
				PDEBUG(DERR,"Create new profile");
				// New profile 
				nprof->is_updated = 1;
				conf_profs.add(nprof);
			}else{
				PDEBUG(DERR,"Profile exist");
				if( memcmp(&cprof->conf,&nprof->conf,sizeof(span_conf_profile_t)) ){
					PDEBUG(DERR,"Profile changed - modify");
					memcpy(cprof,nprof,sizeof(conf_profile));
					delete nprof;
				}
				cprof->is_updated = 1;
			}
		}catch(ConfigException& cex){
    	    if( name ){
				syslog(LOG_ERR,"(%s): error while parsing profile: %s",
					   config_file,name);
				PDEBUG(DERR,"(%s): error while parsing profile: %s",
					   config_file,name);
				return -1;
			}
			break;
		}catch(...){
			syslog(LOG_ERR,"Fail to get span_profiles[%d] element",i);
			PDEBUG(DERR,"Fail to get span_profiles[%d] element",i);

		}
    }

	PDEBUG(DERR,"Clear conf table from old profiles");
    conf_profs.clear();
	conf_profs.sort();

	/*
    //----------- read span alarm profiles ------------------//
    // 1. Check that span_profile group exist
    try{
	Setting &s = cfg.lookup("span_profiles");
    }catch(...){
	// eoc_log("Cannot found \"span_profiles\" section in %s file",file);
	printf("Cannot found \"span_profiles\" section in %s file",file);
	return -1;
    }

    // 2. read all elements of span_profiles
    while(1){
	name = NULL;
	try{
	Setting &s = cfg.lookup("span_profiles");
	const char *str = s[i]["name"];
	if( !(name = strndup(str,SNMP_ADMIN_LEN)) ){
	//eoc_log("Not enougth memory");
	printf("Not enougth memory");
	return -1;
	}
	int wires = s[i]["wires"];
	int min_rate = s[i]["min_rate"];
	int max_rate = s[i]["max_rate"];
	int annex = s[i]["annex"];
	int power = s[i]["power"];
	int ref_clock = s[i]["ref_clock"];
	int line_probe = s[i]["line_probe"];
	    
	// Check input values
	if( wires > 4 || wires <= 0 ){
	//eoc_log("Wrong \"wires\" value in %s profile: %d , may be 1-4",name,wires);
	printf("Wrong \"wires\" value in %s profile: %d , may be 1-4",name,wires);
	return -1;
	}
	if( annex > 3 || annex < 0){
	//eoc_log("Wrong \"annex\" value in %s profile: %d , may be 0-3",name,annex);
	printf("Wrong \"wires\" value in %s profile: %d , may be 1-4",name,annex);
	return -1;
	}
	    	
	if( power != 1 && power != 0){
	//eoc_log("Wrong \"power\" value in %s profile: %d , may be 0,1",name,power);
	printf("Wrong \"power\" value in %s profile: %d , may be 0,1",name,power);
	return -1;
	}

	if( ref_clock > 3 || ref_clock < 0){
	//eoc_log("Wrong \"ref_clock\" value in %s profile: %d , may be 0-3",name,ref_clock);
	printf("Wrong \"ref_clock\" value in %s profile: %d , may be 1-4",name,ref_clock);
	return -1;
	}
	    	
	if( line_probe != 1 && line_probe != 0){
	//eoc_log("Wrong \"line_probe\" value in %s profile: %d , may be 0,1",name,line_probe);
	printf("Wrong \"line_probe\" value in %s profile: %d , may be 0,1",name,line_probe);
	return -1;
	}

	span_profile *nprof = new span_profile;
	nprof->name = name;
	nprof->nsize = strlen(name);
	nprof->wires = wires;
	nprof->annex = annex;
	nprof->power = power;
	nprof->ref_clk = ref_clock;
	nprof->line_probe = line_probe;
	nprof->min_rate = min_rate;
	nprof->max_rate = max_rate;
	conf_profs.add(nprof);
	cout << "ADD: " << name << " " << wires << " " << min_rate << " " << max_rate << " " << annex << " " << power << " " << ref_clock << " " << line_probe << endl;
	}catch(ConfigException& cex){
	if( name ){
	// eoc_log("Error while parsing profile: %s",name);
	printf("Error while parsing profile: %s",name);
	return -1;
	}
	break;
	}catch(...){
	cout << "fail get element" << endl;
	}
    }
	*/

    //----------- read channels configuration ------------------//
    // 1. Check that channels group exist
    try{
		Setting &s = cfg.lookup("channels");
    }catch(...){
		syslog(LOG_ERR,"(%s): cannot found \"channels\" section",config_file);
		PDEBUG(DERR,"(%s): cannot found \"channels\" section",config_file);
        return -1;
    }

    // 2. read all elements of channels
    for(i=0;;i++){
		name = NULL;
		try{
    	    Setting &s = cfg.lookup("channels");
			const char *str = s[i]["name"];
			if( !(name = strndup(str,SNMP_ADMIN_LEN)) ){
				syslog(LOG_ERR,"Not enougth memory");
				PDEBUG(DERR,"Not enougth memory");
				return -1;
			}
			int master = s[i]["master"];
	    
			if( master != 1 && master != 0){
				syslog(LOG_ERR,"(%s): wrong \"master\" value in %s channel: %d , may be 0,1",
					   config_file,name,master);
				PDEBUG(DERR,"(%s): wrong \"master\" value in %s channel: %d , may be 0,1",
					   config_file,name,master);
				return -1;
			}

			// If channel is slave - only responder part
			if( !master ){
				if( add_slave(name) ){
					syslog(LOG_ERR,"(%s): cannot add channel \"%s\" - no such device",
						   config_file,name);
					PDEBUG(DERR,"(%s): cannot add channel \"%s\" - no such device",
						   config_file,name);
				}    
				continue;
			}
	    
			char *cprof = strndup(s[i]["conf_profile"],SNMP_ADMIN_LEN);
			if( !cprof ){
				syslog(LOG_ERR,"(%s): Not enought memory",
					   config_file);
				PDEBUG(DERR,"(%s): Not enought memory",
					   config_file);
				return -1;
			}
			if( !conf_profs.find((char*)cprof,strlen(str)) ){
				syslog(LOG_ERR,"(%s) wrong \"conf_profile\" value in %s channel: %s, no such profile",
					   config_file,name,cprof);
				PDEBUG(DERR,"(%s) wrong \"conf_profile\" value in %s channel: %s, no such profile",
					   config_file,name,cprof);
				return -1;
			}
			/*
			  char *aprof = strndup(s[i]["alarm_profile"],SNMP_ADMIN_LEN);
			  if( !aprof ){
			  //eoc_log("(%s): Not enought memory",config_file);
			  printf("(%s): Not enought memory",config_file);
			  return -1;
			  }
			  if( !alarm_profs.find((char*)str,strlen(str)) ){
			  //eoc_log("(%s): wrong \"alarm_profile\" value in %s channel: %s, no such profile",config_file,name,aprof);
			  printf("(%s) wrong \"alarm_profile\" value in %s channel: %s, no such profile",config_file,name,aprof);
			  return -1;
			  }

			*/	    
	    
			int repeaters = s[i]["repeaters"];
			if( repeaters <0 || repeaters > MAX_REPEATERS ){
				syslog(LOG_ERR,"(%s): wrong \"conf_profile\" value in %s channel: %d, may be 1-%d",
					   config_file,name,repeaters,MAX_REPEATERS);
				PDEBUG(DERR,"(%s): wrong \"conf_profile\" value in %s channel: %d, may be 1-%d",
					   config_file,name,repeaters,MAX_REPEATERS);
				return -1;
			}
	    
			int eocd_apply = 0;
			try{ eocd_apply = s[i]["apply_conf"]; }
			catch(...){ eocd_apply = 0; }
	    
			PDEBUG(DINFO,"%s: apply config from cfg-file = %d",name,eocd_apply);
	    
			// TODO: Add alarm handling
			if( add_master(name,cprof,NULL,repeaters,tick_per_min,eocd_apply) ){
				syslog(LOG_ERR,"(%s): cannot add channel \"%s\" - no such device",
					   config_file,name);
				PDEBUG(DERR,"(%s): cannot add channel \"%s\" - no such device",
					   config_file,name);
				continue;
			}
			cout << name << " " << master << " " << str << endl;
		}catch(ConfigException& cex){
    	    if( name ){
				syslog(LOG_ERR,"(%s): error while parsing profile: %s",
					   config_file,name);
				PDEBUG(DERR,"(%s): error while parsing profile: %s",
					   config_file,name);
				return -1;
			}
			break;
		}catch(...){
			syslog(LOG_ERR,"Fail to get channels[%d] element",i);
			PDEBUG(DERR,"Fail to get channels[%d] element",i);
		}
    }

	PDEBUG(DERR,"Clear Channels table from old channels");
	channels.clear();

    return 0;
}

int EOC_main::
write_config()
{
    Config cfg;
	
	try{ 
		Setting &set = cfg.getRoot(); 
		Setting &tick = set.add("act_per_minute",TypeInt);
		Setting &chans = set.add("channels",TypeList);
		Setting &cprofs = set.add("span_profiles",TypeList);
		//	Setting &aprofs = set.add("alarm_profiles"); // Future work
		PDEBUG(DERR,"Start filling configuration");
		// Time tick setting
		tick = tick_per_min;
		PDEBUG(DERR,"Tick filled");
		// Channels settings
		hash_elem *el = channels.first();
		int i = 0;
		while( el ){
			channel_elem *ch = (channel_elem*)el;
			PDEBUG(DERR,"Add channel %s",el->name);
			chans.add(TypeGroup);
			// Channel name
			chans[i].add("name",TypeString);
			chans[i]["name"] = ch->name;
			PDEBUG(DERR,"\tName filled");
			// Channel type
			chans[i].add("master",TypeInt);
			if( ch->eng->get_type() == master ){
				chans[i]["master"] = 1;
			}else if( ch->eng->get_type() == slave ){
				// We dont need to proceed with other options
				chans[i]["master"] = 0;
				i++;
				el = channels.next(el->name,el->nsize);
				continue;
			}else{
				// Must not happend!
				syslog(LOG_ERR,"Found unknown device type: %s",ch->eng->get_type());
				PDEBUG(DERR,"Found unknown device type: %s",ch->eng->get_type());
			}
			PDEBUG(DERR,"\tMaster filled");
	  
			// Channel repeaters num
			chans[i].add("repeaters",TypeInt);
			chans[i]["repeaters"] = ((EOC_engine_act*)ch->eng)->config()->repeaters();
			PDEBUG(DERR,"\tReps filled");
			// Channel configuration profile
			chans[i].add("conf_profile",TypeString);
			chans[i]["conf_profile"] = ((EOC_engine_act*)ch->eng)->config()->cprof();
			PDEBUG(DERR,"\tConf prof filled");
			// Channels alarm profile 
			// 			chans[i].add("alarm_profile",TypeString);
			// 			chans[i]["alarm_profile"] = ((EOC_engine_act*)ch->eng)->config()->aprof();
			// Apply configuration setting
			chans[i].add("apply_conf",TypeInt);
			chans[i]["apply_conf"] = ((EOC_engine_act*)ch->eng)->config()->can_apply();
			PDEBUG(DERR,"\tapply filled");
			el = channels.next(el->name,el->nsize);
			i++;
		}
		
		// Span configuration profiles
		el = conf_profs.first();
		i = 0;
		while( el ){
			conf_profile *p = (conf_profile*)el;
			cprofs.add(TypeGroup);
			// Channel name
			cprofs[i].add("name",TypeString);
			cprofs[i]["name"] = p->name;
			// Rate
			cprofs[i].add("rate",TypeInt);
			cprofs[i]["rate"] = (int)(p->conf.rate);
			// Annex
			cprofs[i].add("annex",TypeInt);
			cprofs[i]["annex"] = p->conf.annex;
			// powerSource
			cprofs[i].add("powerSource",TypeInt);
			cprofs[i]["powerSource"] = p->conf.power;
			// refClock
			cprofs[i].add("refClock",TypeInt);
			cprofs[i]["refClock"] = p->conf.clk;
			// lineProbe
			cprofs[i].add("lineProbe",TypeInt);
			cprofs[i]["lineProbe"] = p->conf.line_probe;
			// currCondMargDown
			cprofs[i].add("currCondMargDown",TypeInt);
			cprofs[i]["currCondMargDown"] = p->conf.cur_marg_down;
			// worstCaseMargDown
			cprofs[i].add("worstCaseMargDown",TypeInt);
			cprofs[i]["worstCaseMargDown"] = p->conf.worst_marg_down;
			// currCondMargUp
			cprofs[i].add("currCondMargUp",TypeInt);
			cprofs[i]["currCondMargUp"] = p->conf.cur_marg_up;
			// worstCaseMargUp
			cprofs[i].add("worstCaseMargUp",TypeInt);
			cprofs[i]["worstCaseMargUp"] = p->conf.worst_marg_up;

			// Move to next channel
			el = conf_profs.next(el->name,el->nsize);
			i++;
		}

		PDEBUG(DERR,"Write config to disk");
		syslog(LOG_NOTICE,"Write configuration to disk (/var/eocd/settings.cache)");
		cfg.writeFile("/var/eocd/setings.cache");
	}catch(...){
		syslog(LOG_ERR,"Error while writing configuration to cache");
		PDEBUG(DERR,"Error while writing configuration to cache");
	}
	return 0;
}

int EOC_main::
add_slave(char *name)
{
	PDEBUG(DERR,"Add slave %s",name);
	do{
		channel_elem *el = (channel_elem *)channels.find(name,strlen(name));
		if( el ){ // If this device exist in current configuration
			if( el->eng->get_type() != slave ){
				PDEBUG(DERR,"Reinit device %s",name);
				channels.del(name,strlen(name));
				break;
			}
			el->is_updated = 1;
			return 0;
		}
	}while(0);

	EOC_dev_terminal *dev = init_dev(name);
	if( !dev )
		return -1;
	channel_elem *el = new channel_elem(dev);
	el->name = name;
	el->nsize = strlen(name);
	el->is_updated = 1;
	channels.add(el);
	return 0;
}

int EOC_main::
add_master(char *name,char *cprof, char *aprof,int reps,int tick_per_min,int app_cfg)
{
	do{
		channel_elem *el = (channel_elem*)channels.find(name,strlen(name));
		if( el ){ // If this device exist in current configuration
			// If type of interface is changed
			if( el->eng->get_type() != master ){
				PDEBUG(DERR,"Reinit device %s",name);
				channels.del(name,strlen(name));
				break;
			}
			// Check if some of optins changed
			EOC_config *cfg = ((EOC_engine_act*)el->eng)->config();
			// Repeaters number
			cfg->repeaters(reps);
			// Configuration profile name
			cfg->cprof(cprof);
			// Alarm profile name
			cfg->aprof(aprof);
			// eocd apply capability
			cfg->can_apply(app_cfg);
			el->is_updated = 1;
			return 0;
		}
	}while(0);

	EOC_dev_terminal *dev = (EOC_dev_terminal *)init_dev(name);
	if( !dev )
		return -1;
	EOC_config *cfg = new EOC_config(&conf_profs,&alarm_profs,cprof,aprof,reps,app_cfg);
	channel_elem *el = new channel_elem(dev,cfg,tick_per_min);
	el->name = name;
	el->nsize = strlen(name);
	el->is_updated = 1;
	channels.add(el);
	return 0;
}

int EOC_main::
configure_channels()
{
	channel_elem *el = (channel_elem*)channels.first();
	while( el ){
		el->eng->configure(el->name);
		el = (channel_elem*)channels.next(el->name,el->nsize);
	}
}


int EOC_main::
poll_channels()
{
	channel_elem *el = (channel_elem*)channels.first();
	while( el ){
		el->eng->schedule();
		el = (channel_elem*)channels.next(el->name,el->nsize);
	}
}

// ------------ Application requests ------------------------------//

void EOC_main::
app_listen(int seconds)
{
	char *buff;
	int size,conn;
	int ret;
	time_t start,cur;
	time(&start);
	cur = start;

	while( time(&cur) >0 && time(&cur)-start < seconds ){
		int to_wait = seconds - (time(&cur)-start);
		if( !app_srv.wait(to_wait) ){
			continue;
		}
		PDEBUG(DERR,"Get new message");
		while( (size = app_srv.recv(conn,buff)) ){ 
			// Form & check incoming request
			app_frame fr(buff,size);
			if( !fr.frame_ptr() ){
				delete buff;
				PDEBUG(DERR,"Error request");
				continue; 
			}

			// Fill response payload
			if( ret = app_request(&fr) ){
				PDEBUG(DERR,"Failed to serv app request");
				continue;
			}

			// Form response
			fr.response();
			// Send response
			ret = app_srv.send(conn,fr.frame_ptr(),fr.frame_size());
		}
		time(&cur);
	}
}


int EOC_main::
app_request(app_frame *fr)
{
	if( fr->role() != app_frame::REQUEST )
		return -1;
    
	PDEBUG(DERR,"App request, ID = %d",fr->id());
	switch( fr->id() ){
	case APP_SPAN_NAME:
		return app_spanname(fr);
	case APP_CPROF:
		PDEBUG(DERR,"APP_SPAN_CPROF");
		return app_cprof(fr);
	case APP_LIST_CPROF:
		PDEBUG(DERR,"APP_SPAN_CPROF_LIST");
		return app_list_cprof(fr);
	case APP_ADD_CPROF:
		PDEBUG(DERR,"APP_ADD_CPROF");
		return app_add_cprof(fr);
	case APP_DEL_CPROF:
		PDEBUG(DERR,"APP_DEL_CPROF");
		return app_del_cprof(fr);
	case APP_ADD_CHAN:
		PDEBUG(DERR,"APP_ADD_CHAN");
		return app_add_chan(fr);
	case APP_DEL_CHAN:
		PDEBUG(DERR,"APP_DEL_CHAN");
		return app_del_chan(fr);
	case APP_CHNG_CHAN:
		PDEBUG(DERR,"APP_CHANG_CHAN");
		return app_chng_chan(fr);
	case APP_ENDP_APROF:
		PDEBUG(DERR,"APP_ENDP_APEOF");
		return 0;
	}

	PDEBUG(DERR,"Channel request");
	if( fr->chan_name() )
		return app_chann_request(fr);
	return -1;
}


int EOC_main::
app_spanname(app_frame *fr)
{
	span_name_payload *p = (span_name_payload*)fr->payload_ptr();

	switch(fr->type()){
	case APP_GET:
	case APP_GET_NEXT:{
		channel_elem *el = (channel_elem*)channels.first();
		if( !el ){
			fr->negative(ERCHNEXIST);
			return 0;
		}
		if( strnlen(fr->chan_name(),SPAN_NAME_LEN) ){ 
			// if first name isn't zero
			el = (channel_elem *)
				channels.find((char*)fr->chan_name(),strnlen(fr->chan_name(),SPAN_NAME_LEN));
			if( !el ){
				fr->negative(ERCHNEXIST);
				return 0;
			}
			el = (channel_elem*)channels.next(el->name,el->nsize);
		}
		int filled = 0;
		while( el && filled<SPAN_NAMES_NUM){
			if( el->eng->get_type() == master ){
				int cp_len = (el->nsize>SPAN_NAME_LEN) ? SPAN_NAME_LEN : el->nsize;
				strncpy(p->name[filled],el->name,cp_len);
				filled++;
			}
			el = (channel_elem*)channels.next(el->name,el->nsize);
		}
		p->filled = filled;
		p->last_msg = ( el && (filled = SPAN_NAMES_NUM) ) ? 0 : 1;
		return 0;
	}
	}
	fr->negative(ERTYPE);
	return 0;
}

int EOC_main::
app_chann_request(app_frame *fr)
{
	// check that requested channel exist
	channel_elem *el = (channel_elem *)
		channels.find((char*)fr->chan_name(),strnlen(fr->chan_name(),SPAN_NAME_LEN));
	if( !el ){ // No such channel on this device
		fr->negative(ERCHNEXIST);
		return 0;
	}
	EOC_engine *eng = el->eng;
	if( eng->get_type() != master ){ // Channel do not maintain EOC DB
		fr->negative(ERNODB);
		return 0;
	}
	EOC_engine_act *eng_a = (EOC_engine_act *)eng;
	PDEBUG(DERR,"Engine request");
	eng_a->app_request(fr);
	return 0;
}

// -------------- Span configuration profiles requests ----------------//

int EOC_main::
app_cprof(app_frame *fr)
{
	cprof_payload *p = (cprof_payload*)fr->payload_ptr();
	span_conf_profile_t *mconf = &p->conf;
	int len = strnlen(p->pname,SNMP_ADMIN_LEN+1);

	PDEBUG(DERR,"Start");

	conf_profile *prof;
	switch(fr->type()){
	case APP_GET:
		if( !len ){
			fr->negative(ERPARAM);
			return 0;
		}
		prof = (conf_profile *)conf_profs.find(p->pname,len);
		if( !prof ){ // No such profile
			fr->negative(ERPNEXIST);
			return 0;
		}
		p->conf = prof->conf;
		return 0;
	case APP_GET_NEXT:
		PDEBUG(DERR,"GET_NEXT");
		if( !len ){ // requested first entry 
			PDEBUG(DERR,"First entry");
			prof = (conf_profile *)conf_profs.first();
		}else{
			PDEBUG(DERR,"Next after %s",p->pname);
			prof = (conf_profile *)conf_profs.next(p->pname,len);
		}
		if( !prof ){
			PDEBUG(DERR,"prof = 0");
			fr->negative(ERPNEXIST);
			return 0;
		}
		PDEBUG(DERR,"Gen response");
		p->conf = prof->conf;
		strncpy(p->pname,prof->name,prof->nsize+1);
		return 0;
	case APP_SET:{
		// Requested profile exist?
		prof = (conf_profile*)conf_profs.find(p->pname,len);
		if( !prof ){ // Profile already exist - cannot create
			fr->negative(ERPNEXIST);
			PDEBUG(DERR,"Requested profile \"%s\" not exist",p->pname);
			return 0;
		}
		span_conf_profile_t *pconf = &prof->conf;
		// Changes
		cprof_changes *c = (cprof_changes *)fr->changelist_ptr();
		
		if( c->annex ){
			pconf->annex = mconf->annex;
		}
		if( c->wires ){
			pconf->wires = mconf->wires;
		}
		if( c->power ){
			pconf->power = mconf->power;
		}
		if( c->psd ){
			pconf->psd = mconf->psd;
		}
		if( c->clk ){
			pconf->clk = mconf->clk;
		}
		if( c->line_probe ){
			pconf->line_probe = mconf->line_probe;
		}
		if( c->remote_cfg ){
			pconf->remote_cfg = mconf->remote_cfg;
		}
		if( c->use_cur_down ){
			pconf->use_cur_down = mconf->use_cur_down;
		}
		if( c->use_worst_down ){
			pconf->use_worst_down = mconf->use_worst_down;
		}
		if( c->use_cur_up ){
			pconf->use_cur_up = mconf->use_cur_up;
		}
		if( c->use_worst_up ){
			pconf->use_worst_up = mconf->use_worst_up;
		}

		if( c->rate ){
			pconf->rate = mconf->rate;
		}
		if( c->cur_marg_down ){
			pconf->cur_marg_down = mconf->cur_marg_down;
		}
		if( c->worst_marg_down ){
			pconf->worst_marg_down = mconf->worst_marg_down;
		}
		if( c->cur_marg_up ){
			pconf->cur_marg_up = mconf->cur_marg_up;
		}
		if( c->worst_marg_up ){
			pconf->worst_marg_up = mconf->worst_marg_up;
		}
		PDEBUG(DERR,"Commit Changes");
		configure_channels();
		return 0;
	}
	}
}     

int EOC_main::
app_list_cprof(app_frame *fr)
{
	cprof_list_payload *p = (cprof_list_payload*)fr->payload_ptr();

	switch(fr->type()){
	case APP_GET:
	case APP_GET_NEXT:{
		conf_profile *prof;
		int len;
		PDEBUG(DERR,"start,pname[0][0]=%d",(int)p->pname[0][0]);
		if( (len=strnlen(p->pname[0],SNMP_ADMIN_LEN)) ){ 
			// if first name isn't zero
			PDEBUG(DERR,"requested prof=%s\n",p->pname[0]);
			prof = (conf_profile *)conf_profs.find(p->pname[0],len);
			if( !prof ){
				fr->negative(ERPNEXIST);
				return 0;
			}
			PDEBUG(DERR,"prof-name=%s",prof->name);
			prof = (conf_profile*)conf_profs.next(prof->name,prof->nsize);
		}else{
			PDEBUG(DERR,"prof-name=NULL");
			prof = (conf_profile *)conf_profs.first();
			if( !prof ){
				fr->negative(ERPNEXIST);
				return 0;
			}
		}
		if( prof )
			PDEBUG(DERR,"Fill response for %s",prof->name);
		int filled = 0;
		while( prof && filled<PROF_NAMES_NUM){
			int cp_len = (prof->nsize>SNMP_ADMIN_LEN) ? SNMP_ADMIN_LEN : prof->nsize+1;
			strncpy(p->pname[filled],prof->name,cp_len+1);
			PDEBUG(DERR,"add %s,cp_len=%d",prof->name,cp_len);
			filled++;
			prof = (conf_profile*)conf_profs.next(prof->name,prof->nsize);
		}
		p->filled = filled;
		p->last_msg = ( prof && (filled = PROF_NAMES_NUM) ) ? 0 : 1;


		for(int i=0;i<filled;i++)
			PDEBUG(DERR,"pname[%d]=%s",i,p->pname[i]);


		return 0;
	}
	}
	fr->negative(ERTYPE);
	return 0;
}


int EOC_main::
app_add_cprof(app_frame *fr)
{
	cprof_add_payload *p = (cprof_add_payload*)fr->payload_ptr();

	switch( fr->type() ){
	case APP_SET:
		break;
	default: // Only set operation
		fr->negative(ERTYPE);
		return 0;
	}

	// check that adding profile not exist already
	int len = strnlen(p->pname,SNMP_ADMIN_LEN+1);
	if( !len ){
		fr->negative(ERPARAM);
		return 0;
	}
	conf_profile *prof = (conf_profile*)conf_profs.find(p->pname,len);
	if( prof ){ // Profile already exist - cannot create
		fr->negative(ERPEXIST);
		return 0;
	}

	conf_profile *nprof = new conf_profile;
	memset(nprof,0,sizeof(conf_profile));
	if( !(nprof->name = strndup(p->pname,len) ) ){
		// Not enough memory
		fr->negative(ERNOMEM);
		return 0;
	}
	nprof->nsize = len;
	// Default configuration
	nprof->conf.annex = annex_a;
	nprof->conf.wires = twoWire;
	nprof->conf.power = noPower;
	nprof->conf.clk = localClk;
	nprof->conf.line_probe = disable;
	nprof->conf.rate = 192;
	nprof->conf.use_cur_down = 0;
	nprof->conf.use_worst_down = 0;
	nprof->conf.use_cur_up = 0;
	nprof->conf.use_worst_up = 0;
	nprof->conf.cur_marg_down = 0;
	nprof->conf.worst_marg_down = 0;
	nprof->conf.cur_marg_up = 0;
	nprof->conf.worst_marg_up = 0;
	conf_profs.add(nprof);
	conf_profs.sort();
	return 0;
}

int EOC_main::
app_del_cprof(app_frame *fr)
{
	cprof_del_payload *p = (cprof_del_payload*)fr->payload_ptr();
	switch( fr->type() ){
	case APP_SET:
		break;
	default: // Only set operation
		fr->negative(ERTYPE);
		return 0;
	}

	// check that adding profile not exist already
	int len = strnlen(p->pname,SNMP_ADMIN_LEN+1);
	if( !len ){
		PDEBUG(DERR,"Profile name zero len");
		fr->negative(ERPNEXIST);
		return 0;
	}

	conf_profile *prof = (conf_profile*)conf_profs.find(p->pname,len);
	if( !prof ){ // Profile not exist so cannot delete
		PDEBUG(DERR,"Profile %s not exist",prof->name);
		fr->negative(ERPNEXIST);
		return 0;
	}

	// Check that no channel is associated with this profile
	channel_elem *el = (channel_elem *)channels.first();
	while( el ){
		if( el->eng->get_type() == master ){			
			const char *cprof = ((EOC_engine_act*)el->eng)->config()->cprof();
			if( !strncmp(prof->name,cprof,SNMP_ADMIN_LEN) ){
				// Cannot delete profile - associated with this channel
				PDEBUG(DERR,"Profile %s is busy",prof->name);
				fr->negative(ERPBUSY);
				return 0;
			}
		}
		el = (channel_elem *)channels.next(el->name,el->nsize);
	}
	conf_profs.del(prof->name,prof->nsize);	
	return 0;
}

int EOC_main::
app_add_chan(app_frame *fr)
{
	int len = strnlen(fr->chan_name(),SPAN_NAME_LEN);
	char *name = strndup(fr->chan_name(),len);
	chan_add_payload *p = (chan_add_payload *)fr->payload_ptr();

	switch( fr->type() ){
	case APP_SET:
		break;
	default: // Only set operation
		fr->negative(ERTYPE);
		return 0;
	}

	// check that requested channel exist
	channel_elem *el = (channel_elem *)
		channels.find((char*)fr->chan_name(),strnlen(fr->chan_name(),SPAN_NAME_LEN));
	if( el ){ // Channel already exist
		fr->negative(ERCHEXIST);
		return 0;
	}

	if( !p->master ){
		if( add_slave(name) ){
			fr->negative(ERNODEV);
			return 0;
		}
	}else{
		hash_elem *el = conf_profs.first();
		if( !el ){
			// No configuration profiles!
			fr->negative(ERPNEXIST);
			return 0;
		}
		char *cprof = strndup(el->name,SNMP_ADMIN_LEN+1);
		if( !cprof ){
			syslog(LOG_ERR,"Not enought memory");
			PDEBUG(DERR,"Not enought memory");
			fr->negative(ERNOMEM);
			return 0;
		}
		if( add_master(name,cprof,NULL,0/*repeaters*/,tick_per_min,1/*can apply*/) ){
			syslog(LOG_ERR,"(%s): cannot add channel \"%s\" - no such device",
				   config_file,name);
			PDEBUG(DERR,"(%s): cannot add channel \"%s\" - no such device",
				   config_file,name);
			fr->negative(ERNODEV);
			return 0;
		}
	}
	configure_channels();
	return 0;
}

int EOC_main::
app_del_chan(app_frame *fr)
{
	// check that requested channel exist
	channel_elem *el = (channel_elem *)
		channels.find((char*)fr->chan_name(),strnlen(fr->chan_name(),SPAN_NAME_LEN));

	switch( fr->type() ){
	case APP_SET:
		break;
	default: // Only set operation
		fr->negative(ERTYPE);
		return 0;
	}

	if( !el ){ // Nothing to delete
		fr->negative(ERCHNEXIST);
		return 0;
	}
	channels.del(el->name,el->nsize);
	configure_channels();
	return 0;
}

int EOC_main::
app_chng_chan(app_frame *fr)
{
	u8 new_changes = 0;
	int ret = 0;
	switch( fr->type() ){
	case APP_SET:
		break;
	default: // Only set operation
		fr->negative(ERTYPE);
		return 0;
	}

	PDEBUG(DERR,"Start");

	// check that requested channel exist
	channel_elem *el;
	el = (channel_elem *)
		channels.find((char*)fr->chan_name(),strnlen(fr->chan_name(),SPAN_NAME_LEN));
	if( !el ){ // Nothing to change
		PDEBUG(DERR,"No such channel %s",fr->chan_name());
		fr->negative(ERCHNEXIST);
		return 0;
	}
	chan_chng_payload *p;
	p = (chan_chng_payload *)fr->payload_ptr();

	if( p->master_ch ){
		if( el->eng->get_type() == master && !p->master ){
			char *name = strndup(el->name,el->nsize);
			if( add_slave(name) ){
				fr->negative(ERNODEV);
				return 0;
			}
			configure_channels();
			return 0;
		}else if( el->eng->get_type() == slave && p->master ){
			char *name = strndup(el->name,el->nsize);
			hash_elem *prof = conf_profs.first();
			if( !prof ){
				// No configuration profiles!
				PDEBUG(DERR,"No configuration profiles");
				fr->negative(ERNOPROF);
				return 0;
			}
			char *cprof = strndup(prof->name,SNMP_ADMIN_LEN+1);
			if( !cprof ){
				syslog(LOG_ERR,"Not enought memory");
				PDEBUG(DERR,"Not enought memory");
				fr->negative(ERNOMEM);
				return 0;
			}
			if( add_master(name,cprof,NULL,0/*repeaters*/,tick_per_min,1/*can apply*/) ){
				syslog(LOG_ERR,"(%s): cannot add channel \"%s\" - no such device",
					   config_file,name);
				PDEBUG(DERR,"(%s): cannot add channel \"%s\" - no such device",
					   config_file,name);
				fr->negative(ERNODEV);
				return 0;
			}
		}
		el = (channel_elem *)
			channels.find((char*)fr->chan_name(),strnlen(fr->chan_name(),SPAN_NAME_LEN));
		if( !el ){ // Nothing to change
			PDEBUG(DERR,"No such channel %s",fr->chan_name());
			fr->negative(ERCHNEXIST);
			return 0;
		}
	}

	PDEBUG(DERR,"Change configuration");
	EOC_config *cfg;
	cfg = ((EOC_engine_act*)el->eng)->config();
	PDEBUG(DERR,"Set repeaters num");
	if( p->rep_num_ch ){
		new_changes++;
		if( cfg->repeaters(p->rep_num) )
			ret = -1;
	}
	PDEBUG(DERR,"Set conf profle");
	if( p->cprof_ch ){
		new_changes++;
		PDEBUG(DERR,"Cprof=%s",p->cprof);
		if( !conf_profs.find(p->cprof,strnlen(p->cprof,SNMP_ADMIN_LEN)) ){
			syslog(LOG_ERR,"Wrong configuration profile name %s",p->cprof);
			PDEBUG(DERR,"No such profile=%s",p->cprof);
			ret = -1;
		}else{
			char *cprof = strndup(p->cprof,SNMP_ADMIN_LEN);
			cfg->cprof(cprof);
		}
	}
	PDEBUG(DERR,"Set can apply");
	if( p->apply_conf_ch ){
		new_changes++;
		cfg->can_apply(p->apply_conf);
	}

	PDEBUG(DERR,"Configure channels");
	if( new_changes ){
		configure_channels();
	}

	if( ret ){
		fr->negative(ERPARAM);
	}
	return 0;
}

int EOC_main::
app_endpalarm_prof(app_frame *fr)
{
	/*
	  endp_alarm_prof_payload *p = (endp_alarm_prof_payload*)fr->payload_ptr();
	  int len = strnlen(p->ProfileName,SNMP_ADMIN_LEN+1);
	  if( !len )
	  alarm_profile *prof;
	  switch(fr->type()){
	  case app_frame::GET:
	  prof = (alarm_profile *)alarm_profs.find(p->ProfileName,len);
	  if( !prof ){ // No such profile
	  fr->negative();
	  return 0;
	  }
	  fr->response();
	  p->alarm = prof->alarm;
	  return 0;
	  case app_frame::GET_NEXT:
	  if( !len ){ // requested first entry 
	  prof = (alarm_profile *)alarm_profs.first();
	  }else{
	  prof = (alarm_profile *)alarm_profs.next(p->ProfileName,len);
	  }
	  if( !prof ){
	  fr->negative();
	  return 0;
	  }
	  fr->response();
	  p->alarm = prof->alarm;
	  return 0;
	  case app_frame::SET:
	  return 0;
	  /*	1. ���Ü��ל��ݜ��М������ ���՜��������� ���ۜ��� ������֜��� ���������ޜ��� ���ߜ������ޜ�����؜��ۜ���
	  2. ����������ۜ��� ���ݜ��՜������ ���� ���ݜ��� ���������ޜ��؜��� ���ߜ��ޜ��ܜ��՜�����ڜ��� ������ޜ��ל��Ԝ��М������ ���؜��ܜ��� ���ߜ������ޜ�����؜��ۜ��� - ������ќ������ޜ���
	  3. ����������ۜ��� ���՜��������� - ���Ҝ��ݜ��՜��������� ���؜��ל��ܜ��՜��ݜ��՜��ݜ��؜���
	  4. ����������� ���؜��ݜ�����՜���������՜��ٜ������ ���؜��ܜ��՜��������؜��� ���������ޜ��� ���ߜ������ޜ�����؜��ۜ��� ���ߜ��՜������՜��ݜ��М������������ޜ��؜������
	  }
	*/
 err_exit:
	fr->negative();
	return 0;    
}

