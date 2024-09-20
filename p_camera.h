#ifndef __P_CAMERA_H__
#define __P_CAMERA_H__

#include "doomdef.h"
#include "p_local.h"

typedef struct camera_s
{
	// Info for drawing: position.
	fixed_t x, y, z;

	angle_t angle; // orientation
	angle_t aiming; // up/down (future)

	struct subsector_s *subsector;
	fixed_t floorz, ceilingz;

	// Momentums used to update position
	fixed_t momx, momy, momz;
	fixed_t distFromPlayer;
} camera_t;

#define CAM_HEIGHT (20<<FRACBITS)
#define CAM_RADIUS (20<<FRACBITS)
#define CAM_DIST (192<<FRACBITS)

extern mobj_t *camBossMobj;
extern camera_t camera;
void P_MoveChaseCamera(player_t *player, camera_t *thiscam);
#endif
