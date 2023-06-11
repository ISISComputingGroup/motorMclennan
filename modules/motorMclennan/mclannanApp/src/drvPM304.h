/* File: drvPM304.h             */
/* Version: 2.0                 */
/* Date Last Modified: 09-29-99 */


/* Device Driver Support definitions for motor */
/*
 *      Original Author: Mark Rivers
 *      Current Author: Mark Rivers
 *      Date: 11/20/98
 *
 * Modification Log:
 * -----------------
 * .01  11/20/98  mlr  initialized from drvMM4000.h
 * .02  09/29/99  mlr  re-wrote for new version of motor software (V4)
 * .03  02/11/03  mlr  Added support for PM600 model
 */

#ifndef	INCdrvPM304h
#define	INCdrvPM304h 1

#include "motordrvCom.h"
#include "asynOctetSyncIO.h"

/* PM304 default profile. */

#define PM304_NUM_CARDS           4
#define PM304_MAX_CHANNELS        10

#define MODEL_PM304 0
#define MODEL_PM600 1

struct PM304controller
{
    asynUser *pasynUser;    /* asyn */
    int n_axes;             /* Number of axes on this controller */
    int model;              /* Model = MODEL_PM304 or MODEL_PM600 */
    int use_encoder[PM304_MAX_CHANNELS];  /* Does axis have an encoder? */
	int home_mode[PM304_MAX_CHANNELS]; /* The combined home modes for all axes */
    char port[80];          /* Asyn port name */
	int reset_before_move;  /* Reset the controller before any move command */
	int creep_speeds[PM304_MAX_CHANNELS]; /* Creep speed for each axis */
    char current_op[PM304_MAX_CHANNELS][60]; /* PM600: current operation as per CO command */
    int control_mode[PM304_MAX_CHANNELS];  /* PM600: datum mode as per CM command */
    char datum_mode[PM304_MAX_CHANNELS][9];  /* PM600: datum mode as per DM command */
    char abort_mode[PM304_MAX_CHANNELS][9];  /* PM600: abort mode as per AM command */
    int velo[PM304_MAX_CHANNELS]; /* last set velocity, used in homing to set creep speed */
    int datum[PM304_MAX_CHANNELS]; /* state of datum signal */
};

RTN_STATUS PM304Setup(int, int);
RTN_STATUS PM304Config(int, const char *, int, int, int);

enum HomeModes {
   HOME_MODE_BUILTIN=0, HOME_MODE_CONST_VELOCITY_MOVE, HOME_MODE_REVERSE_HOME_AND_ZERO, 
   HOME_MODE_CONST_VELOCITY_MOVE_AND_ZERO, HOME_MODE_FORWARD_HOME_AND_ZERO, 
   HOME_MODE_FORWARD_LIMIT_REVERSE_HOME_AND_ZERO, HOME_MODE_REVERSE_LIMIT_FORWARD_HOME_AND_ZERO 
};

static const char* home_mode_name[] = {
    "HOME_MODE_BUILTIN", "HOME_MODE_CONST_VELOCITY_MOVE", "HOME_MODE_REVERSE_HOME_AND_ZERO",
    "HOME_MODE_CONST_VELOCITY_MOVE_AND_ZERO", "HOME_MODE_FORWARD_HOME_AND_ZERO",
    "HOME_MODE_FORWARD_LIMIT_REVERSE_HOME_AND_ZERO", "HOME_MODE_REVERSE_LIMIT_FORWARD_HOME_AND_ZERO"
};

#endif	/* INCdrvPM304h */
