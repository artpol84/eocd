#ifndef EOC_MAIN_H
#define EOC_MAIN_H

#include <utils/hash_table.h>
#include <snmp/snmp-generic.h>
#include <app-if/app_frame.h>
#include <app-if/app_comm_srv.h>


// TODO: consult for OS about this values
#define MAX_IF_NAME_LEN 256
#define MAX_FNAME 256

#define TICK_PER_MINUTE_MAX 60
#define TICK_PER_MINUTE_DEF 6

#define OS_IF_PATH "/sys/class/net"



class EOC_main{
protected:
    char config_file[MAX_FNAME];
    hash_table conf_profs;
    hash_table alarm_profs;
    hash_table channels;
    app_comm_srv app_srv;
public:
    EOC_main(char *cfg) : conf_profs(SNMP_ADMIN_LEN), alarm_profs(SNMP_ADMIN_LEN),
		     channels(MAX_IF_NAME_LEN), app_srv("./","socket"){
	strncpy(config_file,cfg,MAX_FNAME);
	config_file[MAX_FNAME-1] = '\0';
	read_config();
	configure_channels();
    }
    ~EOC_main(){
    }
    // Read configuration file and initialise or change channels
    int read_config();
    // Write configuration to config file (when it changes from network)
//    int write_config();
    // Add (initialise) or change slave channel with name "ch_name"
    int add_slave(char *ch_name);
    // Add (initialise) or change master channel with name "ch_name"
    int add_master(char *ch_name, char *conf,char *alarm,int reps,int tick);
    //
    int configure_channels();    
    // process channels
    int poll_channels();


    void app_listen();
    int app_request(app_frame *fr);
    int app_chann_request(app_frame *fr);
    int app_spanconf_prof(app_frame *fr);
    int app_endpalarm_prof(app_frame *fr);

};


#endif