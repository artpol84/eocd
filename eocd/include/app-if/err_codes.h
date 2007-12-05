#ifndef ERROR_CODES_H
#define ERROR_CODES_H

enum err_codes {
DEFERR=1,   // Default err code
ERTYPE,     // Error in request type 
ERCHEXIST,  // Channel already exist
ERCHNEXIST, // Channel not existb
ERNOPROF,   // Profile not exist
ERNOMEM,    // Not enough memory
ERNODEV,    // No such device (dslX)
ERPARAM,    // Request parameters in wrong
ERPNEXIST,  // Conf profile not exist
ERPEXIST,   // Conf profile already exist
ERPBUSY,    // Conf profile is in use
ERNODB,     // Channel dont maintain SHDSL DB
ERUNEXP,    // Unexpected error
ERNOELEM    // Requested unit not exist
};

#endif
