/* File: devPM304.cc                    */
/* Version: 2.00                        */
/* Date Last Modified: 09/29/99         */

/* Device Support Routines for motor */
/*
 *      Original Author: Mark Rivers
 *      Date: 11/20/98
 *
 * Modification Log:
 * -----------------
 * .00  11-20-99        mlr     initialized from devMM4000.c
 * .01  09-29-99        mlr     Version 2.0, compatible with V4.04 of 
 *                              motorRecord
 * .02  10-26-99        mlr     Version 2.01, minor fixes for V4.0 of 
 *                              motorRecord
 * .03  02-11-03        mlr     Version 3.0, added support for PM600 model.
 *                              Added SD for decceleration.
 * .04  05/27/03    rls     R3.14 conversion.
 *      ...
 */


#define VERSION 3.00


#include    <string.h>
#include        "motorRecord.h"
#include        "motor.h"
#include        "motordevCom.h"
#include        "drvPM304.h"
#include    "epicsExport.h"

#define STATIC static

#define HOME_MODE_BUILTIN 0
#define HOME_MODE_CONST_VELOCITY_MOVE 1
#define HOME_MODE_REVERSE_HOME_AND_ZERO 2
#define HOME_MODE_CONST_VELOCITY_MOVE_AND_ZERO 3
#define HOME_MODE_FORWARD_HOME_AND_ZERO 4
#define HOME_MODE_FORWARD_LIMIT_REVERSE_HOME_AND_ZERO 5
#define HOME_MODE_REVERSE_LIMIT_FORWARD_HOME_AND_ZERO 6

extern struct driver_table PM304_access;

#define NINT(f) (long)((f)>0 ? (f)+0.5 : (f)-0.5)

/*----------------debugging-----------------*/
volatile int devPM304Debug = 0;
extern "C" {epicsExportAddress(int, devPM304Debug);}

static inline void Debug(int level, const char *format, ...) {
  #ifdef DEBUG
    if (level <= devPM304Debug) {
      va_list pVar;
      va_start(pVar, format);
      vprintf(format, pVar);
      va_end(pVar);
    }
  #endif
}


/* Debugging levels: 
 *      devPM304Debug >= 3  Print new part of command and command string so far
 *                          at the end of PM304_build_trans
 */


/* ----------------Create the dsets for devPM304----------------- */
STATIC struct driver_table *drvtabptr;
STATIC long PM304_init(int);
STATIC long PM304_init_record(void *);
STATIC long PM304_start_trans(struct motorRecord *);
STATIC RTN_STATUS PM304_build_trans(motor_cmnd, double *, struct motorRecord *);
STATIC RTN_STATUS PM304_end_trans(struct motorRecord *);

struct motor_dset devPM304 =
{
    {8, NULL, (DEVSUPFUN) PM304_init, (DEVSUPFUN) PM304_init_record, NULL},
    motor_update_values,
    PM304_start_trans,
    PM304_build_trans,
    PM304_end_trans
};

extern "C" {epicsExportAddress(dset,devPM304);}


/* --------------------------- program data --------------------- */
/* This table is used to define the command types */

static msg_types PM304_table[] = {
    MOTION,     /* MOVE_ABS */
    MOTION,     /* MOVE_REL */
    MOTION,     /* HOME_FOR */
    MOTION,     /* HOME_REV */
    IMMEDIATE,  /* LOAD_POS */
    IMMEDIATE,  /* SET_VEL_BASE */
    IMMEDIATE,  /* SET_VELOCITY */
    IMMEDIATE,  /* SET_ACCEL */
    IMMEDIATE,  /* GO */
    IMMEDIATE,  /* SET_ENC_RATIO */
    INFO,       /* GET_INFO */
    MOVE_TERM,  /* STOP_AXIS */
    VELOCITY,   /* JOG */
    IMMEDIATE,  /* SET_PGAIN */
    IMMEDIATE,  /* SET_IGAIN */
    IMMEDIATE,  /* SET_DGAIN */
    IMMEDIATE,  /* ENABLE_TORQUE */
    IMMEDIATE,  /* DISABL_TORQUE */
    IMMEDIATE,  /* PRIMITIVE */
    IMMEDIATE,  /* SET_HIGH_LIMIT */
    IMMEDIATE   /* SET_LOW_LIMIT */
};


static struct board_stat **PM304_cards;

/* --------------------------- program data --------------------- */


/* initialize device support for PM304 stepper motor */
STATIC long PM304_init(int after)
{
    long rtnval;

    if (!after)
    {
    drvtabptr = &PM304_access;
        (drvtabptr->init)();
    }

    rtnval = motor_init_com(after, *drvtabptr->cardcnt_ptr, drvtabptr, &PM304_cards);
    return(rtnval);
}


/* initialize a record instance */
STATIC long PM304_init_record(void *arg)
{
    struct motorRecord *mr = (struct motorRecord *) arg;
    long rtnval;

    rtnval = motor_init_record_com(mr, *drvtabptr->cardcnt_ptr, 
                                   drvtabptr, PM304_cards);
    return(rtnval);
}


/* start building a transaction */
STATIC long PM304_start_trans(struct motorRecord *mr)
{
    long rtnval;
    rtnval = motor_start_trans_com(mr, PM304_cards);
    return(rtnval);
}


/* end building a transaction */
STATIC RTN_STATUS PM304_end_trans(struct motorRecord *mr)
{
    RTN_STATUS rtnval;
    rtnval = motor_end_trans_com(mr, drvtabptr);
    return(rtnval);
    
}

/* request homing move */
STATIC void request_home(struct mess_node *motor_call, int model, int axis, int home_direction,
                         struct PM304controller* cntrl) {
    // hardware homes use creep speed. Max creep speed is 800, use velo if under this. Previous creep speed
    // is restored next velocity change to cntrl->creep_speeds so previous value is used for any creep steps
    // at end of regular motion
    const int MAX_CREEP_SPEED = 800;
    int velo = cntrl->velo[axis-1];
    int creep_speed = (velo>MAX_CREEP_SPEED) ? MAX_CREEP_SPEED : velo;
    int home_mode = cntrl->home_mode[axis-1];
    char* datum_mode = cntrl->datum_mode[axis-1];
    char buff[32];
    buff[0] = '\0';
    if (model == MODEL_PM304){
        sprintf(buff, "%dSC%d;", axis, creep_speed);
        strcat(motor_call->message, buff);
        sprintf(buff, "%dIX%d;", axis, home_direction);
    } else {
        // datum mode: [0] = encoder index input polarity, [3] automatic direction search, [4] automatic opposite limit search
        if ( home_mode==HOME_MODE_BUILTIN || home_mode==HOME_MODE_REVERSE_HOME_AND_ZERO || home_mode==HOME_MODE_FORWARD_HOME_AND_ZERO ||
             home_mode==HOME_MODE_FORWARD_LIMIT_REVERSE_HOME_AND_ZERO || home_mode==HOME_MODE_REVERSE_LIMIT_FORWARD_HOME_AND_ZERO ) {
            datum_mode[1] = '0'; // set datum mode to capture only once on HD command
            if ( home_mode==HOME_MODE_BUILTIN ) {
                datum_mode[3] = '0'; // set datum mode to no automatic direction search, will use passed home_direction
                datum_mode[4] = '0'; // no auto opposite limit search
            } else if ( home_mode==HOME_MODE_REVERSE_HOME_AND_ZERO||home_mode==HOME_MODE_FORWARD_LIMIT_REVERSE_HOME_AND_ZERO ) {
                sprintf(buff, "%dSH0;", axis); // define home position as 0
                datum_mode[2] = '1'; // set datum mode to apply home position from SH
                datum_mode[3] = '0'; // set datum mode to no automatic direction search
                datum_mode[4] = '0'; // disablke auto limit search
                home_direction = -1;
            } else if ( home_mode==HOME_MODE_FORWARD_HOME_AND_ZERO||home_mode==HOME_MODE_REVERSE_LIMIT_FORWARD_HOME_AND_ZERO ) {
                sprintf(buff, "%dSH0;", axis); // define home position as 0
                datum_mode[2] = '1'; // set datum mode to apply home position from SH
                datum_mode[3] = '0'; // set datum mode to no automatic direction search
                datum_mode[4] = '0'; // disablke auto limit search
                home_direction = 1;
            }
            if (home_mode==HOME_MODE_FORWARD_LIMIT_REVERSE_HOME_AND_ZERO||home_mode==HOME_MODE_REVERSE_LIMIT_FORWARD_HOME_AND_ZERO){
                datum_mode[4] = '1'; // enable auto limit search
            } 
            strcat(motor_call->message, buff);
            sprintf(buff, "%dCD;", axis);
            strcat(motor_call->message, buff);
            sprintf(buff, "%dDM%s;", axis, datum_mode);
            strcat(motor_call->message, buff);
            sprintf(buff, "%dSC%d;", axis, creep_speed);
            strcat(motor_call->message, buff);
            sprintf(buff, "%dHD%d;", axis, home_direction);
            // home position may or may not be automatically applied at end of HD depending on datum mode setting
        } else {
            // For all other home modes let SNL take care of everything, so do not reset creep speed. See homing.st
        }
    }
    strcat(motor_call->message, buff);
}

/* add a part to the transaction */
STATIC RTN_STATUS PM304_build_trans(motor_cmnd command, double *parms, struct motorRecord *mr)
{
    struct motor_trans *trans = (struct motor_trans *) mr->dpvt;
    struct mess_node *motor_call;
    struct controller *brdptr;
    struct PM304controller *cntrl;
    const double MAX_SERVO_PID = 32767.0;
    char buff[30];
    int axis, card;
    RTN_STATUS rtnval;
    double dval;
    long ival;

    rtnval = OK;
    buff[0] = '\0';
    /* Protect against NULL pointer with WRTITE_MSG(GO/STOP_AXIS/GET_INFO, NULL). */
    dval = (parms == NULL) ? 0.0 : *parms;
    ival = NINT(dval);

    motor_call = &(trans->motor_call);
    card = motor_call->card;
    axis = motor_call->signal + 1;
    brdptr = (*trans->tabptr->card_array)[card];
    if (brdptr == NULL)
        return(rtnval = ERROR);

    cntrl = (struct PM304controller *) brdptr->DevicePrivate;

    if (PM304_table[command] > motor_call->type)
        motor_call->type = PM304_table[command];

    if (trans->state != BUILD_STATE)
        return(rtnval = ERROR);

    if (command == PRIMITIVE && mr->init != NULL && strlen(mr->init) != 0)
    {
        strcat(motor_call->message, mr->init);
        strcat(motor_call->message, "\r");
    }

    switch (command)
    {
        case MOVE_ABS:
        case MOVE_REL:
        case HOME_FOR:
        case HOME_REV:
        case JOG:
            if (strlen(mr->prem) != 0)
            {
                strcat(motor_call->message, mr->prem);
                strcat(motor_call->message, ";");
            }
            if (strlen(mr->post) != 0)
                motor_call->postmsgptr = (char *) &mr->post;
            /* Send a reset command before any move */
            if (cntrl->reset_before_move==1) {
                sprintf(buff, "%dRS;", axis);
                strcat(motor_call->message, buff);
                buff[0] = '\0';
			}
            break;
        default:
            break;
    }
    
    switch (command)
    {
    case MOVE_ABS:
        sprintf(buff, "%dMA%ld;", axis, ival);
        break;
    case MOVE_REL:
        sprintf(buff, "%dMR%ld;", axis, ival);
        break;
    case HOME_REV:
        request_home(motor_call, cntrl->model, axis, -1, cntrl);
        break;
    case HOME_FOR:
        request_home(motor_call, cntrl->model, axis, 1, cntrl);
        break;
    case LOAD_POS:
        sprintf(buff, "%dCP%ld;", axis, ival);
        if (cntrl->use_encoder[axis-1] || cntrl->model != MODEL_PM304){
           strcat(motor_call->message, buff);
           sprintf(buff, "%dAP%ld;", axis, ival);
        }
        break;
    case SET_VEL_BASE:
        break;          /* PM304 does not use base velocity */
    case SET_VELOCITY:
        if (cntrl->creep_speeds[axis-1] != 0) {
            sprintf(buff, "%dSC%d;", axis, cntrl->creep_speeds[axis-1]);
            strcat(motor_call->message, buff);
        }
        sprintf(buff, "%dSV%ld;", axis, ival);
        cntrl->velo[axis-1] = ival;
        break;
    case SET_ACCEL:
        sprintf(buff, "%dSA%ld;", axis, ival);
        strcat(motor_call->message, buff);
        sprintf(buff, "%dSD%ld;", axis, ival);
        break;
    case GO:
        /*
         * The PM304 starts moving immediately on move commands, GO command
         * does nothing
         */
        break;
    case SET_ENC_RATIO:
        /*
         * The PM304 does not have the concept of encoder ratio, ignore this
         * command
         */
        break;
    case GET_INFO:
        /* These commands are not actually done by sending a message, but
           rather they will indirectly cause the driver to read the status
           of all motors */
        break;
    case STOP_AXIS:
	    /* Send a stop command as our first port of call */
        sprintf(buff, "%dST;", axis);
        strcat(motor_call->message, buff);
        /* Send a second stop preceded by a reset command as a contingency in case the first stop is impeded by a tracking abort */
        sprintf(buff, "%dRS;", axis);
        strcat(motor_call->message, buff);
        sprintf(buff, "%dST;", axis);
        break;
    case JOG:
        if (cntrl->model == MODEL_PM304) {
            sprintf(buff, "%dSV%ld;", axis, ival);
            strcat(motor_call->message, buff);
            if (ival > 0) {
                /* This is a positive move in PM304 coordinates */
                sprintf(buff, "%dCV1;", axis);
            } else {
                /* This is a negative move in PM304 coordinates */
                sprintf(buff, "%dCV-1;", axis);
            }
        } else {
            sprintf(buff, "%dCV%ld;", axis, ival);
        }
        break;
    case SET_PGAIN:
        // for servo mode we need NINT(dval * MAX_SERVO_PID)
        // in stepper mode this is used for end of move position correction
        if (cntrl->model == MODEL_PM304) {
            sprintf(buff, "%dKP%ld;", axis, ival);
        } else if (cntrl->control_mode[axis-1] == 1) {
            sprintf(buff, "%dKP%ld;", axis, NINT(dval * MAX_SERVO_PID)); // servo motor, proportional gain servo coefficient
        } else {
            sprintf(buff, "%dKP%ld;", axis, NINT(dval * 100.0)); // stepper motor, a correction gain percentage
        }
        break;

    case SET_IGAIN:
        if (cntrl->model == MODEL_PM304) {
            sprintf(buff, "%dKS%ld;", axis, ival);
        } else if (cntrl->control_mode[axis-1] == 1) {
            sprintf(buff, "%dKS%ld;", axis, NINT(dval * MAX_SERVO_PID)); // only valid in servo mode on PM600
        }
        break;

    case SET_DGAIN:
        if (cntrl->model == MODEL_PM304) {
            sprintf(buff, "%dKV%ld;", axis, ival);
        } else if (cntrl->control_mode[axis-1] == 1) {
            sprintf(buff, "%dKV%ld;", axis, NINT(dval * MAX_SERVO_PID)); // only valid in servo mode on PM600
        }
        break;

    case ENABLE_TORQUE:
        sprintf(buff, "%dRS;", axis);
        break;

    case DISABL_TORQUE:
        sprintf(buff, "%dAB;", axis);
        break;

   /* limits may or may not be enforced depending on last SL command. 
      Hardware will not let you set a low limit >= high limit so order of setting
      may be important, but might get round that by just sending twice i.e set_low, set_high, set_low, set_high
      Need to look more closely at motor record, it may already cover this if it gets a correct readback of existing limits 
      which it does not currently get. 
            sprintf(buff, "%dUL%ld;", axis, ival); // high limit 
            sprintf(buff, "%dLL%ld;", axis, ival); // low limit
    */
    case SET_HIGH_LIMIT:
        trans->state = IDLE_STATE;  /* No command sent to the controller. */
        /* The PM304 internal soft limits are very difficult to retrieve, not
         * implemented yet */
        break;

    case SET_LOW_LIMIT:
        trans->state = IDLE_STATE;  /* No command sent to the controller. */
        /* The PM304 internal soft limits are very difficult to retrieve, not
         * implemented yet */
        break;

    default:
        rtnval = ERROR;
    }
    strcat(motor_call->message, buff);
    Debug(3, "PM304_build_trans: buff=%s, motor_call->message=%s\n", buff, motor_call->message);

    return (rtnval);
}
